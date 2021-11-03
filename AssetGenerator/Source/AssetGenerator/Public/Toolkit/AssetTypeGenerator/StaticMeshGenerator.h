#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "StaticMeshGenerator.generated.h"

UCLASS(MinimalAPI)
class UStaticMeshGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	
	void PopulateStaticMeshWithData(UStaticMesh* Asset);
	UStaticMesh* ImportStaticMesh(UPackage* Package, const FName& AssetName, const EObjectFlags ObjectFlags);
	void ReimportStaticMeshSource(UStaticMesh* Asset);
	void SetupFbxImportSettings(class UFbxImportUI* ImportUI) const;
	
	bool IsStaticMeshDataUpToDate(UStaticMesh* Asset) const;
	bool IsStaticMeshSourceFileUpToDate(UStaticMesh* Asset) const;
public:
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
	virtual FName GetAssetClass() override;
};