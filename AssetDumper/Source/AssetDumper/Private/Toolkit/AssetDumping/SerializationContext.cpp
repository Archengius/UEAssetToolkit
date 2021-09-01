#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"

UObject* ResolveBlueprintClassAsset(UPackage* Package, const FAssetData& AssetData) {
	FString GeneratedClassExportedPath;
	if (!AssetData.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClassExportedPath)) {
		checkf(0, TEXT("GetBlueprintAsset called on non-blueprint asset"));
		return NULL;
	}
		
	//Make sure export path represents a valid path and convert it to pure object path
	FString GeneratedClassPath;
	check(FPackageName::ParseExportTextPath(GeneratedClassExportedPath, NULL, &GeneratedClassPath));
	const FString BlueprintClassObjectName = FPackageName::ObjectPathToObjectName(GeneratedClassPath);

	//Load UBlueprintGeneratedClass for provided object and make sure it has been loaded
	return FindObjectFast<UObject>(Package, *BlueprintClassObjectName);
}

UObject* ResolveGenericAsset(UPackage* Package, const FAssetData& AssetData) {
	return FindObjectFast<UObject>(Package, *AssetData.AssetName.ToString());
}

FSerializationContext::FSerializationContext(const FString& RootOutputDirectory, const FAssetData& AssetData, UObject* AssetObject) {
	this->AssetSerializedData = MakeShareable(new FJsonObject());
	this->PropertySerializer = NewObject<UPropertySerializer>();
	this->ObjectHierarchySerializer = NewObject<UObjectHierarchySerializer>();
	this->ObjectHierarchySerializer->SetPropertySerializer(PropertySerializer);

	this->PropertySerializer->AddToRoot();
	this->ObjectHierarchySerializer->AddToRoot();

	//Object hierarchy serializer will also root package object by referencing it
	this->AssetObject = AssetObject;
	this->Package = AssetObject->GetOutermost();
	this->ObjectHierarchySerializer->InitializeForSerialization(Package);
	this->AssetData = AssetData;

	this->RootOutputDirectory = RootOutputDirectory;
	this->PackageBaseDirectory = FPaths::Combine(RootOutputDirectory, AssetData.PackagePath.ToString());

	//Make sure package base directory exists
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*PackageBaseDirectory);

	//Set mark on the asset object so it can be referenced by other objects in hierarchy
	this->ObjectHierarchySerializer->SetObjectMark(AssetObject, TEXT("$AssetObject$"));
}

FSerializationContext::~FSerializationContext() {
	this->PropertySerializer->RemoveFromRoot();
	this->ObjectHierarchySerializer->RemoveFromRoot();

	this->PropertySerializer->MarkPendingKill();
	this->ObjectHierarchySerializer->MarkPendingKill();
}

UObject* FSerializationContext::GetAssetObjectFromPackage(UPackage* Package, const FAssetData& AssetData) {
	if (AssetData.TagsAndValues.Contains(FBlueprintTags::GeneratedClassPath)) {
		return ResolveBlueprintClassAsset(Package, AssetData);
	}
	
	//No GeneratedClassPath, it is an ordinary asset object
	return ResolveGenericAsset(Package, AssetData);
}

void FSerializationContext::Finalize() const {
	TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject());
	RootObject->SetStringField(TEXT("AssetClass"), AssetData.AssetClass.ToString());
	RootObject->SetStringField(TEXT("AssetPackage"), Package->GetName());
	RootObject->SetStringField(TEXT("AssetName"), AssetData.AssetName.ToString());
	
	RootObject->SetObjectField(TEXT("AssetSerializedData"), AssetSerializedData);
	RootObject->SetArrayField(TEXT("ObjectHierarchy"), ObjectHierarchySerializer->FinalizeSerialization());

	FString ResultString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
	FJsonSerializer::Serialize(RootObject, Writer);

	const FString OutputFilename = GetDumpFilePath(TEXT(""), TEXT("json"));
	check(FFileHelper::SaveStringToFile(ResultString, *OutputFilename));
}
