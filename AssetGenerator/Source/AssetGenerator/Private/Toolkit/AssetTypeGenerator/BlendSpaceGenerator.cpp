#include "Toolkit/AssetTypeGenerator/BlendSpaceGenerator.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/BlendSpaceBase.h"
#include "Dom/JsonObject.h"

UClass* UBlendSpaceGenerator::GetAssetObjectClass() const {
	const FString AssetClassPath = GetAssetData()->GetStringField(TEXT("AssetClass"));
	return FindObjectChecked<UClass>(NULL, *AssetClassPath);
}

void UBlendSpaceGenerator::GetAdditionallyHandledAssetClasses(TArray<FName>& OutExtraAssetClasses) {
	OutExtraAssetClasses.Add(UBlendSpace::StaticClass()->GetFName());
	OutExtraAssetClasses.Add(UBlendSpace1D::StaticClass()->GetFName());
	OutExtraAssetClasses.Add(UAimOffsetBlendSpace::StaticClass()->GetFName());
	OutExtraAssetClasses.Add(UAimOffsetBlendSpace1D::StaticClass()->GetFName());
}

FName UBlendSpaceGenerator::GetAssetClass() {
	return UBlendSpaceBase::StaticClass()->GetFName();
}