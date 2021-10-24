#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "StaticMeshAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UStaticMeshAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
    virtual bool SupportsParallelDumping() const override;
};
