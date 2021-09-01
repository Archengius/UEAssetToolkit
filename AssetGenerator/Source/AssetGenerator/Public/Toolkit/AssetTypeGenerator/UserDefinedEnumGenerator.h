#pragma once
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "UserDefinedEnumGenerator.generated.h"

UCLASS(MinimalAPI)
class UUserDefinedEnumGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	void PopulateEnumWithData(class UUserDefinedEnum* Enum);
	bool IsEnumerationUpToDate(class UUserDefinedEnum* Enum) const;
public:
	virtual FName GetAssetClass() override;
};