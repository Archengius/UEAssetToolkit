#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "UserDefinedStructAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UUserDefinedStructAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;    
};
