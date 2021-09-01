#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "FontAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UFontAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
