#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "SubsurfaceProfileGenerator.generated.h"

UCLASS(MinimalAPI)
class USubsurfaceProfileGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
public:
	virtual FName GetAssetClass() override;
};