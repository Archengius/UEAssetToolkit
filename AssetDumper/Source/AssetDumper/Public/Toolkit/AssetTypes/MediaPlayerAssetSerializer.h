#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "MediaPlayerAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UMediaPlayerAssetSerializer : public UAssetTypeSerializer {
	GENERATED_BODY()
public:
	virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

	virtual FName GetAssetClass() const override;
};