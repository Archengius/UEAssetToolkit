#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "FontFaceAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UFontFaceAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    virtual FName GetAssetClass() const override;
};
