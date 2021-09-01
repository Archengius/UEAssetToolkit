#include "Toolkit/AssetTypeGenerator/LandscapeGrassTypeGenerator.h"
#include "LandscapeGrassType.h"

UClass* ULandscapeGrassTypeGenerator::GetAssetObjectClass() const {
	return ULandscapeGrassType::StaticClass();
}

FName ULandscapeGrassTypeGenerator::GetAssetClass() {
	return ULandscapeGrassType::StaticClass()->GetFName();
}
