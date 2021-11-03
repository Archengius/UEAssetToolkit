#include "Toolkit/AssetTypeGenerator/AnimSequenceGenerator.h"
#include "AutomatedAssetImportData.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxImportUI.h"
#include "Factories/ReimportFbxAnimSequenceFactory.h"

void UAnimSequenceGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(*GetPackageName().ToString());
	UAnimSequence* NewAnimSequence = ImportAnimation(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewAnimSequence);
	PopulateAnimationProperties(NewAnimSequence);
}

void UAnimSequenceGenerator::OnExistingPackageLoaded() {
	UAnimSequence* ExistingAnimation = GetAsset<UAnimSequence>();
	
	if (!IsAnimationSourceUpToDate(ExistingAnimation)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing AnimationSequence %s Source File"), *GetPackageName().ToString());
		ReimportAnimationFromSource(ExistingAnimation);
	}
	
	if (!IsAnimationPropertiesUpToDate(ExistingAnimation)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing AnimationSequence %s Properties"), *GetPackageName().ToString());
		PopulateAnimationProperties(ExistingAnimation);
	}
}

UAnimSequence* UAnimSequenceGenerator::ImportAnimation(UPackage* Package, const FName& AssetName, const EObjectFlags ObjectFlags) {
	UFbxFactory* AnimationFactory = NewObject<UFbxFactory>(GetTransientPackage(), NAME_None);
	
	AnimationFactory->SetAutomatedAssetImportData(NewObject<UAutomatedAssetImportData>(AnimationFactory));
	AnimationFactory->SetDetectImportTypeOnImport(false);
	SetupFbxImportSettings(AnimationFactory->ImportUI);

	const FString AssetFbxFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("fbx"));
	
	bool bOperationCancelled = false;
	UObject* ResultAnimation = AnimationFactory->ImportObject(UAnimSequence::StaticClass(), Package, AssetName, ObjectFlags, AssetFbxFilePath, TEXT(""), bOperationCancelled);
	
	checkf(ResultAnimation, TEXT("Failed to import AnimSequence %s from FBX file %s. See log for errors"), *GetPackageName().ToString(), *AssetFbxFilePath);
	checkf(ResultAnimation->GetOuter() == Package, TEXT("Expected Outer to be package %s, found %s"), *Package->GetName(), *ResultAnimation->GetOuter()->GetPathName());
	checkf(ResultAnimation->GetFName() == AssetName, TEXT("Expected Name to be %s, but found %s"), *AssetName.ToString(), *ResultAnimation->GetName());
	
	return CastChecked<UAnimSequence>(ResultAnimation);
}

void UAnimSequenceGenerator::ReimportAnimationFromSource(UAnimSequence* Asset) {
	UClass* ReimportFbxAnimSequenceFactoryClass = FindObjectChecked<UClass>(NULL, TEXT("/Script/UnrealEd.ReimportFbxAnimSequenceFactory"));
	UReimportFbxAnimSequenceFactory* AnimationFactory = static_cast<UReimportFbxAnimSequenceFactory*>(NewObject<UObject>(GetTransientPackage(), ReimportFbxAnimSequenceFactoryClass));
	
	AnimationFactory->SetAutomatedAssetImportData(NewObject<UAutomatedAssetImportData>(AnimationFactory));
	AnimationFactory->SetDetectImportTypeOnImport(false);
	SetupFbxImportSettings(AnimationFactory->ImportUI);
	
	const FString AssetFbxFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("fbx"));
	
	AnimationFactory->SetReimportPaths(Asset, {AssetFbxFilePath});
	AnimationFactory->Reimport(Asset);
	MarkAssetChanged();
}

void UAnimSequenceGenerator::SetupFbxImportSettings(UFbxImportUI* ImportUI) const {
	ImportUI->MeshTypeToImport = FBXIT_Animation;
	ImportUI->bOverrideFullName = true;
	ImportUI->bImportMaterials = false;
	ImportUI->bImportTextures = false;
	ImportUI->bImportAnimations = true;
	ImportUI->OverrideAnimationName = GetAssetName().ToString();

	const int32 SkeletonObjectIndex = GetAssetData()->GetObjectField(TEXT("AssetObjectData"))->GetIntegerField(TEXT("Skeleton"));
	USkeleton* Skeleton = CastChecked<USkeleton>(GetObjectSerializer()->DeserializeObject(SkeletonObjectIndex));
	ImportUI->Skeleton = Skeleton;

	ImportUI->AnimSequenceImportData = NewObject<UFbxAnimSequenceImportData>(ImportUI, NAME_None, RF_NoFlags);
	ImportUI->AnimSequenceImportData->bRemoveRedundantKeys = true;

	const int32 ExportedFrameRate = GetAssetData()->GetIntegerField(TEXT("FrameRate"));
	ImportUI->AnimSequenceImportData->bUseDefaultSampleRate = false;
	ImportUI->AnimSequenceImportData->CustomSampleRate = ExportedFrameRate;

	const int32 NumFrames = GetAssetData()->GetIntegerField(TEXT("NumFrames"));
	ImportUI->AnimSequenceImportData->AnimationLength = EFBXAnimationLengthImportType::FBXALIT_SetRange;
	ImportUI->AnimSequenceImportData->FrameImportRange = FInt32Interval(0, NumFrames - 1);
}

void UAnimSequenceGenerator::PopulateAnimationProperties(UAnimSequence* Asset) {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	GetObjectSerializer()->DeserializeObjectProperties(AssetObjectProperties.ToSharedRef(), Asset);
	MarkAssetChanged();
}

bool UAnimSequenceGenerator::IsAnimationPropertiesUpToDate(UAnimSequence* Asset) const {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	return GetObjectSerializer()->AreObjectPropertiesUpToDate(AssetObjectProperties.ToSharedRef(), Asset);
}

bool UAnimSequenceGenerator::IsAnimationSourceUpToDate(UAnimSequence* Asset) const {
	//TEMPFIX: Old dump files do not have the correct model file hash inside of them, so assume asset is up to date when it is missing
	if (!GetAssetData()->HasField(TEXT("ModelFileHash"))) {
		return true;
	}
	
	const FAssetImportInfo& AssetImportInfo = Asset->AssetImportData->SourceData;
	const FMD5Hash& ExistingFileHash = AssetImportInfo.SourceFiles[0].FileHash;
	
	const FString ExistingFileHashString = LexToString(ExistingFileHash);
	const FString ModelFileHash = GetAssetData()->GetStringField(TEXT("ModelFileHash"));
	return ExistingFileHashString == ModelFileHash;
}

void UAnimSequenceGenerator::PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		PopulateReferencedObjectsDependencies(OutDependencies);
	}
}

FName UAnimSequenceGenerator::GetAssetClass() {
	return UAnimSequence::StaticClass()->GetFName();
}
