#include "Toolkit/AssetTypeGenerator/StaticMeshGenerator.h"
#include "AutomatedAssetImportData.h"
#include "MeshDescription.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/ReimportFbxStaticMeshFactory.h"
#include "PhysicsEngine/BodySetup.h"
#include "Toolkit/AssetGeneration/PublicProjectStubHelper.h"

void UStaticMeshGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(*GetPackageName().ToString());
	UStaticMesh* NewStaticMesh = ImportStaticMesh(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewStaticMesh);
	
	PopulateStaticMeshWithData(NewStaticMesh);
}

void UStaticMeshGenerator::OnExistingPackageLoaded() {
	UStaticMesh* ExistingMesh = GetAsset<UStaticMesh>();
	
	if (!IsStaticMeshSourceFileUpToDate(ExistingMesh)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing StaticMesh %s Source Model"), *GetPackageName().ToString());
		ReimportStaticMeshSource(ExistingMesh);
	}
	if (!IsStaticMeshDataUpToDate(ExistingMesh)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing StaticMesh %s Properties"), *GetPackageName().ToString());
		PopulateStaticMeshWithData(ExistingMesh);
	}
}

UStaticMesh* UStaticMeshGenerator::ImportStaticMesh(UPackage* Package, const FName& AssetName, const EObjectFlags ObjectFlags) {
	UFbxFactory* StaticMeshFactory = NewObject<UFbxFactory>(GetTransientPackage(), NAME_None);
	
	StaticMeshFactory->SetAutomatedAssetImportData(NewObject<UAutomatedAssetImportData>(StaticMeshFactory));
	StaticMeshFactory->SetDetectImportTypeOnImport(false);
	SetupFbxImportSettings(StaticMeshFactory->ImportUI);

	FString AssetFbxFilePath;
	if (!IsGeneratingPublicProject()) {
		AssetFbxFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("fbx"));
	} else {
		AssetFbxFilePath = FPublicProjectStubHelper::EditorCube.GetFullFilePath();
	}
	
	bool bOperationCancelled = false;
	UObject* ResultMesh = StaticMeshFactory->ImportObject(UStaticMesh::StaticClass(), Package, AssetName, ObjectFlags, AssetFbxFilePath, TEXT(""), bOperationCancelled);
	
	checkf(ResultMesh, TEXT("Failed to import StaticMesh %s from FBX file %s. See log for errors"), *GetPackageName().ToString(), *AssetFbxFilePath);
	checkf(ResultMesh->GetOuter() == Package, TEXT("Expected Outer to be package %s, found %s"), *Package->GetName(), *ResultMesh->GetOuter()->GetPathName());
	checkf(ResultMesh->GetFName() == AssetName, TEXT("Expected Name to be %s, but found %s"), *AssetName.ToString(), *ResultMesh->GetName());
	
	return CastChecked<UStaticMesh>(ResultMesh);
}

void UStaticMeshGenerator::ReimportStaticMeshSource(UStaticMesh* Asset) {
	UReimportFbxStaticMeshFactory* StaticMeshFactory = NewObject<UReimportFbxStaticMeshFactory>(GetTransientPackage(), NAME_None);
	
	StaticMeshFactory->SetAutomatedAssetImportData(NewObject<UAutomatedAssetImportData>(StaticMeshFactory));
	StaticMeshFactory->SetDetectImportTypeOnImport(false);
	SetupFbxImportSettings(StaticMeshFactory->ImportUI);
	
	FString AssetFbxFilePath;
	if (!IsGeneratingPublicProject()) {
		AssetFbxFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("fbx"));
	} else {
		AssetFbxFilePath = FPublicProjectStubHelper::EditorCube.GetFullFilePath();
	}
	
	StaticMeshFactory->SetReimportPaths(Asset, {AssetFbxFilePath});
	StaticMeshFactory->Reimport(Asset);
	MarkAssetChanged();
}

void UStaticMeshGenerator::SetupFbxImportSettings(UFbxImportUI* ImportUI) const {
	ImportUI->MeshTypeToImport = FBXIT_StaticMesh;
	ImportUI->bOverrideFullName = true;
	ImportUI->bImportMaterials = false;
	ImportUI->bImportTextures = false;

	ImportUI->StaticMeshImportData = NewObject<UFbxStaticMeshImportData>(ImportUI, NAME_None, RF_NoFlags);
	ImportUI->StaticMeshImportData->NormalImportMethod = FBXNIM_ImportNormalsAndTangents;
	ImportUI->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
	ImportUI->StaticMeshImportData->bAutoGenerateCollision = true;

	const TSharedPtr<FJsonObject> AssetData = GetAssetData();	
	const TArray<TSharedPtr<FJsonValue>> ScreenSize = AssetData->GetArrayField(TEXT("ScreenSize"));
	float LODScreenSizes[MAX_STATIC_MESH_LODS];
	
	for (int32 i = 0; i < MAX_STATIC_MESH_LODS; i++) {
		LODScreenSizes[i] = ScreenSize[i]->AsNumber();	
	}

	ImportUI->MinimumLodNumber = AssetData->GetIntegerField(TEXT("MinimumLodNumber"));
	ImportUI->LodNumber = AssetData->GetIntegerField(TEXT("LodNumber"));

	ImportUI->LodDistance0 = LODScreenSizes[0];
	ImportUI->LodDistance1 = LODScreenSizes[1];
	ImportUI->LodDistance2 = LODScreenSizes[2];
	ImportUI->LodDistance3 = LODScreenSizes[3];
	ImportUI->LodDistance4 = LODScreenSizes[4];
	ImportUI->LodDistance5 = LODScreenSizes[5];
	ImportUI->LodDistance6 = LODScreenSizes[6];
	ImportUI->LodDistance7 = LODScreenSizes[7];
}

void UStaticMeshGenerator::PopulateStaticMeshWithData(UStaticMesh* Asset) {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	GetObjectSerializer()->DeserializeObjectProperties(AssetObjectProperties.ToSharedRef(), Asset);
	
	const TArray<TSharedPtr<FJsonValue>>& Materials = AssetData->GetArrayField(TEXT("Materials"));
	
	//TODO not quite exactly the case, because for some LODs the material instances can be different, but we discard any LODs
	//ensure(Materials.Num() == Asset->StaticMaterials.Num());
	
	for (int32 i = 0; i < FMath::Min(Materials.Num(), Asset->StaticMaterials.Num()); i++) {
		const TSharedPtr<FJsonObject> MaterialObject = Materials[i]->AsObject();
		const FName MaterialSlotName = FName(*MaterialObject->GetStringField(TEXT("MaterialSlotName")));
		UObject* MaterialInterface = GetObjectSerializer()->DeserializeObject(MaterialObject->GetIntegerField(TEXT("MaterialInterface")));
		
		FStaticMaterial& StaticMaterial = Asset->StaticMaterials[i];
		StaticMaterial.MaterialSlotName = MaterialSlotName;
		if (MaterialInterface) {
			StaticMaterial.MaterialInterface = CastChecked<UMaterialInterface>(MaterialInterface);
		}
	}
	
	UObject* NavCollision = GetObjectSerializer()->DeserializeObject(AssetData->GetIntegerField(TEXT("NavCollision")));
	UObject* BodySetupObject = GetObjectSerializer()->DeserializeObject(AssetData->GetIntegerField(TEXT("BodySetup")));
	
	Asset->NavCollision = Cast<UNavCollisionBase>(NavCollision);
	Asset->BodySetup = Cast<UBodySetup>(BodySetupObject);
	MarkAssetChanged();
}

bool UStaticMeshGenerator::IsStaticMeshDataUpToDate(UStaticMesh* Asset) const {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	//If object properties are not up to date, we need to re-apply them
	if (!GetObjectSerializer()->AreObjectPropertiesUpToDate(AssetObjectProperties.ToSharedRef(), Asset)) {
		return false;
	}

	//check material slot names and values
	const TArray<TSharedPtr<FJsonValue>>& Materials = AssetData->GetArrayField(TEXT("Materials"));
	//TODO not quite exactly the case, because for some LODs the material instances can be different, but we discard any LODs
	//ensure(Materials.Num() == Asset->StaticMaterials.Num());
	
	for (int32 i = 0; i < FMath::Min(Materials.Num(), Asset->StaticMaterials.Num()); i++) {
		const TSharedPtr<FJsonObject> MaterialObject = Materials[i]->AsObject();
		
		const FName MaterialSlotName = FName(*MaterialObject->GetStringField(TEXT("MaterialSlotName")));
		const int32 MaterialInterface = MaterialObject->GetIntegerField(TEXT("MaterialInterface"));
		const FStaticMaterial& StaticMaterial = Asset->StaticMaterials[i];
		
		if (!GetObjectSerializer()->CompareUObjects(MaterialInterface, StaticMaterial.MaterialInterface, true, true) ||
			StaticMaterial.MaterialSlotName != MaterialSlotName) {
			return false;
		}
	}

	//check some properties serialized externally
	const int32 NavCollisionObjectIndex = AssetData->GetIntegerField(TEXT("NavCollision"));
	const int32 BodySetupObjectIndex = AssetData->GetIntegerField(TEXT("BodySetup"));
	
	if (!GetObjectSerializer()->CompareUObjects(NavCollisionObjectIndex, Asset->NavCollision, false, false)) {
		return false;
	}
	if (!GetObjectSerializer()->CompareUObjects(BodySetupObjectIndex, Asset->BodySetup, false, false)) {
		return false;
	}
	return true;
}

bool UStaticMeshGenerator::IsStaticMeshSourceFileUpToDate(UStaticMesh* Asset) const {
	const FAssetImportInfo& AssetImportInfo = Asset->AssetImportData->SourceData;
	
	if (AssetImportInfo.SourceFiles.Num() == 0) {
		return false;
	}
	
	const FMD5Hash& ExistingFileHash = AssetImportInfo.SourceFiles[0].FileHash;
	const FString ExistingFileHashString = LexToString(ExistingFileHash);

	FString ModelFileHash;
	if (!IsGeneratingPublicProject()) {
		ModelFileHash = GetAssetData()->GetStringField(TEXT("ModelFileHash"));;
	} else {
		ModelFileHash = FPublicProjectStubHelper::EditorCube.GetFileHash();
	}
	return ExistingFileHashString == ModelFileHash;
}

void UStaticMeshGenerator::PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		const TSharedPtr<FJsonObject> AssetData = GetAssetData();
		
		const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));
		const TArray<TSharedPtr<FJsonValue>> ReferencedObjects = AssetObjectProperties->GetArrayField(TEXT("$ReferencedObjects"));

		const TArray<TSharedPtr<FJsonValue>>& Materials = AssetData->GetArrayField(TEXT("Materials"));
		const int32 NavCollisionObjectIndex = AssetData->GetIntegerField(TEXT("NavCollision"));
		const int32 BodySetupObjectIndex = AssetData->GetIntegerField(TEXT("BodySetup"));
		
		TArray<FString> OutReferencedPackages;
		GetObjectSerializer()->CollectReferencedPackages(ReferencedObjects, OutReferencedPackages);
		GetObjectSerializer()->CollectObjectPackages(NavCollisionObjectIndex, OutReferencedPackages);
		GetObjectSerializer()->CollectObjectPackages(BodySetupObjectIndex, OutReferencedPackages);

		for (int32 i = 0; i < Materials.Num(); i++) {
			const TSharedPtr<FJsonObject> MaterialObject = Materials[i]->AsObject();
			const int32 MaterialInterface = MaterialObject->GetIntegerField(TEXT("MaterialInterface"));
			GetObjectSerializer()->CollectObjectPackages(MaterialInterface, OutReferencedPackages);
		}
		
		for (const FString& DependencyPackageName : OutReferencedPackages) {
			OutDependencies.Add(FPackageDependency{*DependencyPackageName, EAssetGenerationStage::CDO_FINALIZATION});
		}
	}
}

FName UStaticMeshGenerator::GetAssetClass() {
	return UStaticMesh::StaticClass()->GetFName();
}
