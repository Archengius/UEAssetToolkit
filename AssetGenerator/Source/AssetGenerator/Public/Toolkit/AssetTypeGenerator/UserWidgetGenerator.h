#pragma once
#include "CoreMinimal.h"
#include "BlueprintGenerator.h"
#include "UserWidgetGenerator.generated.h"

class UBlueprint;

UCLASS(MinimalAPI)
class UUserWidgetGenerator : public UBlueprintGenerator {
	GENERATED_BODY()
protected:
	virtual void PostInitializeAssetGenerator() override;
	virtual UBlueprint* CreateNewBlueprint(UPackage* Package, UClass* ParentClass) override;
	virtual void FinalizeAssetCDO() override;
	virtual UClass* GetFallbackParentClass() const override;
public:
	virtual FName GetAssetClass() override;
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
};