#include "Toolkit/AssetTypeGenerator/AnimMontageGenerator.h"
#include "Animation/AnimMontage.h"

UClass* UAnimMontageGenerator::GetAssetObjectClass() const {
	return UAnimMontage::StaticClass();
}

FName UAnimMontageGenerator::GetAssetClass() {
	return UAnimMontage::StaticClass()->GetFName();
}
