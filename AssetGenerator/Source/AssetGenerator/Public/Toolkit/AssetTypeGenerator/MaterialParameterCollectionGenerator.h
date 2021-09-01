#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "MaterialParameterCollectionGenerator.generated.h"

UCLASS(MinimalAPI)
class UMaterialParameterCollectionGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual void PostInitializeAssetGenerator() override;
	virtual UClass* GetAssetObjectClass() const override;
	virtual void PopulateSimpleAssetWithData(UObject* Asset) override;
public:
	virtual FName GetAssetClass() override;
};