#pragma once
#include "Toolkit/AssetTypeGenerator/TextureGenerator.h"
#include "Texture2DArrayGenerator.generated.h"

UCLASS(MinimalAPI)
class UTexture2DArrayGenerator : public UTextureGenerator {
	GENERATED_BODY()
protected:
	virtual void UpdateTextureSource(UTexture* Texture) override;
	virtual TSubclassOf<UTexture> GetTextureClass() override;
public:
	virtual FName GetAssetClass() override;
};