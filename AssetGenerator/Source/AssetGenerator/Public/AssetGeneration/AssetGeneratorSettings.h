#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AssetGeneratorSettings.generated.h"

UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "Asset Generator Settings"))
class ASSETGENERATOR_API UAssetGeneratorSettings : public UDeveloperSettings {
	GENERATED_BODY()
public:
	FORCEINLINE static const UAssetGeneratorSettings* Get() {
		return GetDefault<UAssetGeneratorSettings>();
	};
public:
	/** List of full package names to force no mip maps generation policy for */
	UPROPERTY(EditAnywhere, Config, Category = "Texture Generator")
	TArray<FString> PackagesToForceNoMipMaps;
};