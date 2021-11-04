﻿#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "CameraAnimAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UCameraAnimAssetSerializer : public UAssetTypeSerializer {
	GENERATED_BODY()
public:
	virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

	virtual FName GetAssetClass() const override;
};