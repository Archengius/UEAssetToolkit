#include "Toolkit/AssetTypeGenerator/PhysicalMaterialGenerator.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UClass* UPhysicalMaterialGenerator::GetAssetObjectClass() const {
	return UPhysicalMaterial::StaticClass();
}

FName UPhysicalMaterialGenerator::GetAssetClass() {
	return UPhysicalMaterial::StaticClass()->GetFName();
}
