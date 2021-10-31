#include "Toolkit/AssetTypes/MaterialInstanceAssetSerializer.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Toolkit/PropertySerializer.h"

void UMaterialInstanceAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UMaterialInstanceConstant)
    DISABLE_SERIALIZATION(FStaticParameterSet, MaterialLayersParameters);
    SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UMaterialInstanceAssetSerializer::GetAssetClass() const {
    return UMaterialInstanceConstant::StaticClass()->GetFName();
}
