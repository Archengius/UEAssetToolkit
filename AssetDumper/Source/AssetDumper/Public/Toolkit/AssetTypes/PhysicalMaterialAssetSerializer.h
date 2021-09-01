#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "PhysicalMaterialAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UPhysicalMaterialAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
