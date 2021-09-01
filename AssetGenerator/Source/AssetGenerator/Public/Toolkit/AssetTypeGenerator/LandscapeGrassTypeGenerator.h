#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "LandscapeGrassTypeGenerator.generated.h"

UCLASS(MinimalAPI)
class ULandscapeGrassTypeGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
public:
	virtual FName GetAssetClass() override;
};