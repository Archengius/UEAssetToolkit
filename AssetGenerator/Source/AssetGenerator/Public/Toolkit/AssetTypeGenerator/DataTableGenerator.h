#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "DataTableGenerator.generated.h"

UCLASS(MinimalAPI)
class UDataTableGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	using FTableRowMap = TMap<FName, TSharedPtr<class FStructOnScope>>;
	
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	virtual void PopulateAssetWithData() override;
	void PopulateDataTableWithData(class UDataTable* DataTable, const FTableRowMap& TableRowMap);
	bool IsDataTableUpToDate(class UDataTable* DataTable, const FTableRowMap& TableRowMap) const;
public:
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
	virtual FName GetAssetClass() override;
};