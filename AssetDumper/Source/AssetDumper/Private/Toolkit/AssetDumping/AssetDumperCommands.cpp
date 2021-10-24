#include "AssetRegistryModule.h"
#include "Toolkit/AssetDumping/AssetDumperWidget.h"
#include "Toolkit/AssetDumping/AssetDumpConsoleWidget.h"
#include "Toolkit/AssetDumping/AssetRegistryViewWidget.h"
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Util/GameEditorHelper.h"

#define LOCTEXT_NAMESPACE "AssetDumper"

void OpenAssetDumperUI() {
	TSharedRef<SWindow> Window = SNew(SWindow)
						.Title(LOCTEXT("AssetDumper_Title", "Asset Dumper Settings"))
						.MinWidth(800).MinHeight(600)
						.AutoCenter(EAutoCenter::PreferredWorkArea);
	Window->SetContent(SNew(SAssetDumperWidget));

	const TSharedRef<SWindow> ParentWindow = FGameEditorHelper::GetMainWindow().ToSharedRef();
	FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow, true);
}

void OpenAssetDumperProgressConsole() {
	const TSharedPtr<FAssetDumpProcessor> ActiveProcessor = FAssetDumpProcessor::GetActiveDumpProcessor();
	if (ActiveProcessor.IsValid()) {
		SAssetDumpConsoleWidget::CreateForAssetDumper(ActiveProcessor.ToSharedRef());
	}
}

void DumpAllGameAssets(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar) {
	Ar.Log(TEXT("Starting console-driven asset dumping, dumping all assets"));
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

	Ar.Log(TEXT("Gathering asset data under / path, this may take a while..."));
	SelectedAssetsStruct->GatherAssetsData();

	const TMap<FName, FAssetData>& AssetData = SelectedAssetsStruct->GetGatheredAssets();
	Ar.Logf(TEXT("Asset data gathered successfully! Gathered %d assets for dumping"), AssetData.Num());

	FAssetDumpSettings DumpSettings{};
	DumpSettings.bExitOnFinish = true;
	FAssetDumpProcessor::StartAssetDump(DumpSettings, AssetData);
	Ar.Log(TEXT("Asset dump started successfully, game will shutdown on finish"));
}

void PrintUnknownAssetClasses(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar) {
	TArray<FName> SupportedClasses;
	for (UAssetTypeSerializer* Serializer : UAssetTypeSerializer::GetAvailableAssetSerializers()) {
		SupportedClasses.Add(Serializer->GetAssetClass());
		Serializer->GetAdditionallyHandledAssetClasses(SupportedClasses);
	}

	TArray<FName> UnknownAssetClasses;
	FSelectedAssetsStruct::FindUnknownAssetClasses(SupportedClasses, UnknownAssetClasses);
	if (UnknownAssetClasses.Num() > 0) {
		Ar.Log(TEXT("Unknown asset classes in asset registry: "));
		for (const FName& AssetClass : UnknownAssetClasses) {
			Ar.Logf(TEXT(" - '%s'"), *AssetClass.ToString());
		}
	} else {
		Ar.Logf(TEXT("No unknown asset classes found in asset registry"));
	}
}

void SearchAllAssets() {
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);
}
 
static FAutoConsoleCommand OpenAssetDumperCommand(
	TEXT("OpenAssetDumper"),
	TEXT("Opens an asset dumper GUI"),
	FConsoleCommandDelegate::CreateStatic(&OpenAssetDumperUI));

static FAutoConsoleCommand OpenAssetDumperConsoleCommand(
	TEXT("OpenAssetDumperConsole"),
	TEXT("Opens an asset dumper console for the active dumping process"),
	FConsoleCommandDelegate::CreateStatic(&OpenAssetDumperProgressConsole));

static FAutoConsoleCommand ScanPathsForAssetsSynchronously(
	TEXT("SearchAllAssets"),
	TEXT("Searches for new assets on the disk"),
	FConsoleCommandDelegate::CreateStatic(&SearchAllAssets));

static FAutoConsoleCommand DumpAllGameAssetsCommand(
	TEXT("DumpAllGameAssets"),
	TEXT("Dumps all assets existing in the game, for all supported asset types"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpAllGameAssets));

static FAutoConsoleCommand PrintUnknownAssetClassesCommand(
	TEXT("PrintUnknownAssetClasses"),
	TEXT("Prints a list of all unknown asset classes"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&PrintUnknownAssetClasses));