#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "BlendSpaceAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UBlendSpaceSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual void GetAdditionallyHandledAssetClasses(TArray<FName>& OutExtraAssetClasses) override;
    virtual FName GetAssetClass() const override;
};
