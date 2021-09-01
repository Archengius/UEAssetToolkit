#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "AnimMontageGenerator.generated.h"

UCLASS(MinimalAPI)
class UAnimMontageGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
public:
	virtual FName GetAssetClass() override;
};