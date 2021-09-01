#pragma once
#include "Modules/ModuleManager.h"

class ASSETGENERATOR_API FAssetGeneratorModule : public FDefaultGameModuleImpl {
public:
	static const FName AssetGeneratorTabName;
	
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};