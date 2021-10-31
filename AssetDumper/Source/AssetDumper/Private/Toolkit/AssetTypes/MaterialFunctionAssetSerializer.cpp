#include "Toolkit/AssetTypes/MaterialFunctionAssetSerializer.h"
#include "Materials/MaterialFunction.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/MaterialAssetSerializer.h"
#include "AssetDumperModule.h"

void UMaterialFunctionAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
	BEGIN_ASSET_SERIALIZATION(UMaterialFunction)
	UMaterialAssetSerializer::DisableMaterialFunctionSerialization(Serializer);
	
	SERIALIZE_ASSET_OBJECT
	END_ASSET_SERIALIZATION
}

FName UMaterialFunctionAssetSerializer::GetAssetClass() const {
	return UMaterialFunction::StaticClass()->GetFName();
}
