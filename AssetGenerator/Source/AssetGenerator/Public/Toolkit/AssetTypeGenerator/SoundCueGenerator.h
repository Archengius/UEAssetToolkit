#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "SoundCueGenerator.generated.h"

UCLASS(MinimalAPI)
class USoundCueGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
	virtual void CreateAssetPackage() override;
public:
	virtual FName GetAssetClass() override;
};