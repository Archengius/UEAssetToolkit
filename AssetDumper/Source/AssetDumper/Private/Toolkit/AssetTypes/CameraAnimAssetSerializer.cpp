#include "Toolkit/AssetTypes/CameraAnimAssetSerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Camera/CameraAnim.h"

void UCameraAnimAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
	BEGIN_ASSET_SERIALIZATION(UCameraAnim)
	SERIALIZE_ASSET_OBJECT
	END_ASSET_SERIALIZATION
}

FName UCameraAnimAssetSerializer::GetAssetClass() const {
	return UCameraAnim::StaticClass()->GetFName();
}