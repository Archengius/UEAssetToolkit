#include "Toolkit/AssetDumping/AssetDumperCommands.h"
#include "AssetRegistryModule.h"
#include "Toolkit/AssetDumping/AssetDumperWidget.h"
#include "Toolkit/AssetDumping/AssetDumpConsoleWidget.h"
#include "Toolkit/AssetDumping/AssetRegistryViewWidget.h"
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Util/GameEditorHelper.h"

#define LOCTEXT_NAMESPACE "AssetDumper"

static TAutoConsoleVariable<int32> AssetsToProcessPerTick(
	TEXT("dumper.AssetsToProcessPerTick"), 16,
	TEXT("Amount of assets to process per tick in automatic asset dumping using the console command or command line switch"));

static TAutoConsoleVariable<bool> ForceSingleThreaded(
	TEXT("dumper.ForceSingleThreaded"), true,
	TEXT("Whenever to force a single-threaded dumping in automatic mode using the console command or command line switch"));

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

void FAssetDumperCommands::DumpAllGameAssets() {
	const TSharedRef<FSelectedAssetsStruct> SelectedAssetsStruct(new FSelectedAssetsStruct);

	SelectedAssetsStruct->AddIncludedPackagePath(TEXT("/"));
	for (UAssetTypeSerializer* Serializer : UAssetTypeSerializer::GetAvailableAssetSerializers()) {
		SelectedAssetsStruct->AddAssetClassWhitelist(Serializer->GetAssetClass());
		TArray<FName> AdditionalClassNames;
		Serializer->GetAdditionallyHandledAssetClasses(AdditionalClassNames);
		for (const FName& AssetClass : AdditionalClassNames) {
			SelectedAssetsStruct->AddAssetClassWhitelist(AssetClass);
		}
	}

	UE_LOG(LogAssetDumper, Log, TEXT("Gathering asset data under / path, this may take a while..."));
	SelectedAssetsStruct->GatherAssetsData();

	const TMap<FName, FAssetData>& AssetData = SelectedAssetsStruct->GetGatheredAssets();
	UE_LOG(LogAssetDumper, Log, TEXT("Asset data gathered successfully! Gathered %d assets for dumping"), AssetData.Num());

	FAssetDumpSettings DumpSettings{};
	DumpSettings.MaxPackagesToProcessInOneTick = AssetsToProcessPerTick.GetValueOnGameThread();
	DumpSettings.bForceSingleThread = true;
	
	DumpSettings.bExitOnFinish = true;
	
	FPaths::NormalizeDirectoryName(DumpSettings.RootDumpDirectory);
	
	FAssetDumpProcessor::StartAssetDump(DumpSettings, AssetData);
	UE_LOG(LogAssetDumper, Log, TEXT("Asset dump started successfully, game will shutdown on finish"));
}


void DumpAllGameAssets(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar) {
	Ar.Log(TEXT("Starting console-driven asset dumping, dumping all assets"));
	FAssetDumperCommands::DumpAllGameAssets();
}

void FAssetDumperCommands::FindUnknownAssetClasses(TArray<FName>& OutUnknownAssetClasses) {
	TArray<FName> SupportedClasses;
	for (UAssetTypeSerializer* Serializer : UAssetTypeSerializer::GetAvailableAssetSerializers()) {
		SupportedClasses.Add(Serializer->GetAssetClass());
		Serializer->GetAdditionallyHandledAssetClasses(SupportedClasses);
	}
	FSelectedAssetsStruct::FindUnknownAssetClasses(SupportedClasses, OutUnknownAssetClasses);
}

void PrintUnknownAssetClasses(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar) {
	TArray<FName> UnknownAssetClasses;
	FAssetDumperCommands::FindUnknownAssetClasses(UnknownAssetClasses);
	
	if (UnknownAssetClasses.Num() > 0) {
		Ar.Log(TEXT("Unknown asset classes in asset registry: "));
		for (const FName& AssetClass : UnknownAssetClasses) {
			Ar.Logf(TEXT(" - '%s'"), *AssetClass.ToString());
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