#pragma once
#include "CoreMinimal.h"
#include "SimpleAssetGenerator.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialInstanceGenerator.generated.h"

UCLASS(MinimalAPI)
class UMaterialInstanceGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual void PostInitializeAssetGenerator() override;
	virtual UClass* GetAssetObjectClass() const override;

	virtual void PopulateSimpleAssetWithData(UObject* Asset) override;
	virtual bool IsSimpleAssetUpToDate(UObject* Asset) const override;
	FStaticParameterSet GetStaticParameterOverrides() const;
public:
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
protected:
	virtual void PreFinishAssetGeneration() override;
public:
	virtual FName GetAssetClass() override;
private:
	FProperty* AssetUserDataProperty;
};