#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "SimpleAssetGen.generated.h"

UCLASS(MinimalAPI)
class USimpleAssetGen : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
public:
	virtual FName GetAssetClass() override;
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
};