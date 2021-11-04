#include "Toolkit/AssetTypes/Texture2DArrayAssetSerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/PropertySerializer.h"
#include "Engine/Texture2DArray.h"
#include "Toolkit/AssetTypes/TextureAssetSerializer.h"

void UTexture2DArrayAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
	BEGIN_ASSET_SERIALIZATION(UTexture2DArray)
	DISABLE_SERIALIZATION_RAW(UTexture, TEXT("LightingGuid"));
	
	SERIALIZE_ASSET_OBJECT
	UTextureAssetSerializer::SerializeTextureData(Asset->GetPathName(), Asset->PlatformData, Data, Context, false, TEXT(""));   
	END_ASSET_SERIALIZATION
}

FName UTexture2DArrayAssetSerializer::GetAssetClass() const {
	return UTexture2DArray::StaticClass()->GetFName();
}
