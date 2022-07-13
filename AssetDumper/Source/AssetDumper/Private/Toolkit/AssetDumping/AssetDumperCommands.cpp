#include "Toolkit/AssetDumping/AssetDumperCommands.h"
#include "AssetRegistryModule.h"
#include "Toolkit/AssetDumping/AssetDumperWidget.h"
#include "Toolkit/AssetDumping/AssetDumpConsoleWidget.h"
#include "Toolkit/AssetDumping/AssetRegistryViewWidget.h"
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Util/GameEditorHelper.h"

#define LOCTEXT_NAMESPACE "AssetDumper"

void FAssetDumperCommands::OpenAssetDumperUI() {
	TSharedRef<SWindow> Window = SNew(SWindow)
						.Title(LOCTEXT("AssetDumper_Title", "Asset Dumper Settings"))
						.MinWidth(800).MinHeight(600)
						.AutoCenter(EAutoCenter::PreferredWorkArea);
	Window->SetContent(SNew(SAssetDumperWidget));

	const TSharedRef<SWindow> ParentWindow = FGameEditorHelper::GetMainWindow().ToSharedRef();
	FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow, true);
}

void FAssetDumperCommands::OpenAssetDumperProgressConsole() {
	const TSharedPtr<FAssetDumpProcessor> ActiveProcessor = FAssetDumpProcessor::GetActiveDumpProcessor();
	if (ActiveProcessor.IsValid()) {
		SAssetDumpConsoleWidget::CreateForAssetDumper(ActiveProcessor.ToSharedRef());
	}
}

void FAssetDumperCommands::DumpAllGameAssets(const FString& Params) {
	const TSharedRef<FSelectedAssetsStruct> SelectedAssetsStruct(new FSelectedAssetsStruct);

	FString RootAssetPath = TEXT("/Game");
	FParse::Value(*Params, TEXT("RootAssetPath="), RootAssetPath);
	SelectedAssetsStruct->AddIncludedPackagePath(RootAssetPath);

	{
		FString ExcludedPackagePaths;
		if (FParse::Value(*Params, TEXT("ExcludePackagePaths="), ExcludedPackagePaths)) {
			TArray<FString> ExcludedPackagePathsArray;
			ExcludedPackagePaths.ParseIntoArray(ExcludedPackagePathsArray, TEXT(","));

			for (const FString& PackagePath : ExcludedPackagePathsArray) {
				SelectedAssetsStruct->AddExcludedPackagePath(PackagePath);
			}
		}
	}

	{
		FString ExcludedPackageNames;
		if (FParse::Value(*Params, TEXT("ExcludePackageNames="), ExcludedPackageNames)) {
			TArray<FString> ExcludedPackageNamesArray;
			ExcludedPackageNames.ParseIntoArray(ExcludedPackageNamesArray, TEXT(","));

			for (const FString& PackagePath : ExcludedPackageNamesArray) {
				SelectedAssetsStruct->AddExcludedPackageName(PackagePath);
			}
		}
	}

	TArray<FName> AssetClassWhitelist;
	
	for (UAssetTypeSerializer* Serializer : UAssetTypeSerializer::GetAvailableAssetSerializers()) {
		AssetClassWhitelist.Add(Serializer->GetAssetClass());
		Serializer->GetAdditionallyHandledAssetClasses(AssetClassWhitelist);
	}

	{
		FString UserDefinedAssetClassWhitelist;
		if (FParse::Value(*Params, TEXT("AssetClassWhitelist="), UserDefinedAssetClassWhitelist)) {
			TArray<FString> NewClassWhitelist;
			UserDefinedAssetClassWhitelist.ParseIntoArray(NewClassWhitelist, TEXT(","));

			AssetClassWhitelist.Empty();
			for (const FString& AssetClass : NewClassWhitelist) {
				AssetClassWhitelist.Add(*AssetClass);
			}
		}
	}

	for (const FName& AssetClass : AssetClassWhitelist) {
		SelectedAssetsStruct->AddAssetClassWhitelist(AssetClass);
	}

	
	FAssetDumpSettings DumpSettings{};
	FParse::Value(*Params, TEXT("PackagesPerTick="), DumpSettings.MaxPackagesToProcessInOneTick);
	DumpSettings.bForceSingleThread = !FParse::Param(*Params, TEXT("MultiThreaded"));
	DumpSettings.bExitOnFinish = FParse::Param(*Params, TEXT("ExitOnFinish"));

	{
		FString OverrideDumpRootPath;
		if (FParse::Value(*Params, TEXT("AssetDumpRootPath="), OverrideDumpRootPath)) {
			FPaths::NormalizeDirectoryName(OverrideDumpRootPath);
			DumpSettings.RootDumpDirectory = OverrideDumpRootPath;
		}
	}

	UE_LOG(LogAssetDumper, Log, TEXT("Gathering asset data under the path, this may take a while..."));
	SelectedAssetsStruct->GatherAssetsData();

	const TMap<FName, FAssetData>& AssetData = SelectedAssetsStruct->GetGatheredAssets();
	UE_LOG(LogAssetDumper, Log, TEXT("Asset data gathered successfully! Gathered %d assets for dumping"), AssetData.Num());
	
	FPaths::NormalizeDirectoryName(DumpSettings.RootDumpDirectory);
	
	FAssetDumpProcessor::StartAssetDump(DumpSettings, AssetData);
	UE_LOG(LogAssetDumper, Log, TEXT("Asset dump started successfully, game will shutdown on finish"));
}

void DumpAllGameAssets(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar) {
	Ar.Log(TEXT("Starting console-driven asset dumping, dumping all assets"));
	FAssetDumperCommands::DumpAllGameAssets(FString::Join(Args, TEXT(" ")));
}

void FAssetDumperCommands::FindUnknownAssetClasses(const FString& PackagePathFilter, TArray<FUnknownAssetClass>& OutUnknownAssetClasses) {
	TArray<FName> SupportedClasses;
	for (UAssetTypeSerializer* Serializer : UAssetTypeSerializer::GetAvailableAssetSerializers()) {
		SupportedClasses.Add(Serializer->GetAssetClass());
		Serializer->GetAdditionallyHandledAssetClasses(SupportedClasses);
	}
	FSelectedAssetsStruct::FindUnknownAssetClasses(PackagePathFilter, SupportedClasses, OutUnknownAssetClasses);
}

void PrintUnknownAssetClasses(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar) {
	TArray<FUnknownAssetClass> UnknownAssetClasses;
	const FString FilterAssetPath = FString::Join(Args, TEXT(" "));
	FAssetDumperCommands::FindUnknownAssetClasses(FilterAssetPath, UnknownAssetClasses);
	
	if (UnknownAssetClasses.Num() > 0) {
		Ar.Logf(TEXT("Unknown asset classes in asset registry under path %s: "), *FilterAssetPath);
		for (const FUnknownAssetClass& AssetClass : UnknownAssetClasses) {
			Ar.Logf(TEXT("%s (%d)"), *AssetClass.AssetClass.ToString(), AssetClass.FoundAssets.Num());

			for (int32 i = 0; i < FMath::Min(AssetClass.FoundAssets.Num(), 10); i++) {
				Ar.Logf(TEXT("  %s"), *AssetClass.FoundAssets[i].ToString());
			}
		}
	} else {
		Ar.Logf(TEXT("No unknown asset classes found in asset registry"));
	}
}

void FAssetDumperCommands::RescanAssetsOnDisk() {
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);
	UE_LOG(LogAssetDumper, Log, TEXT("Asset registry has been synchronized with the assets on disk"));
}
 
static FAutoConsoleCommand OpenAssetDumperCommand(
	TEXT("dumper.OpenAssetDumper"),
	TEXT("Opens an asset dumper GUI"),
	FConsoleCommandDelegate::CreateStatic(&FAssetDumperCommands::OpenAssetDumperUI));

static FAutoConsoleCommand OpenAssetDumperConsoleCommand(
	TEXT("dumper.OpenAssetDumperConsole"),
	TEXT("Opens an asset dumper console for the active dumping process"),
	FConsoleCommandDelegate::CreateStatic(&FAssetDumperCommands::OpenAssetDumperProgressConsole));

static FAutoConsoleCommand ScanPathsForAssetsSynchronously(
	TEXT("dumper.RescanAssetsOnDisk"),
	TEXT("Synchronizes asset registry state with the state of the assets on the disk"),
	FConsoleCommandDelegate::CreateStatic(&FAssetDumperCommands::RescanAssetsOnDisk));

static FAutoConsoleCommand DumpAllGameAssetsCommand(
	TEXT("dumper.DumpAllGameAssets"),
	TEXT("Dumps all assets existing in the game, for all supported asset types"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpAllGameAssets));

static FAutoConsoleCommand PrintUnknownAssetClassesCommand(
	TEXT("dumper.PrintUnknownAssetClasses"),
	TEXT("Prints a list of all unknown asset classes"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&PrintUnknownAssetClasses));
	
#undef LOCTEXT_NAMESPACE