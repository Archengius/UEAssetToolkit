#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Animation/Skeleton.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "SkeletonGenerator.generated.h"

struct FSkeletonCompareData {
protected:
	FReferenceSkeleton ReferenceSkeleton;
	TArray<EBoneTranslationRetargetingMode::Type> BoneTree;
	TArray<FVirtualBone> VirtualBones;
	TMap<FName, FReferencePose> AnimRetargetSources;
public:
	FSkeletonCompareData(const TSharedPtr<FJsonObject>& AssetData);
	
	bool IsSkeletonUpToDate(USkeleton* Skeleton) const;
	bool ApplySkeletonChanges(USkeleton* Skeleton) const;
};

UCLASS(MinimalAPI)
class USkeletonGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
public:
	virtual FName GetAssetClass() override;
};