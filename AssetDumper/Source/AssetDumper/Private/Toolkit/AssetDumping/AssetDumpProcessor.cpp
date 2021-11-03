#include "Toolkit/AssetDumping/AssetDumpProcessor.h"
#include "Async/ParallelFor.h"
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "AssetDumperModule.h"

using FInlinePackageArray = TArray<FPendingPackageData, TInlineAllocator<16>>;

#define DEFAULT_PACKAGES_TO_PROCESS_PER_TICK 16

FAssetDumpSettings::FAssetDumpSettings() :
		RootDumpDirectory(GetDefaultRootDumpDirectory()),
        MaxPackagesToProcessInOneTick(DEFAULT_PACKAGES_TO_PROCESS_PER_TICK),
        bForceSingleThread(false),
        bOverwriteExistingAssets(true),
		bExitOnFinish(false),
		GarbageCollectionInterval(10.0f) {
}

FString FAssetDumpSettings::GetDefaultRootDumpDirectory() {
	FString ResultDefaultPath = FPaths::ProjectDir() + TEXT("AssetDump/");
	FPaths::NormalizeDirectoryName(ResultDefaultPath);
	return ResultDefaultPath;
}

TSharedPtr<FAssetDumpProcessor> FAssetDumpProcessor::ActiveDumpProcessor = NULL;

FAssetDumpProcessor::FAssetDumpProcessor(const FAssetDumpSettings& Settings, const TArray<FAssetData>& InAssets) {
	this->Settings = Settings;
	this->PackagesToLoad = InAssets;
	InitializeAssetDump();
}

FAssetDumpProcessor::FAssetDumpProcessor(const FAssetDumpSettings& Settings, const TMap<FName, FAssetData>& InAssets) {
	this->Settings = Settings;
	InAssets.GenerateValueArray(this->PackagesToLoad);
	InitializeAssetDump();
}

FAssetDumpProcessor::~FAssetDumpProcessor() {
	//Unroot all currently unprocessed UPackages and make sure we have no in-fly package load requests,
	//which will crash trying to call our method upon finishing after we've been destructed
	check(PackageLoadRequestsInFlyCounter.GetValue() == 0);
	
	for (const FPendingPackageData& PackageData : this->LoadedPackages) {
		PackageData.AssetObject->RemoveFromRoot();
	}
	
	this->LoadedPackages.Empty();
	this->AssetDataByPackageName.Empty();
	this->PackagesToLoad.Empty();
}

TSharedRef<FAssetDumpProcessor> FAssetDumpProcessor::StartAssetDump(const FAssetDumpSettings& Settings, const TArray<FAssetData>& InAssets) {
	checkf(!ActiveDumpProcessor.IsValid(), TEXT("StartAssetDump is called while another asset dump is in progress"));
	
	TSharedRef<FAssetDumpProcessor> NewProcessor = MakeShareable(new FAssetDumpProcessor(Settings, InAssets));
	ActiveDumpProcessor = NewProcessor;
	return NewProcessor;
}

TSharedRef<FAssetDumpProcessor> FAssetDumpProcessor::StartAssetDump(const FAssetDumpSettings& Settings, const TMap<FName, FAssetData>& InAssets) {
	checkf(!ActiveDumpProcessor.IsValid(), TEXT("StartAssetDump is called while another asset dump is in progress"));
	
	TSharedRef<FAssetDumpProcessor> NewProcessor = MakeShareable(new FAssetDumpProcessor(Settings, InAssets));
	ActiveDumpProcessor = NewProcessor;
	return NewProcessor;
}

void FAssetDumpProcessor::Tick(float DeltaTime) {
	this->TimeSinceGarbageCollection += DeltaTime;

	//Collect garbage if we exceeded GC interval
	if (TimeSinceGarbageCollection >= Settings.GarbageCollectionInterval) {
		UE_LOG(LogAssetDumper, Log, TEXT("Forcing garbage collection (GC interval exceeded)"));
		this->TimeSinceGarbageCollection = 0.0f;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	
	//Load packages as long as we have space in queue + packages to process
	while (PackageLoadRequestsInFlyCounter.GetValue() < MaxLoadRequestsInFly &&
			PackagesWaitingForProcessing.GetValue() < MaxPackagesInProcessQueue	&&
			CurrentPackageToLoadIndex < PackagesToLoad.Num()) {
		
		//Associate package data with the package name (so we can find it later in async load request handler and increment counter)
		FAssetData* AssetDataToLoadNext = &PackagesToLoad[CurrentPackageToLoadIndex++];
		this->AssetDataByPackageName.Add(AssetDataToLoadNext->PackageName, AssetDataToLoadNext);
		PackageLoadRequestsInFlyCounter.Increment();

		//Start actual async loading of the asset, use our function as handler
		LoadPackageAsync(AssetDataToLoadNext->PackageName.ToString(), FLoadPackageAsyncDelegate::CreateRaw(this, &FAssetDumpProcessor::OnPackageLoaded));
	}

	//Process pending dump requests in parallel for loop
	FInlinePackageArray PackagesToProcessThisTick;
	PackagesToProcessThisTick.Reserve(MaxPackagesToProcessInOneTick);

	//Lock packages array and copy elements from it
	this->LoadedPackagesCriticalSection.Lock();
	
	const int32 ElementsToCopy = FMath::Min(LoadedPackages.Num(), MaxPackagesToProcessInOneTick);
	PackagesToProcessThisTick.Append(LoadedPackages.GetData(), ElementsToCopy);
	LoadedPackages.RemoveAt(0, ElementsToCopy, false);
	PackagesWaitingForProcessing.Subtract(ElementsToCopy);	

	this->LoadedPackagesCriticalSection.Unlock();

	FInlinePackageArray PackagesToProcessParallel;
	FInlinePackageArray PackagesToProcessInMainThread;

	for (const FPendingPackageData& PackageData : PackagesToProcessThisTick) {
		if (PackageData.Serializer->SupportsParallelDumping()) {
			PackagesToProcessParallel.Add(PackageData);
		} else {
			PackagesToProcessInMainThread.Add(PackageData);
		}
	}

	if (PackagesToProcessParallel.Num()) {
		EParallelForFlags ParallelFlags = EParallelForFlags::Unbalanced;
		if (Settings.bForceSingleThread) {
			ParallelFlags |= EParallelForFlags::ForceSingleThread;
		}
		ParallelFor(PackagesToProcessParallel.Num(), [this, &PackagesToProcessParallel](const int32 PackageIndex) {
            PerformAssetDumpForPackage(PackagesToProcessParallel[PackageIndex]);
        }, ParallelFlags);
	}

	if (PackagesToProcessInMainThread.Num()) {
		for (int32 i = 0; i < PackagesToProcessInMainThread.Num(); i++) {
			PerformAssetDumpForPackage(PackagesToProcessInMainThread[i]);
		}
	}

	if (CurrentPackageToLoadIndex >= PackagesToLoad.Num() &&
		PackageLoadRequestsInFlyCounter.GetValue() == 0 &&
		PackagesWaitingForProcessing.GetValue() == 0) {
		UE_LOG(LogAssetDumper, Display, TEXT("Asset dumping finished successfully"));
		this->bHasFinishedDumping = true;

		//If we were requested to exit on finish, do it now
		if (Settings.bExitOnFinish) {
			UE_LOG(LogAssetDumper, Display, TEXT("Exiting because bExitOnFinish was set to true in asset dumper settings..."));
			RequestEngineExit(TEXT("AssetDumper finished working with bExitOnFinish set"));
		}

		//If we represent an active dump processor, make sure to reset ourselves once dumping is done
		if (ActiveDumpProcessor.Get() == this) {
			ActiveDumpProcessor.Reset();
		}
	}
}

void FAssetDumpProcessor::OnPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result) {
	//Reduce package load requests in fly counter, so next request can be made
	this->PackageLoadRequestsInFlyCounter.Decrement();

	//Make sure request suceeded
	if (Result != EAsyncLoadingResult::Succeeded) {
		UE_LOG(LogAssetDumper, Error, TEXT("Failed to load package %s for dumping. It will be skipped."), *PackageName.ToString());
		PackagesSkipped.Increment();
		return;
	}

	check(LoadedPackage);
	FPendingPackageData PendingPackageData;
	
	if (!CreatePackageData(LoadedPackage, PendingPackageData)) {
		this->PackagesSkipped.Increment();
		return;
	}
	
	//Add asset object to the root set so it will not be garbage collected while waiting to be processed
	PendingPackageData.AssetObject->AddToRoot();

	//Add package to loaded ones, using critical section so we avoid concurrency issues
	this->LoadedPackagesCriticalSection.Lock();
	this->LoadedPackages.Add(PendingPackageData);
	this->LoadedPackagesCriticalSection.Unlock();

	//Increment counter for packages in queue, it will prevent main thread from loading more packages if queue is already full
	this->PackagesWaitingForProcessing.Increment();
}

void FAssetDumpProcessor::PerformAssetDumpForPackage(const FPendingPackageData& PackageData) {
	UE_LOG(LogAssetDumper, Display, TEXT("Serializing asset %s"), *PackageData.Package->GetName());

	//Serialize asset, finalize serialization, save data into file
	PackageData.Serializer->SerializeAsset(PackageData.SerializationContext.ToSharedRef());
	PackageData.SerializationContext->Finalize();

	//Unroot object now, we have processed it already and do not need to keep it in memory anymore
	PackageData.AssetObject->RemoveFromRoot();
	
	this->PackagesProcessed.Increment();
}

bool FAssetDumpProcessor::IsTickable() const {
	return bHasFinishedDumping == false;
}

TStatId FAssetDumpProcessor::GetStatId() const {
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAssetDumpProcessor, STATGROUP_Game);
}

bool FAssetDumpProcessor::IsTickableWhenPaused() const {
	return true;
}

bool FAssetDumpProcessor::CreatePackageData(UPackage* Package, FPendingPackageData& PendingPackageData) {
	const FAssetData* AssetData = AssetDataByPackageName.FindChecked(Package->GetFName());
	UAssetTypeSerializer* Serializer = UAssetTypeSerializer::FindSerializerForAssetClass(AssetData->AssetClass);

	if (Serializer == NULL) {
		UE_LOG(LogAssetDumper, Warning, TEXT("Skipping dumping asset %s, failed to find serializer for the associated asset class '%s'"), *Package->GetName(), *AssetData->AssetClass.ToString());
		return false;
	}

	UObject* AssetObject = FSerializationContext::GetAssetObjectFromPackage(Package, *AssetData);
	checkf(AssetObject, TEXT("Failed to find asset object '%s' inside of the package '%s'"), *AssetData->AssetName.ToString(), *Package->GetPathName());

	const TSharedPtr<FSerializationContext> Context = MakeShareable(new FSerializationContext(Settings.RootDumpDirectory, *AssetData, AssetObject));
	
	//Check for existing asset files
	if (!Settings.bOverwriteExistingAssets) {
		const FString AssetOutputFile = Context->GetDumpFilePath(TEXT(""), TEXT("json"));
		
		//Skip dumping when we have a dump file already and are not allowed to overwrite assets
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssetOutputFile)) {
			UE_LOG(LogAssetDumper, Display, TEXT("Skipping dumping asset %s, dump file is already present and overwriting is not allowed"), *Package->GetName());
			return false;
		}
	}

	PendingPackageData.Package = Package;
	PendingPackageData.AssetObject = AssetObject;
	PendingPackageData.SerializationContext = Context;
	PendingPackageData.Serializer = Serializer;
	return true;
}

void FAssetDumpProcessor::InitializeAssetDump() {
	this->TimeSinceGarbageCollection = 0.0f;
	this->CurrentPackageToLoadIndex = 0;
	this->bHasFinishedDumping = false;
	this->PackagesTotal = PackagesToLoad.Num();

	this->MaxPackagesToProcessInOneTick = Settings.MaxPackagesToProcessInOneTick;
	this->MaxLoadRequestsInFly = Settings.MaxPackagesToProcessInOneTick;
	this->MaxPackagesInProcessQueue = Settings.MaxPackagesToProcessInOneTick * 2;
	
	UE_LOG(LogAssetDumper, Display, TEXT("Starting asset dump of %d packages..."), PackagesTotal);
}
