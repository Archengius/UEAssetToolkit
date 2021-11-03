#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"

class SNotificationItem;

struct FDependencyList {
	UAssetTypeGenerator* AssetTypeGenerator;
	TMap<FName, EAssetGenerationStage> PackageDependencies;
};

enum class EAddPackageResult {
	PACKAGE_EXISTS,
	PACKAGE_WILL_BE_GENERATED,
	PACKAGE_NOT_FOUND
};

/** Describes configuration for the asset generator */
struct ASSETGENERATOR_API FAssetGeneratorConfiguration {
public:
	/** Root directory for the source asset dump */
	FString DumpRootDirectory;
	/** Maximum amount of asset generators to advance in one tick */
	int32 MaxAssetsToAdvancePerTick;
	/** True to refresh existing assets, false to completely ignore assets already present */
	bool bRefreshExistingAssets;
	/** True to generate public project, with all of the non-redistributable asset files replaced with stubs */
	bool bGeneratePublicProject;
	/** If true, ticking will be performed manually by the external code like commandlet, and tickable game object logic will be fully ignored */
	bool bTickOnTheSide;

	FAssetGeneratorConfiguration();
};

struct ASSETGENERATOR_API FAssetGenStatistics {
public:
	/** Amount of new asset packages generated */
	int32 AssetPackagesCreated;
	
	/** Amount of refreshed asset packages */
	int32 AssetPackagesRefreshed;

	/** Amount of asset packages that were up to date */
	int32 AssetPackagesUpToDate;
	
	/** Amount of package assets skipped due to whitelist */
	int32 AssetPackagesSkipped;
	
	/** Total asset packages involved in generation */
	int32 TotalAssetPackages;

	FAssetGenStatistics();

	FORCEINLINE int32 GetTotalPackagesHandled() const {
		return this->AssetPackagesCreated + this->AssetPackagesRefreshed + this->AssetPackagesUpToDate + this->AssetPackagesSkipped;
	}
};

/**
 * Asset generation processor manages asset generation process for the selected group of the assets
 * It generates one asset at a time, going through multiple stages and possibly satisfying generation
 * dependencies for each stage. This way, it keeps minimal amount of packages loaded, so it can be used
 * for large asset baths without impacting memory footprint and performance too much
 */
class ASSETGENERATOR_API FAssetGenerationProcessor : public FTickableGameObject, public TSharedFromThis<FAssetGenerationProcessor> {
private:
	static TSharedPtr<FAssetGenerationProcessor> ActiveAssetGenerator;
	
	/** Configuration for the asset generator */
	FAssetGeneratorConfiguration Configuration;
	/** Package name mapping to it's active asset generator */
	TMap<FName, UAssetTypeGenerator*> AssetGenerators;
	/** Maps package name to the list of dependencies waiting for it's generation */
	TMap<FName, TArray<TSharedPtr<FDependencyList>>> PendingDependencies;
	/** Generators which dependencies have been fully satisfied are added here, they will be advanced next tick */
	TArray<UAssetTypeGenerator*> GeneratorsReadyToAdvance;
	/** External packages checked to exist are added here and checked quickly */
	TSet<FName> ExternalPackagesResolved;
	/** Packages that have been generated before are listed here */
	TSet<FName> AlreadyGeneratedPackages;
	/** Missing packages list, stored here so warnings about them will be logged only one time */
	TSet<FName> KnownMissingPackages;
	/** List of packages that were present in the dump, but skipped because of the whitelisting rules */
	TSet<FName> SkippedPackages;
	/** List of packages to generate */
	TArray<FName> PackagesToGenerate;
	/** Index of the next package to generate */
	int32 NextPackageToGenerateIndex;
	/** True when generation has been finished */
	bool bGenerationFinished;
	/** True if next tick is gonna be first one */
	bool bIsFirstTick;
	/** Statics for current asset generation process */
	FAssetGenStatistics Statistics;
	/** Notification shown to indicate asset generation progress */
	TSharedPtr<SNotificationItem> NotificationItem;

	/** Initializes generator for the provided asset */
	void InitializeAssetGeneratorInternal(UAssetTypeGenerator* Generator);
	/** Marks external package as resolved */
	void MarkExternalPackageDependencySatisfied(FName PackageName);
	/** Marks package as unresolved and prints warning */
	void MarkPackageAsNotFound(FName PackageName);
	/** Marks package as skipped and prints the warning */
	void MarkPackageSkipped(FName PackageName, const FString& Reason);
	/** Determines whenever given package should be abandoned and skipped */
	bool ShouldSkipPackage(UAssetTypeGenerator* PackageGenerator, FString& OutSkipReason) const;
	
	/** Refreshes generator dependencies for the new stage */
	void RefreshGeneratorDependencies(UAssetTypeGenerator* Generator);
	/** Called when generator stage is advanced to notify dependents and refresh dependencies */
	void OnGeneratorStageAdvanced(UAssetTypeGenerator* Generator);
	/** Cleans up asset generator for the provided asset and then removes it */
	void CleanupAssetGenerator(UAssetTypeGenerator* Generator);
	/** Adds new package to asset generator */
	EAddPackageResult AddPackage(FName PackageName);
	/** Called to find new packages for asset generation */
	bool GatherNewAssetsForGeneration();
	/** Called when asset generation is finished */
	void OnAssetGenerationFinished();
	/** Prints current state of the asset generator into the log */
	void PrintStateIntoTheLog();
	/** Ticks asset generation and optionally terminates it when finished */
	void TickAssetGeneration(int32& PackagesGeneratedThisTick);
	/** Called at the first tick of asset generation, before any work is done */
	void OnAssetGenerationStarted();
	/** Updates notification item state if it's valid */
	void UpdateNotificationItem();
	/** Tracks asset generator in statistics */
	void TrackAssetGeneratorStatistics(UAssetTypeGenerator* Generator);

	/** Internal constructor. Call CreateAssetGenerator(...) instead */
	FAssetGenerationProcessor(const FAssetGeneratorConfiguration& Configuration, const TArray<FName>& PackagesToGenerate);
public:
	FORCEINLINE bool HasFinishedAssetGeneration() const { return bGenerationFinished; }
	FORCEINLINE const FAssetGenStatistics& GetStatistics() const { return Statistics; } 
	
	/** Returns currently active instance of the asset generator */
	FORCEINLINE static TSharedPtr<FAssetGenerationProcessor> GetActiveAssetGenerator() {
		return ActiveAssetGenerator;
	}

	/** Creates asset generator and makes it active. Throws exception if another asset gen is currently active */
	static TSharedRef<FAssetGenerationProcessor> CreateAssetGenerator(const FAssetGeneratorConfiguration& Configuration, const TArray<FName>& PackagesToGenerate);

	/** Called manually from the commandlet, gives back some information */
	void TickOnTheSide(int32& PackagesGeneratedThisTick);
	
	//Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override;
	virtual bool IsTickableInEditor() const override;
	//End FTickableGameObject
};

