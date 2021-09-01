#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "UserDefinedEnumAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UUserDefinedEnumAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;    
};
