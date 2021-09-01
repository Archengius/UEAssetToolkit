#include "LandscapeGrassType.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/LandscapeGrassTypeAssetSerializer.h"

void ULandscapeGrassTypeAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
	BEGIN_ASSET_SERIALIZATION(ULandscapeGrassType)
	SERIALIZE_ASSET_OBJECT
	END_ASSET_SERIALIZATION
}

FName ULandscapeGrassTypeAssetSerializer::GetAssetClass() const {
	return ULandscapeGrassType::StaticClass()->GetFName();
}



