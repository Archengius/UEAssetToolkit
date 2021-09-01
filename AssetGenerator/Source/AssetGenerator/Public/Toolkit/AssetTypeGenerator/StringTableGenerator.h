#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "StringTableGenerator.generated.h"

UCLASS(MinimalAPI)
class UStringTableGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	void PopulateStringTableWithData(class UStringTable* StringTable);
	bool IsStringTableUpToDate(class UStringTable* StringTable) const;
public:
	virtual FName GetAssetClass() override;
};