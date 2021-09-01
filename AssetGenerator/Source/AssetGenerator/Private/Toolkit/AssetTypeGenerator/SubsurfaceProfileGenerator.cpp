#include "Toolkit/AssetTypeGenerator/SubsurfaceProfileGenerator.h"
#include "Engine/SubsurfaceProfile.h"

UClass* USubsurfaceProfileGenerator::GetAssetObjectClass() const {
	return USubsurfaceProfile::StaticClass();
}

FName USubsurfaceProfileGenerator::GetAssetClass() {
	return USubsurfaceProfile::StaticClass()->GetFName();
}
