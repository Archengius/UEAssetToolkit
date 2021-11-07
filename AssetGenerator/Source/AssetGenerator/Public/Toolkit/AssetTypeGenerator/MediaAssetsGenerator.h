#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetTypeGenerator/SimpleAssetGenerator.h"
#include "MediaAssetsGenerator.generated.h"

UCLASS(MinimalAPI)
class UMediaPlayerGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
public:
	virtual FName GetAssetClass() override;
};

UCLASS(MinimalAPI)
class UMediaTextureGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
public:
	virtual FName GetAssetClass() override;
};

UCLASS(MinimalAPI)
class UFileMediaSourceGenerator : public USimpleAssetGenerator {
	GENERATED_BODY()
protected:
	virtual UClass* GetAssetObjectClass() const override;
	virtual void PopulateSimpleAssetWithData(UObject* Asset) override;
public:
	virtual FName GetAssetClass() override;
};
