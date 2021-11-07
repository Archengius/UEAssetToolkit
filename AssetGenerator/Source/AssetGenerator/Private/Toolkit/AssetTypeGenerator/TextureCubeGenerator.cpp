#include "Toolkit/AssetTypeGenerator/TextureCubeGenerator.h"
#include "Engine/TextureCube.h"

TSubclassOf<UTexture> UTextureCubeGenerator::GetTextureClass() {
	return UTextureCube::StaticClass();
}

FName UTextureCubeGenerator::GetAssetClass() {
	return UTextureCube::StaticClass()->GetFName();
}
