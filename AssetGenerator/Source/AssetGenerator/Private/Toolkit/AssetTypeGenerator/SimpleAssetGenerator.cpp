#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"

void USimpleAssetGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UClass* AssetClass = GetAssetObjectClass();
	UObject* NewAssetObject = NewObject<UObject>(NewPackage, AssetClass, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewAssetObject);
	
	PopulateSimpleAssetWithData(NewAssetObject);
}

void USimpleAssetGenerator::OnExistingPackageLoaded() {
	UObject* ExistingAssetObject = GetAsset<UObject>();

	if (!IsSimpleAssetUpToDate(ExistingAssetObject)) {
		UE_LOG(LogAssetGenerator, Display, TEXT("%s %s is not up to date, regenerating data"), *ExistingAssetObject->GetClass()->GetName(), *ExistingAssetObject->GetPathName());
		
		PopulateSimpleAssetWithData(ExistingAssetObject);
	}
}

void USimpleAssetGenerator::PopulateSimpleAssetWithData(UObject* Asset) {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	GetObjectSerializer()->DeserializeObjectProperties(AssetObjectProperties.ToSharedRef(), Asset);
}

bool USimpleAssetGenerator::IsSimpleAssetUpToDate(UObject* Asset) const {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	return GetObjectSerializer()->AreObjectPropertiesUpToDate(AssetObjectProperties.ToSharedRef(), Asset);
}

void USimpleAssetGenerator::PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		PopulateReferencedObjectsDependencies(OutDependencies);
	}
}
