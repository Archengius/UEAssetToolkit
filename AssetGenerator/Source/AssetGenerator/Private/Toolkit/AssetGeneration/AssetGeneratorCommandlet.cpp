#include "Toolkit/AssetGeneration/AssetGeneratorCommandlet.h"
#include "FileHelpers.h"
#include "Toolkit/AssetGeneration/AssetDumpViewWidget.h"
#include "Toolkit/AssetGeneration/AssetGenerationProcessor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ShaderCompiler.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Toolkit/AssetGeneration/AssetGenerationUtil.h"

DEFINE_LOG_CATEGORY(LogAssetGeneratorCommandlet)

struct FAssetGeneratorGCController {
private:
	int32 PackagesCookedSinceLastGC;
	double LastGCTimestamp;

	float MaxSecondsBetweenGC;
	int32 MaxPackagesBetweenGC;
public:
	explicit FAssetGeneratorGCController(float MaxSecondsBetweenGC = 20.0f, int32 MaxPackagesBetweenGC = 32) {
		this->PackagesCookedSinceLastGC = 0;
		this->LastGCTimestamp = 0.0f;
		this->MaxSecondsBetweenGC = MaxSecondsBetweenGC;
		this->MaxPackagesBetweenGC = MaxPackagesBetweenGC;
	}
		
	void Update(int32 PackagesGeneratedThisTick) {
		this->PackagesCookedSinceLastGC += PackagesGeneratedThisTick;
	}

	void ConditionallyCollectGarbage() {
		bool bShouldGC = false;

		if (this->PackagesCookedSinceLastGC >= MaxPackagesBetweenGC) {
			bShouldGC = true;
		}
		if (FApp::GetCurrentTime() - LastGCTimestamp >= MaxSecondsBetweenGC) {
			bShouldGC = true;
		}
		if (bShouldGC) {
			UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("Collecting garbage"));
			CollectGarbage(RF_Standalone);

			this->PackagesCookedSinceLastGC = 0;
			this->LastGCTimestamp = FApp::GetCurrentTime();
		}
	}
};

UAssetGeneratorCommandlet::UAssetGeneratorCommandlet() {
	HelpDescription = TEXT("Generates assets from the dump located in the provided folder using the provided settings");
	HelpUsage = TEXT("assetgenerator -DumpDirectory=Path/To/Directory [-ForceGeneratePackageNames=ForceGeneratePackageNames.txt] [-BlacklistPackageNames=BlacklistPackageNames.txt] [-AssetClassWhitelist=Class1,Class2] [-NoRefresh] [-PublicProject]");
	ShowErrorCount = false;
}

int32 UAssetGeneratorCommandlet::Main(const FString& Params) {
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);
		
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const bool bRefreshExistingAssets = !Switches.Contains(TEXT("NoRefresh"));
	const bool bGeneratePublicProject = Switches.Contains(TEXT("PublicProject"));
	
	FString DumpDirectory;
	{
		if (!FParse::Value(*Params, TEXT("DumpDirectory="), DumpDirectory)) {
			UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("Usage: %s"), *HelpUsage);
			return 1;
		}
		FPaths::NormalizeDirectoryName(DumpDirectory);

		if (!PlatformFile.DirectoryExists(*DumpDirectory)) {
			UE_LOG(LogAssetGeneratorCommandlet, Error, TEXT("Asset dump root directory %s does not exist"), *DumpDirectory);
		}
	}
	
	TSharedPtr<TSet<FName>> WhitelstedAssetClasses;
	{
		FString AssetClassWhitelistString;
		if (FParse::Value(*Params, TEXT("AssetClassWhitelist="), AssetClassWhitelistString)) {
			TArray<FString> AssetClassWhitelist;
			AssetClassWhitelistString.ParseIntoArray(AssetClassWhitelist, TEXT(","));

			WhitelstedAssetClasses = MakeShared<TSet<FName>>();
			for (const FString& AssetClass : AssetClassWhitelist) {
				WhitelstedAssetClasses->Add(*AssetClass);
			}
		}
	}

	//Collect package names that are force included, disregarding any kind of exclusion rules
	TArray<FString> ForceGeneratePackageNames;
	{
		FString ForceGeneratePackageNamesFile;
		if (FParse::Value(*Params, TEXT("ForceGeneratePackageNames="), ForceGeneratePackageNamesFile)) {

			if (!PlatformFile.FileExists(*ForceGeneratePackageNamesFile)) {
				UE_LOG(LogAssetGeneratorCommandlet, Error, TEXT("Force generated package list file %s does not exist"), *ForceGeneratePackageNamesFile);
				return 1;
			}

			FString ResultFileContents;
			FFileHelper::LoadFileToString(ResultFileContents, *ForceGeneratePackageNamesFile);
			ResultFileContents.ParseIntoArrayLines(ForceGeneratePackageNames);
		}
	}

	//Build a list of packages to skip saving for in final save pass
	TArray<FString> InMemoryPackagesToSkip;
	{
		FString SkipSavePackagesFile;
		if (FParse::Value(*Params, TEXT("SkipSavePackages="), SkipSavePackagesFile)) {

			if (!PlatformFile.FileExists(*SkipSavePackagesFile)) {
				UE_LOG(LogAssetGeneratorCommandlet, Error, TEXT("Skip save packages file %s does not exist"), *SkipSavePackagesFile);
				return 1;
			}

			FString ResultFileContents;
			FFileHelper::LoadFileToString(ResultFileContents, *SkipSavePackagesFile);
			ResultFileContents.ParseIntoArrayLines(InMemoryPackagesToSkip);
		}
	}

	//Build a blacklist function
	TFunction<bool(const FString& PackageName)> PackageNameBlacklistFilter = [](const FString& PackageName) { return true; };
	{
		FString BlacklistPackageNamesFile;
		if (FParse::Value(*Params, TEXT("BlacklistPackageNames="), BlacklistPackageNamesFile)) {

			if (!PlatformFile.FileExists(*BlacklistPackageNamesFile)) {
				UE_LOG(LogAssetGeneratorCommandlet, Error, TEXT("Blacklisted package names file %s does not exist"), *BlacklistPackageNamesFile);
				return 1;
			}

			FString ResultFileContents;
			FFileHelper::LoadFileToString(ResultFileContents, *BlacklistPackageNamesFile);

			TArray<FString> PackageNamesLines;
			ResultFileContents.ParseIntoArrayLines(PackageNamesLines);

			TSet<FString> BlacklistPackageNames;
			TArray<FString> WildcardBlacklistedPaths;
			
			for (const FString& PackageNameOrPath : PackageNamesLines) {
				if (PackageNameOrPath.EndsWith(TEXT("/"))) {
					//Wildcard path - add to array for fast iteration
					WildcardBlacklistedPaths.Add(PackageNameOrPath);
				} else {
					//Exact package name - add to set for instant lookup
					BlacklistPackageNames.Add(PackageNameOrPath);
				}
			}

			PackageNameBlacklistFilter = [=](const FString& PackageName) {
				if (BlacklistPackageNames.Contains(PackageName)) {
					return false;
				}
				for (const FString& PackagePath : WildcardBlacklistedPaths) {
					if (PackageName.StartsWith(PackagePath)) {
						return false;
					}
				}
				return true;
			};
		}
	}

	//Create basic asset generator configuration
	FAssetGeneratorConfiguration Configuration;
	Configuration.DumpRootDirectory = DumpDirectory;
	Configuration.bRefreshExistingAssets = bRefreshExistingAssets;
	Configuration.bGeneratePublicProject = bGeneratePublicProject;

	//Populate the initial list of the packages with asset category filters applied
	TArray<FName> ResultPackagesToGenerate;
	{
		TArray<FName> GeneratedPackageNames;
		const TSharedPtr<FAssetDumpTreeNode> RootNode = FAssetDumpTreeNode::CreateRootTreeNode(DumpDirectory);
		RootNode->PopulateGeneratedPackages(GeneratedPackageNames, WhitelstedAssetClasses.Get());

		if (GeneratedPackageNames.Num() == 0) {
			UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("No assets matching the specified category whitelist found"));
			return 0;
		}

		TSet<FName> PackagesAlreadyAdded;

		//First, add force included packages that ignore filters
		for (const FString& ForceIncludedPackageName : ForceGeneratePackageNames) {
			const FName PackageName = *ForceIncludedPackageName;
			ResultPackagesToGenerate.Add(PackageName);
			PackagesAlreadyAdded.Add(PackageName);
		}

		//Then iterate all of the remaining packages, applying blacklist in the process
		for (const FName& GeneratedPackageName : GeneratedPackageNames) {
			if (PackagesAlreadyAdded.Contains(GeneratedPackageName)) {
				continue;
			}
			if (!PackageNameBlacklistFilter(GeneratedPackageName.ToString())) {
				continue;
			}
			ResultPackagesToGenerate.Add(GeneratedPackageName);
			PackagesAlreadyAdded.Add(GeneratedPackageName);
		}
	}

	//Print the amount of assets to be generated and start the generation processor
	UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("Starting generation of %d game assets"), ResultPackagesToGenerate.Num());

	//Synchronize asset registry state with the current assets we have on the disk
	ClearEmptyGamePackagesLoadedDuringDisregardGC();
	IAssetRegistry::GetChecked().SearchAllAssets(true);
	ProcessDeferredCommands();

	FAssetGeneratorGCController AssetGeneratorGCController{};
	AssetGeneratorGCController.ConditionallyCollectGarbage();

	//We always want to tick the generator manually, even though without engine ticking game objects will not be processed anyway
	Configuration.bTickOnTheSide = true;
	TSharedPtr<FAssetGenerationProcessor> GenerationProcessor = FAssetGenerationProcessor::CreateAssetGenerator(Configuration, ResultPackagesToGenerate);
	
	while (!GenerationProcessor->HasFinishedAssetGeneration() && !IsEngineExitRequested()) {
		int32 GeneratedPkgCount = 0;
		GenerationProcessor->TickOnTheSide(GeneratedPkgCount);

		// Flush the asset registry before GC
		FAssetRegistryModule::TickAssetRegistry(-1.0f);

		AssetGeneratorGCController.Update(GeneratedPkgCount);
		AssetGeneratorGCController.ConditionallyCollectGarbage();

		//process deferred commands
		ProcessDeferredCommands();

		//Tick shader compilation if we have any pending async shader jobs
		while (GShaderCompilingManager->HasShaderJobs() && !IsEngineExitRequested()) {
			
			// force at least a tick shader compilation even if we are requesting stuff
			GShaderCompilingManager->ProcessAsyncResults(true, false);
			
			ProcessDeferredCommands();
			AssetGeneratorGCController.ConditionallyCollectGarbage();
		}
	}

	//Save any packages that are still in memory and have dirty flag
	TArray<UPackage*> InMemoryDirtyPackages;

	for (TObjectIterator<UPackage> It; It; ++It) {
		UPackage* Package = *It;
		if (Package->IsDirty() && !InMemoryPackagesToSkip.Contains(Package->GetName())) {
			UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("Scheduling save of dirty package %s"), *Package->GetName());
			InMemoryDirtyPackages.Add(Package);
		}
	}

	if (InMemoryDirtyPackages.Num()) {
		UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("Saving %d in-memory dirty packages after the asset generation is done"), InMemoryDirtyPackages.Num());

		for (UPackage* Package : InMemoryDirtyPackages) {
			UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("Saving dirty package %s"), *Package->GetName());
			UEditorLoadingAndSavingUtils::SavePackages({Package}, true);
		}
		AssetGeneratorGCController.ConditionallyCollectGarbage();
	}

	//Synchronize asset registry state with all of the new packages we created
	IAssetRegistry::GetChecked().SearchAllAssets(true);
	
	UE_LOG(LogAssetGeneratorCommandlet, Display, TEXT("Asset generation finished successfully"));
	return 0;
}

void UAssetGeneratorCommandlet::ProcessDeferredCommands() {
	// update task graph
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	// execute deferred commands
	for (int32 DeferredCommandsIndex = 0; DeferredCommandsIndex< GEngine->DeferredCommands.Num(); DeferredCommandsIndex++) {
		GEngine->Exec(GWorld, *GEngine->DeferredCommands[DeferredCommandsIndex], *GLog);
	}
	GEngine->DeferredCommands.Empty();
}

void UAssetGeneratorCommandlet::ClearEmptyGamePackagesLoadedDuringDisregardGC() {
	//Some packages get loaded through the config system during the initial load, which then
	//marks them as GC disregarded, so they end up being loaded as empty rooted packages when actual referenced
	//packages on disk do not exist. Here we track down these packages and delete them, because they
	//interfere with the asset generator by the commandlet
	for(TObjectIterator<UPackage> It; It; ++It) {
		UPackage* Package = *It;
		if (!Package->GetName().StartsWith(TEXT("/Game/"))) {
			continue;
		}
		
		int32 ExistingObjectsNum = 0;
		ForEachObjectWithPackage(Package, [&](UObject* Object){
			ExistingObjectsNum++;
			return false;
		});
		if (ExistingObjectsNum != 0) {
			continue;
		}

		UE_LOG(LogAssetGenerator, Warning, TEXT("Found empty game package loaded during the initial load: %s. It will be deleted"), *Package->GetName());

		const FString NewPackageName = FString::Printf(TEXT("TRASH_%s"), *Package->GetName());
		const FName NewUniquePackageName = MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass(), FName(*NewPackageName));

		//Package->RemoveFromRoot();
		Package->SetFlags(RF_Transient);
		Package->Rename(*NewUniquePackageName.ToString(), NULL, REN_DoNothing);
		//Package->MarkPendingKill();
	}
	//CollectGarbage(RF_Standalone);
}
