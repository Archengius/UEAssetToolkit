#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "CurveBaseGenerator.generated.h"

UCLASS(MinimalAPI)
class UCurveBaseGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
public:
	virtual void GetAdditionallyHandledAssetClasses(TArray<FName>& OutExtraAssetClasses) override;
	virtual FName GetAssetClass() override;
};