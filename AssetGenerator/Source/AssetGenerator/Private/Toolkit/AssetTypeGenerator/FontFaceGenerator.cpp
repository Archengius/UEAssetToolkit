#include "Toolkit/AssetTypeGenerator/FontFaceGenerator.h"
#include "Dom/JsonObject.h"
#include "Engine/FontFace.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"

void UFontFaceGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UFontFace* NewFontFace = NewObject<UFontFace>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewFontFace);
	
	PopulateFontFaceWithData(NewFontFace);
}

void UFontFaceGenerator::OnExistingPackageLoaded() {
	UFontFace* ExistingFontFace = GetAsset<UFontFace>();

	if (!IsFontFaceUpToDate(ExistingFontFace)) {
		UE_LOG(LogAssetGenerator, Display, TEXT("FontFace %s is not up to date, regenerating data"), *ExistingFontFace->GetPathName());
		
		PopulateFontFaceWithData(ExistingFontFace);
	}
}

void UFontFaceGenerator::PopulateFontFaceWithData(UFontFace* FontFace) {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const FString FontFilename = GetAdditionalDumpFilePath(TEXT(""), TEXT("ttf"));

	const TSharedPtr<FJsonObject> AssetObjectData = AssetData->GetObjectField(TEXT("AssetObjectData"));
	GetObjectSerializer()->DeserializeObjectProperties(AssetObjectData.ToSharedRef(), FontFace);
	
	TArray<uint8> LoadedFontfaceData;
	checkf(FFileHelper::LoadFileToArray(LoadedFontfaceData, *FontFilename), TEXT("Failed to load fontface file %s"), *FontFilename);
	FontFace->FontFaceData->SetData(MoveTemp(LoadedFontfaceData));
	FontFace->CacheSubFaces();
}

bool UFontFaceGenerator::IsFontFaceUpToDate(UFontFace* FontFace) const {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const FString FontPayloadHash = AssetData->GetStringField(TEXT("FontPayloadHash"));

	const FString ExistingDataHash = FAssetHelper::ComputePayloadHash(FontFace->FontFaceData->GetData());
	if (ExistingDataHash != FontPayloadHash) {
		return false;
	}

	const TSharedPtr<FJsonObject> AssetObjectData = AssetData->GetObjectField(TEXT("AssetObjectData"));
	if (!GetObjectSerializer()->AreObjectPropertiesUpToDate(AssetObjectData.ToSharedRef(), FontFace)) {
		return false;
	}
	return true;
}

FName UFontFaceGenerator::GetAssetClass() {
	return UFontFace::StaticClass()->GetFName();
}
