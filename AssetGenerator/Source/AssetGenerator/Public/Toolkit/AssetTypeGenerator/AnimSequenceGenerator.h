#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "AnimSequenceGenerator.generated.h"

UCLASS(MinimalAPI)
class UAnimSequenceGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	
	void PopulateAnimationProperties(UAnimSequence* Asset);
	bool IsAnimationPropertiesUpToDate(UAnimSequence* Asset) const;
	
	UAnimSequence* ImportAnimation(UPackage* Package, const FName& AssetName, const EObjectFlags ObjectFlags);
	void ReimportAnimationFromSource(UAnimSequence* Asset);
	bool IsAnimationSourceUpToDate(UAnimSequence* Asset) const;

	void SetupFbxImportSettings(class UFbxImportUI* ImportUI) const;
public:
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
	virtual FName GetAssetClass() override;
};