#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "AssetTypeGenerator.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAssetGenerator, Log, All);

class UObjectHierarchySerializer;
class UPropertySerializer;
class FJsonObject;

/** Describes various phases of asset generation, followed by each other */
enum class EAssetGenerationStage {
	CONSTRUCTION,
	DATA_POPULATION,
	CDO_FINALIZATION,
	PRE_FINSHED,
	FINISHED
};

/** Describes a single asset package dependency to be satisfied */
struct ASSETGENERATOR_API FPackageDependency {
	const FName PackageName;
	const EAssetGenerationStage State;
};

struct ASSETGENERATOR_API FGeneratorStateAdvanceResult {
	EAssetGenerationStage NewStage;
	bool bPreviousStageNotImplemented;
};

UCLASS()
class ASSETGENERATOR_API UAssetTypeGenerator : public UObject {
	GENERATED_BODY()
private:
	FString DumpRootDirectory;
	FString PackageBaseDirectory;
    FName PackageName;
	FName AssetName;
    TSharedPtr<FJsonObject> AssetData;
	EAssetGenerationStage CurrentStage;
	bool bUsingExistingPackage;
	bool bAssetChanged;
	bool bHasAssetEverBeenChanged;
	bool bIsGeneratingPublicProject;
	bool bIsStageNotOverriden;
	
	UPROPERTY()
    UObjectHierarchySerializer* ObjectSerializer;
	UPROPERTY()
	UPropertySerializer* PropertySerializer;
	UPROPERTY()
	UPackage* AssetPackage;
	UPROPERTY()
	UObject* AssetObject;

	/** Initializes this asset generator instance with the file data */
	void InitializeInternal(const FString& DumpRootDirectory, const FString& PackageBaseDirectory, FName PackageName, TSharedPtr<FJsonObject> RootFileObject, bool bGeneratePublicProject);

	/** Dispatches asset construction and tries to locate existing packages */
	void ConstructAssetAndPackage();
protected:
	FORCEINLINE TSharedPtr<FJsonObject> GetAssetData() const { return AssetData; }
	
	/** Retrieves path to the base directory containing current asset data */
	FORCEINLINE const FString& GetPackageBaseDirectory() const { return PackageBaseDirectory; };

	/** Returns instance of the active property serializer */
	FORCEINLINE UPropertySerializer* GetPropertySerializer() const { return PropertySerializer; }

	/** Returns instance of the object hierarchy serializer associated with this package */
	FORCEINLINE UObjectHierarchySerializer* GetObjectSerializer() const { return ObjectSerializer; }

	/** True whenever we are generating project that is publicly distributable. Some assets will be blanked out in that case */
	FORCEINLINE bool IsGeneratingPublicProject() const { return bIsGeneratingPublicProject; }

	FORCEINLINE TSharedPtr<FJsonObject> GetAssetObjectData() const { return AssetData->GetObjectField(TEXT("AssetObjectData")); }

	/** Marks asset as changed by this generator */
	void MarkAssetChanged();

	void SetPackageAndAsset(UPackage* NewPackage, UObject* NewAsset, bool bSetObjectMark = true);

	FString GetAdditionalDumpFilePath(const FString& Postfix, const FString& Extension) const;

	FORCEINLINE const FString& GetRootDumpDirectory() const { return DumpRootDirectory; }

	/** Adds assets referenced by the asset object through referenced objects */
	void PopulateReferencedObjectsDependencies(TArray<FPackageDependency>& OutDependencies) const;

	/** Called right after asset generator is initialized with asset data */
	virtual void PostInitializeAssetGenerator() {}

	/** Allocates new package object and asset object inside of it */
	virtual void CreateAssetPackage() PURE_VIRTUAL(ConstructAsset, );
	virtual void PopulateAssetWithData();
	virtual void FinalizeAssetCDO();
	virtual void PreFinishAssetGeneration();

	/** Called when existing package is loaded from the disk to be used with asset generator. In that case, no CreateAssetPackage call will happen */
	virtual void OnExistingPackageLoaded() {};

	/** Append additional packages that need to be saved when this package is changed here */
	virtual void GetAdditionalPackagesToSave(TArray<UPackage*>& OutPackages) {};

	static void MoveToTransientPackageAndRename(UObject* Object);
public:
	UAssetTypeGenerator();

	/** Sets generating public project mode on this generator */
	FORCEINLINE void SetGeneratingPublicProject() { this->bIsGeneratingPublicProject = true; }

	/** Returns name of the asset object as it is loaded from the dump */
	FORCEINLINE FName GetAssetName() const { return AssetName; }
	
	FORCEINLINE bool IsUsingExistingPackage() const { return bUsingExistingPackage; }

	FORCEINLINE bool HasAssetBeenEverChanged() const { return bHasAssetEverBeenChanged; }
	
	/** Returns package name of the asset being generated */
	FORCEINLINE FName GetPackageName() const { return PackageName; }

	/** Returns current stage of the asset generation for this asset */
	FORCEINLINE EAssetGenerationStage GetCurrentStage() const { return CurrentStage; }

	/** Returns asset package created by CreateAssetPackage or loaded from the disk */
	FORCEINLINE UPackage* GetAssetPackage() const { return AssetPackage; }

	template<typename T>
	FORCEINLINE T* GetAsset() const { return CastChecked<T>(AssetObject); }

	/** Populates array with the dependencies required to perform current asset generation stage */
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {}

	/** Attempts to advance asset generation stage. Returns new stage, or finished if generation is finished */
	FGeneratorStateAdvanceResult AdvanceGenerationState();

	/** Additional asset classes handled by this generator, can be empty, these have lower priority than GetAssetClass */
	virtual void GetAdditionallyHandledAssetClasses(TArray<FName>& OutExtraAssetClasses) {}
	
	/** Determines class of the asset this generator is capable of generating. Will be called on CDO, do not access any state here! */
	virtual FName GetAssetClass() PURE_VIRTUAL(GetAssetClass, return NAME_None;);

	/** Returns file path corresponding to the provided package in the root directory */
	static FString GetAssetFilePath(const FString& RootDirectory, FName PackageName);

	/** Tries to load asset generator state from the asset dump located under the provided root directory and having given package name */
	static UAssetTypeGenerator* InitializeFromFile(const FString& RootDirectory, FName PackageName, bool bGeneratePublicProject);

	static TArray<TSubclassOf<UAssetTypeGenerator>> GetAllGenerators();

	/** Finds generator capable of generating asset of the given class */
	static TSubclassOf<UAssetTypeGenerator> FindGeneratorForClass(FName AssetClass);
};