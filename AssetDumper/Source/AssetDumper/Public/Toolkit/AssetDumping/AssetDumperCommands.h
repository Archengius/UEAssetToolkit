#pragma once
#include "CoreMinimal.h"

class ASSETDUMPER_API FAssetDumperCommands {
public:
	/** Opens the UI for performing asset dumping manually */
	static void OpenAssetDumperUI();

	/** Opens the asset dumper progress console window */
	static void OpenAssetDumperProgressConsole();

	/** Dumps all of the game assets into the specified output directory */
	static void DumpAllGameAssets();

	/** Retrieves a list of classes not supported by the asset dumper */
	static void FindUnknownAssetClasses(TArray<FName>& OutUnknownAssetClasses);

	/** Synchronizes asset registry with the state of the assets on disk */
	static void RescanAssetsOnDisk();
};