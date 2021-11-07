#include "Toolkit/AssetTypeGenerator/Texture2DArrayGenerator.h"
#include "Engine/Texture2DArray.h"

void UTexture2DArrayGenerator::UpdateTextureSource(UTexture* Texture) {
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->PowerOfTwoMode = ETexturePowerOfTwoSetting::None;
	
	Super::UpdateTextureSource(Texture);
}

TSubclassOf<UTexture> UTexture2DArrayGenerator::GetTextureClass() {
	return UTexture2DArray::StaticClass();
}

FName UTexture2DArrayGenerator::GetAssetClass() {
	return UTexture2DArray::StaticClass()->GetFName();
}
