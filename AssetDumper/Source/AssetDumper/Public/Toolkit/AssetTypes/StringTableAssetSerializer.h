#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "StringTableAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UStringTableAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
