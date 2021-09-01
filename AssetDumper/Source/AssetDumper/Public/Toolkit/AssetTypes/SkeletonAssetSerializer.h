#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "SkeletonAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class USkeletonAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;
    static void SerializeSmartNameContainer(const struct FSmartNameContainer& Container, TSharedPtr<class FJsonObject> OutObject);
    
    virtual FName GetAssetClass() const override;
	virtual bool SupportsParallelDumping() const override;
};
