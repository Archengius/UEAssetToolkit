#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "LandscapeGrassTypeAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class ULandscapeGrassTypeAssetSerializer : public UAssetTypeSerializer {
	GENERATED_BODY()
public:
	virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;
	virtual FName GetAssetClass() const override;
};