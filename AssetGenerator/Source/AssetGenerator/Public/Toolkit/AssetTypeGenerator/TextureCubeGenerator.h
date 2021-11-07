#pragma once
#include "Toolkit/AssetTypeGenerator/TextureGenerator.h"
#include "TextureCubeGenerator.generated.h"

UCLASS(MinimalAPI)
class UTextureCubeGenerator : public UTextureGenerator {
	GENERATED_BODY()
protected:
	virtual TSubclassOf<UTexture> GetTextureClass() override;
public:
	virtual FName GetAssetClass() override;
};