#include "Toolkit/AssetTypes/MediaTextureAssetSerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "MediaTexture.h"
#include "Toolkit/PropertySerializer.h"

void UMediaTextureAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
	BEGIN_ASSET_SERIALIZATION(UMediaTexture)
	
	//Do not serialize any UTexture properties, they are controlled automatically
	UStruct* Struct = UTexture::StaticClass();
	for (TFieldIterator<FProperty> It(Struct); It; ++It) {
		Serializer->DisablePropertySerialization(Struct, *It->GetName());
	}
	
	SERIALIZE_ASSET_OBJECT
	END_ASSET_SERIALIZATION
}

FName UMediaTextureAssetSerializer::GetAssetClass() const {
	return UMediaTexture::StaticClass()->GetFName();
}
