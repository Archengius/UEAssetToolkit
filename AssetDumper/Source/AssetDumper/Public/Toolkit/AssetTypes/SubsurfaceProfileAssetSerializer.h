#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "SubsurfaceProfileAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class USubsurfaceProfileAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
