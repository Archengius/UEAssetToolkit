#pragma once
#include "Modules/ModuleManager.h"

#ifndef WITH_CSS_ENGINE_PATCHES
#define WITH_CSS_ENGINE_PATCHES 0
#endif

#ifndef METHOD_PATCHING_SUPPORTED
#define METHOD_PATCHING_SUPPORTED 0
#endif

class ASSETDUMPER_API FAssetDumperModule : public FDefaultGameModuleImpl {
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
private:
	void EnableGlobalStaticMeshCPUAccess();
	void EnableFixTireConfigSerialization();
};