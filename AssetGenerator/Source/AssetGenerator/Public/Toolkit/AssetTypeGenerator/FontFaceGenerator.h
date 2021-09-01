#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "FontFaceGenerator.generated.h"

UCLASS(MinimalAPI)
class UFontFaceGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	void PopulateFontFaceWithData(class UFontFace* FontFace);
	bool IsFontFaceUpToDate(class UFontFace* FontFace) const;
public:
	virtual FName GetAssetClass() override;
};