#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "CurveLinearColorAtlasAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UCurveLinearColorAtlasAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;    
};
