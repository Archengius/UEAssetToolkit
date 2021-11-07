#include "Toolkit/AssetTypeGenerator/CameraAnimGenerator.h"
#include "Camera/CameraAnim.h"

UClass* UCameraAnimGenerator::GetAssetObjectClass() const {
	return UCameraAnim::StaticClass();
}

FName UCameraAnimGenerator::GetAssetClass() {
	return UCameraAnim::StaticClass()->GetFName();
}
