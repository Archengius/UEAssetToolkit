#include "AssetDumperModule.h"
#include "Toolkit/AssetDumping/AssetDumperCommands.h"

#if METHOD_PATCHING_SUPPORTED
#include "Patching/NativeHookManager.h"
#endif

DEFINE_LOG_CATEGORY(LogAssetDumper)

void FAssetDumperModule::StartupModule() {
	UE_LOG(LogAssetDumper, Log, TEXT("Starting up asset dumper plugin"));
	
	if (!GIsEditor) {
		EnableGlobalStaticMeshCPUAccess();
		EnableFixTireConfigSerialization();
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("DumpAllGameAssets"))) {
		UE_LOG(LogAssetDumper, Log, TEXT("Game asset dump required through the command line. Game will dump the assets and exit"));
		FAssetDumperCommands::DumpAllGameAssets(FCommandLine::Get());
	}
}

void FAssetDumperModule::ShutdownModule() {
}

void FAssetDumperModule::EnableGlobalStaticMeshCPUAccess() {
#if METHOD_PATCHING_SUPPORTED
	//setup hook for forcing all static mesh data to be CPU resident
	void* UObjectCDO = GetMutableDefault<UObject>();
	SUBSCRIBE_METHOD_EXPLICIT_VIRTUAL_AFTER(void(UObject::*)(FArchive&), UObject::Serialize, UObjectCDO, [](UObject* Object, FArchive& Ar) {
		if (Object->IsA<UStaticMesh>()) {
			CastChecked<UStaticMesh>(Object)->bAllowCPUAccess = true;
		}
	});

	//reload all existing static mesh packages to apply the patch
	TArray<UPackage*> PackagesToReload;

	for (TObjectIterator<UStaticMesh> It; It; ++It) {
		UStaticMesh* StaticMesh = *It;
		if (!StaticMesh->bAllowCPUAccess) {
			UPackage* OwnerPackage = StaticMesh->GetOutermost();
			//Only interested in non-transient FactoryGame packages
			if (OwnerPackage->GetName().StartsWith(TEXT("/Game/FactoryGame/"))) {
				if (!OwnerPackage->HasAnyFlags(RF_Transient)) {
					UE_LOG(LogAssetDumper, Log, TEXT("StaticMesh Package %s has been loaded before CPU access fixup application, attempting to reload"), *OwnerPackage->GetName());
					PackagesToReload.Add(OwnerPackage);	
				}
			}
		}
	}
	
	if (PackagesToReload.Num()) {
		UE_LOG(LogAssetDumper, Log, TEXT("Reloading %d StaticMesh packages for CPU access fixup"), PackagesToReload.Num());
		for (UPackage* Package : PackagesToReload) {
			ReloadPackage(Package, LOAD_None);
		}
	}
#endif
}

void FAssetDumperModule::EnableFixTireConfigSerialization() {
#if METHOD_PATCHING_SUPPORTED
	//disable that weird shit because it apparently crashes on garbage collection
	UClass* TireConfigClass = FindObject<UClass>(NULL, TEXT("/Script/PhysXVehicles.TireConfig"));
	check(TireConfigClass);
	SUBSCRIBE_METHOD_VIRTUAL(UObject::BeginDestroy, TireConfigClass->GetDefaultObject(), [TireConfigClass](auto& Scope, UObject* Object) {
		if (Object->IsA(TireConfigClass)) {
			Object->PostInitProperties();
		}
	});
#endif
}

IMPLEMENT_GAME_MODULE(FAssetDumperModule, AssetDumper);
