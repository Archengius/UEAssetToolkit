#include "AssetGeneratorModule.h"
#include "ContentBrowserModule.h"
#include "WorkspaceMenuStructure.h"
#include "Toolkit/AssetGeneration/AssetGeneratorWidget.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "AssetGenerator"

const FName FAssetGeneratorModule::AssetGeneratorTabName = TEXT("AssetGenerator");

void FAssetGeneratorModule::StartupModule() {
	const TSharedPtr<FWorkspaceItem> WorkspaceGroup = WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory();
	const TSharedRef<FGlobalTabmanager> TabManager = FGlobalTabmanager::Get();
	
	FTabSpawnerEntry& SpawnerEntry = TabManager->RegisterNomadTabSpawner(AssetGeneratorTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
			return SNew(SDockTab)
			.TabRole(NomadTab)[
				SNew(SAssetGeneratorWidget)
			];
	}))
	.SetDisplayName(LOCTEXT("AssetGenerator_TabName", "Asset Generator"))
	.SetTooltipText(LOCTEXT("AssetGenerator_TabTooltip", "Allows generating assets from the game dump files"));

	if (WorkspaceGroup.IsValid()) {
		SpawnerEntry.SetGroup(WorkspaceGroup.ToSharedRef());
	}
}

void FAssetGeneratorModule::ShutdownModule() {
	const TSharedRef<FGlobalTabmanager> TabManager = FGlobalTabmanager::Get();
	
	TabManager->UnregisterNomadTabSpawner(AssetGeneratorTabName);
}

IMPLEMENT_GAME_MODULE(FAssetGeneratorModule, AssetGenerator);
