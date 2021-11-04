#pragma once
#include "CoreMinimal.h"

struct FUnknownAssetClass;

class ASSETDUMPER_API FAssetDumperCommands {
public:
	/** Opens the UI for performing asset dumping manually */
	static void OpenAssetDumperUI();

	/** Opens the asset dumper progress console window */
	static void OpenAssetDumperProgressConsole();

	/** Dumps all of the game assets into the specified output directory */
	static void DumpAllGameAssets(const FString& Params);

	/** Retrieves a list of classes not supported by the asset dumper */
	static void FindUnknownAssetClasses(const FString& PackagePathFilter, TArray<FUnknownAssetClass>& OutUnknownAssetClasses);

	/** Synchronizes asset registry with the state of the assets on disk */
	static void RescanAssetsOnDisk();
};