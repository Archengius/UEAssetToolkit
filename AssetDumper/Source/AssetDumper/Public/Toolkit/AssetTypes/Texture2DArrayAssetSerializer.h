#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Texture2DArrayAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UTexture2DArrayAssetSerializer : public UAssetTypeSerializer {
	GENERATED_BODY()
public:
	virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

	virtual FName GetAssetClass() const override;
};