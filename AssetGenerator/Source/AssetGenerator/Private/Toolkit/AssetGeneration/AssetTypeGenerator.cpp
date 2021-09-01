#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "FileHelpers.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"

DEFINE_LOG_CATEGORY(LogAssetGenerator)

FString UAssetTypeGenerator::GetAdditionalDumpFilePath(const FString& Postfix, const FString& Extension) const {
	FString Filename = FPackageName::GetShortName(GetPackageName());
		
	if (Postfix.Len() > 0) {
		Filename.AppendChar('-').Append(Postfix);
	}
	if (Extension.Len() > 0) {
		if (Extension[0] != '.')
			Filename.AppendChar('.');
		Filename.Append(Extension);
	}
	return FPaths::Combine(PackageBaseDirectory, Filename);
}

void UAssetTypeGenerator::PopulateReferencedObjectsDependencies(TArray<FAssetDependency>& OutDependencies) const {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));
	const TArray<TSharedPtr<FJsonValue>> ReferencedObjects = AssetObjectProperties->GetArrayField(TEXT("$ReferencedObjects"));
		
	TArray<FString> OutReferencedPackages;
	GetObjectSerializer()->CollectReferencedPackages(ReferencedObjects, OutReferencedPackages);

	for (const FString& DependencyPackageName : OutReferencedPackages) {
		OutDependencies.Add(FAssetDependency{*DependencyPackageName, EAssetGenerationStage::CDO_FINALIZATION});
	}
}

UAssetTypeGenerator::UAssetTypeGenerator() {
	this->PropertySerializer = CreateDefaultSubobject<UPropertySerializer>(TEXT("PropertySerializer"));
	this->ObjectSerializer = CreateDefaultSubobject<UObjectHierarchySerializer>(TEXT("ObjectSerializer"));
	this->ObjectSerializer->SetPropertySerializer(PropertySerializer);

	this->CurrentStage = EAssetGenerationStage::CONSTRUCTION;
	this->AssetPackage = NULL;
	this->AssetObject = NULL;
	this->bUsingExistingPackage = false;
	this->bAssetChanged = false;
	this->bHasAssetEverBeenChanged = false;
	this->bIsGeneratingPublicProject = false;
}

void UAssetTypeGenerator::InitializeInternal(const FString& InPackageBaseDirectory, const FName InPackageName, const TSharedPtr<FJsonObject> RootFileObject) {
	this->PackageBaseDirectory = InPackageBaseDirectory;
	this->PackageName = FName(*RootFileObject->GetStringField(TEXT("AssetPackage")));
	this->AssetName = FName(*RootFileObject->GetStringField(TEXT("AssetName")));
	checkf(this->PackageName == InPackageName, TEXT("InitializeInternal called with inconsistent package name. Externally provided name was '%s', but internal dump package name is '%s'"),
		*InPackageName.ToString(), *this->PackageName.ToString());

	const TArray<TSharedPtr<FJsonValue>> ObjectHierarchy = RootFileObject->GetArrayField(TEXT("ObjectHierarchy"));
	this->ObjectSerializer->InitializeForDeserialization(ObjectHierarchy);
	this->AssetData = RootFileObject->GetObjectField(TEXT("AssetSerializedData"));
	PostInitializeAssetGenerator();
}

void UAssetTypeGenerator::SetPackageAndAsset(UPackage* NewPackage, UObject* NewAsset) {
	checkf(CurrentStage == EAssetGenerationStage::CONSTRUCTION, TEXT("SetPackageAndAsset can only be called during CONSTRUCTION"));
	checkf(AssetPackage == NULL, TEXT("SetPackageAndAsset can only be called once during CreateAssetPackage"));
	
	check(NewPackage->GetFName() == GetPackageName());
	check(NewAsset->GetFName() == GetAssetName());
	check(NewAsset->GetOuter() == NewPackage);
	
	this->AssetPackage = NewPackage;
	this->AssetObject = NewAsset;

	ObjectSerializer->SetPackageForDeserialization(AssetPackage);
	ObjectSerializer->SetObjectMark(AssetObject, TEXT("$AssetObject$"));
}


EAssetGenerationStage UAssetTypeGenerator::AdvanceGenerationState() {
	if (CurrentStage == EAssetGenerationStage::CONSTRUCTION) {
		UPackage* ExistingPackage = FindPackage(NULL, *PackageName.ToString());
		if (!ExistingPackage) {
			ExistingPackage = LoadPackage(NULL, *PackageName.ToString(), LOAD_Quiet);
		}
		
		if (ExistingPackage == NULL) {
			//Make new package if we don't have existing one, make sure asset object is also allocated
			CreateAssetPackage();
			checkf(AssetPackage, TEXT("CreateAssetPackage should call SetPackageAndAsset"));

			//Make sure to mark package as changed because it has never been saved to disk before
			MarkAssetChanged();
		} else {
			//Package already exist, reuse it while making sure out asset is contained within
			UObject* AssetObject = FindObject<UObject>(ExistingPackage, *AssetName.ToString());

			//We need to verify package exists and provide meaningful error message, so user knows what is wrong
			checkf(AssetObject, TEXT("Existing package %s does not contain an asset named %s, requested by asset dump"), *PackageName.ToString(), *AssetName.ToString());
			SetPackageAndAsset(ExistingPackage, AssetObject);
			
			//Notify generator we are reusing existing package, so it can do additional cleanup and settings
			this->bUsingExistingPackage = true;
			OnExistingPackageLoaded();
		}

		//Advance asset generation state, next state after construction is data population
		this->CurrentStage = EAssetGenerationStage::DATA_POPULATION;
		
	} else if (CurrentStage == EAssetGenerationStage::DATA_POPULATION) {
		this->PopulateAssetWithData();
		this->CurrentStage = EAssetGenerationStage::CDO_FINALIZATION;

	} else if (CurrentStage == EAssetGenerationStage::CDO_FINALIZATION) {
		this->FinalizeAssetCDO();
		this->CurrentStage = EAssetGenerationStage::FINISHED;
	}

	//Force package to be saved to disk if it has been marked dirty at some point before
	if (bAssetChanged) {
		FEditorFileUtils::PromptForCheckoutAndSave({AssetPackage}, false, false);
		this->bAssetChanged = false;
		this->bHasAssetEverBeenChanged = true;
	}
	return CurrentStage;
}

FString UAssetTypeGenerator::GetAssetFilePath(const FString& RootDirectory, FName PackageName) {
	const FString ShortPackageName = FPackageName::GetShortName(PackageName);
	FString PackagePath = FPackageName::GetLongPackagePath(PackageName.ToString());
	PackagePath.RemoveAt(0);

	const FString PackageBaseDirectory = FPaths::Combine(RootDirectory, PackagePath);
	
	const FString AssetDumpFilename = FPaths::SetExtension(ShortPackageName, TEXT("json"));
	return FPaths::Combine(PackageBaseDirectory, AssetDumpFilename);
}

UAssetTypeGenerator* UAssetTypeGenerator::InitializeFromFile(const FString& RootDirectory, const FName PackageName) {
	const FString AssetDumpFilePath = GetAssetFilePath(RootDirectory, PackageName);
	const FString PackageBaseDirectory = FPaths::GetPath(AssetDumpFilePath);

	//Return early if dump file is not found for this asset
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssetDumpFilePath)) {
		return NULL;
	}

	FString DumpFileStringContents;
	if (!FFileHelper::LoadFileToString(DumpFileStringContents, *AssetDumpFilePath)) {
		UE_LOG(LogAssetGenerator, Error, TEXT("Failed to load asset dump file %s"), *AssetDumpFilePath);
		return NULL;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DumpFileStringContents);
	TSharedPtr<FJsonObject> RootFileObject;
	if (!FJsonSerializer::Deserialize(Reader, RootFileObject)) {
		UE_LOG(LogAssetGenerator, Error, TEXT("Failed to parse asset dump file %s: invalid json"), *AssetDumpFilePath);
		return NULL;
	}

	const FName AssetClass = FName(*RootFileObject->GetStringField(TEXT("AssetClass")));
	UClass* AssetTypeGenerator = FindGeneratorForClass(AssetClass);
	if (AssetTypeGenerator == NULL) {
		UE_LOG(LogAssetGenerator, Error, TEXT("Asset generator not found for asset class '%s', loaded from %s"), *AssetClass.ToString(), *AssetDumpFilePath);
		return NULL;
	}

	UAssetTypeGenerator* NewGenerator = NewObject<UAssetTypeGenerator>(GetTransientPackage(), AssetTypeGenerator);
	NewGenerator->InitializeInternal(PackageBaseDirectory, PackageName, RootFileObject);
	return NewGenerator;
}

//Static class used to lazily populate serializer registry
class FAssetTypeGeneratorRegistry {
public:
	//Native classes should never get unloaded, so we can get away with using TSubclassOf without weak pointer
	TMap<FName, TWeakObjectPtr<UClass>> Generators;

	//Constructor that will automatically populate registry serializers
	FAssetTypeGeneratorRegistry();

	static const FAssetTypeGeneratorRegistry& Get() {
		static FAssetTypeGeneratorRegistry AssetTypeSerializerRegistry{};
		return AssetTypeSerializerRegistry;
	}
};

FAssetTypeGeneratorRegistry::FAssetTypeGeneratorRegistry() {
	TArray<UClass*> AssetGeneratorClasses;
	GetDerivedClasses(UAssetTypeGenerator::StaticClass(), AssetGeneratorClasses, true);

	//Iterate classes in memory to resolve serializers
	TArray<UAssetTypeGenerator*> OutFoundInitializers;
	for (UClass* Class : AssetGeneratorClasses) {
		//Skip classes that are marked as Abstract
		if (Class->HasAnyClassFlags(CLASS_Abstract)) {
			continue;
		}
		//Skip classes that are not marked as Native
		if (!Class->HasAnyClassFlags(CLASS_Native)) {
			continue;
		}
            
		//Check asset type in class default object
		UAssetTypeGenerator* ClassDefaultObject = CastChecked<UAssetTypeGenerator>(Class->GetDefaultObject());
		if (ClassDefaultObject->GetAssetClass() != NAME_None) {
			OutFoundInitializers.Add(ClassDefaultObject);
		}
	}

	//First register additional asset classes, so primary ones will overwrite them later
	for (UAssetTypeGenerator* Generator : OutFoundInitializers) {
		TArray<FName> OutExtraAssetClasses;
		Generator->GetAdditionallyHandledAssetClasses(OutExtraAssetClasses);
		for (const FName& AssetClass : OutExtraAssetClasses) {
			this->Generators.Add(AssetClass, Generator->GetClass());
		}
	}

	//Now setup primary asset classes and add serializers into array
	for (UAssetTypeGenerator* Generator : OutFoundInitializers) {
		const FName AssetClass = Generator->GetAssetClass();
		this->Generators.Add(AssetClass, Generator->GetClass());
	}
}

TSubclassOf<UAssetTypeGenerator> UAssetTypeGenerator::FindGeneratorForClass(const FName AssetClass) {
	const TWeakObjectPtr<UClass>* Class = FAssetTypeGeneratorRegistry::Get().Generators.Find(AssetClass);
	return Class ? (*Class).Get() : NULL;
}

TArray<TSubclassOf<UAssetTypeGenerator>> UAssetTypeGenerator::GetAllGenerators() {
	TArray<TSubclassOf<UAssetTypeGenerator>> ResultArray;

	for (const TPair<FName, TWeakObjectPtr<UClass>>& Pair : FAssetTypeGeneratorRegistry::Get().Generators) {
		if (UClass* ResultingValue = Pair.Value.Get(); ResultingValue) {
			ResultArray.AddUnique(ResultingValue);
		}
	}
	return ResultArray;
}
