#include "AssetDumperModule.h"
#include "Patching/NativeHookManager.h"
#include "Toolkit/AssetDumping/AssetDumpProcessor.h"
#include "Toolkit/AssetTypes/StaticMeshAssetSerializer.h"

void FAssetDumperModule::StartupModule() {
	UE_LOG(LogAssetDumper, Log, TEXT("Starting up asset dumper plugin"));
	
	//eats up considerable amount of RAM and also hooks in the very hot place, but we're fine since we are a development tool only
	UStaticMeshAssetSerializer::EnableGlobalStaticMeshCPUAccess();

	//disable that weird shit because it apparently crashes on garbage collection
	UClass* TireConfigClass = FindObject<UClass>(NULL, TEXT("/Script/PhysXVehicles.TireConfig"));
	check(TireConfigClass);
	SUBSCRIBE_METHOD_VIRTUAL(UObject::BeginDestroy, TireConfigClass->GetDefaultObject(), [TireConfigClass](auto& Scope, UObject* Object) {
		if (Object->IsA(TireConfigClass)) {
			Object->PostInitProperties();
		}
	});
}

void FAssetDumperModule::ShutdownModule() {
}

IMPLEMENT_GAME_MODULE(FAssetDumperModule, AssetDumper);
