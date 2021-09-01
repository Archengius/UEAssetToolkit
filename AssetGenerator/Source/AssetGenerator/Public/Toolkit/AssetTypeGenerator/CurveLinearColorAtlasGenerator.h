#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "CurveLinearColorAtlasGenerator.generated.h"

UCLASS(MinimalAPI)
class UCurveLinearColorAtlasGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	void PopulateAtlasAssetWithData(class UCurveLinearColorAtlas* Asset);
	bool IsAtlasUpToDate(class UCurveLinearColorAtlas* Asset) const;
public:
	virtual FName GetAssetClass() override;
};