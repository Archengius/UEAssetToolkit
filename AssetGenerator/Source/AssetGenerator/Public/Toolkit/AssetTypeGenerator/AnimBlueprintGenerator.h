#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetGenerationUtil.h"
#include "Toolkit/AssetTypeGenerator/BlueprintGenerator.h"
#include "Engine/Blueprint.h"
#include "AnimBlueprintGenerator.generated.h"

UCLASS()
class ASSETGENERATOR_API UAnimBlueprintGenerator : public UBlueprintGenerator {
GENERATED_BODY()
protected:
	//virtual void CreateAssetPackage() override;
	//virtual void OnExistingPackageLoaded() override;

	virtual UBlueprint* CreateNewBlueprint(UPackage* Package, UClass* ParentClass) override;
	//virtual void PostConstructOrUpdateAsset(UBlueprint* Blueprint);
	//virtual void PopulateAssetWithData() override;
	//virtual void FinalizeAssetCDO() override;
	//void UpdateDeserializerBlueprintClassObject(bool bRecompileBlueprint);
	//virtual UClass* GetFallbackParentClass() const;
public:
	//virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
	virtual FName GetAssetClass() override;
};
