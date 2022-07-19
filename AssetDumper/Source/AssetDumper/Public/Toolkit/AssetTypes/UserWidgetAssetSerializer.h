#pragma once
#include "Blueprint/WidgetTree.h"
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "UserWidgetAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UUserWidgetAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
        
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;
    
    virtual FName GetAssetClass() const override;
};
