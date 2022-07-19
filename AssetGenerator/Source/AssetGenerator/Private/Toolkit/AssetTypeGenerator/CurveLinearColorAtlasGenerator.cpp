#include "Toolkit/AssetTypeGenerator/CurveLinearColorAtlasGenerator.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetTypeGenerator/Texture2DGenerator.h"

void UCurveLinearColorAtlasGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UCurveLinearColorAtlas* NewAsset = NewObject<UCurveLinearColorAtlas>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewAsset);
	
	PopulateAtlasAssetWithData(NewAsset);
}

void UCurveLinearColorAtlasGenerator::OnExistingPackageLoaded() {
	UCurveLinearColorAtlas* ExistingAssetObject = GetAsset<UCurveLinearColorAtlas>();

	if (!IsAtlasUpToDate(ExistingAssetObject)) {
		UE_LOG(LogAssetGenerator, Display, TEXT("CurveLinearColorAtlas %s is not up to date, regenerating data"), *ExistingAssetObject->GetPathName());
		
		PopulateAtlasAssetWithData(ExistingAssetObject);
	}
}

void UCurveLinearColorAtlasGenerator::PopulateAtlasAssetWithData(UCurveLinearColorAtlas* Asset) {
	const FString TextureFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("png"));
	UTexture2DGenerator::RebuildTextureData(Asset, TextureFilePath, GetObjectSerializer(), GetAssetData());
	Asset->PostLoad();
}

bool UCurveLinearColorAtlasGenerator::IsAtlasUpToDate(UCurveLinearColorAtlas* Asset) const {
	return UTexture2DGenerator::IsTextureUpToDate(Asset, GetObjectSerializer(), GetAssetData());
}

FName UCurveLinearColorAtlasGenerator::GetAssetClass() {
	return UCurveLinearColorAtlas::StaticClass()->GetFName();
}
