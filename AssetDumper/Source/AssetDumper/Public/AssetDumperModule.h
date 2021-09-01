#pragma once
#include "Modules/ModuleManager.h"

class ASSETDUMPER_API FAssetDumperModule : public FDefaultGameModuleImpl {
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};