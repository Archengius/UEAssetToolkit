#pragma once
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "Texture2DGenerator.generated.h"

UCLASS()
class ASSETGENERATOR_API UTexture2DGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	void RebuildTextureData(class UTexture2D* Texture);
	static FString ComputeTextureHash(UTexture2D* Texture);
public:
	/** Checks whenever texture is up-to-date. Exposed to public because other assets often embed textures inside themselves */
	static bool IsTextureUpToDate(UTexture2D* ExistingTexture,
		UObjectHierarchySerializer* ObjectSerializer,
		const TSharedPtr<FJsonObject> AssetData,
		bool bIsGeneratingPublicProject = false);

	/** Rebuilds texture data for the provided texture using provided image file and asset data */
	static void RebuildTextureData(UTexture2D* Texture,
		const FString& TextureFilePath,
		UObjectHierarchySerializer* ObjectSerializer,
		const TSharedPtr<FJsonObject> AssetData,
		bool bIsGeneratingPublicProject = false);
	
	virtual FName GetAssetClass() override;
};