#pragma once
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "TextureGenerator.generated.h"

class UTexture;

UCLASS(Abstract)
class ASSETGENERATOR_API UTextureGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	
	virtual void UpdateTextureSource(UTexture* Texture);
	virtual void UpdateTextureInfo(UTexture* Texture);
	
	static FString ComputeTextureHash(UTexture* Texture);
	static FString ComputeBlankTextureHash(int32 Width, int32 Height, int32 NumTextures);

	void SetTextureSourceToDumpFile(UTexture* Texture);
	static void SetTextureSourceToWhite(UTexture* Texture);

	virtual TSubclassOf<UTexture> GetTextureClass() PURE_VIRTUAL(, return NULL;);
};