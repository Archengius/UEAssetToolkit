#include "Toolkit/AssetTypeGenerator/SkeletonGenerator.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"

FSkeletonCompareData::FSkeletonCompareData(const TSharedPtr<FJsonObject>& AssetData) {
	const TSharedPtr<FJsonObject> ReferenceSkeleton = AssetData->GetObjectField(TEXT("ReferenceSkeleton"));
	const TArray<TSharedPtr<FJsonValue>>& Bones = ReferenceSkeleton->GetArrayField(TEXT("Bones"));

	FReferenceSkeletonModifier ReferenceSkeletonModifier(this->ReferenceSkeleton, NULL);
	
	for (int32 i = 0; i < Bones.Num(); i++) {
		const TSharedPtr<FJsonObject> BoneElement = Bones[i]->AsObject();

		const FString BoneName = BoneElement->GetStringField(TEXT("Name"));
		const int32 ParentBoneIndex = BoneElement->GetIntegerField(TEXT("ParentIndex"));

		FTransform BonePose;
		BonePose.InitFromString(BoneElement->GetStringField(TEXT("Pose")));
		
		ReferenceSkeletonModifier.Add(FMeshBoneInfo(*BoneName, BoneName, ParentBoneIndex), BonePose);
	}

	const TArray<TSharedPtr<FJsonValue>>& BoneTree = AssetData->GetArrayField(TEXT("BoneTree"));
	
	for (int32 i = 0; i < BoneTree.Num(); i++) {
		const int32 RetargetingModeValue = BoneTree[i]->AsNumber();
		this->BoneTree.Add((EBoneTranslationRetargetingMode::Type) RetargetingModeValue);
	}

	const TArray<TSharedPtr<FJsonValue>>& VirtualBones = AssetData->GetArrayField(TEXT("VirtualBones"));

	for (int32 i = 0; i < VirtualBones.Num(); i++) {
		const TSharedPtr<FJsonObject> VirtualBoneNode = VirtualBones[i]->AsObject();

		const FString SourceBoneName = VirtualBoneNode->GetStringField(TEXT("SourceBoneName"));
		const FString TargetBoneName = VirtualBoneNode->GetStringField(TEXT("TargetBoneName"));
		
		this->VirtualBones.Add(FVirtualBone(*SourceBoneName, *TargetBoneName));
	}

	const TArray<TSharedPtr<FJsonValue>>& AnimRetargetSources = AssetData->GetArrayField(TEXT("AnimRetargetSources"));

	for (int32 i = 0; i < AnimRetargetSources.Num(); i++) {
		const TSharedPtr<FJsonObject> ReferencePoseObject = AnimRetargetSources[i]->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& ReferencePoseArray = ReferencePoseObject->GetArrayField(TEXT("ReferencePose"));
		
		FReferencePose ReferencePose{};
		ReferencePose.PoseName = FName(*ReferencePoseObject->GetStringField(TEXT("PoseName")));
		
		for (int32 j = 0; j < ReferencePoseArray.Num(); j++) {
			FTransform Transform;
			Transform.InitFromString(ReferencePoseArray[j]->AsString());
			ReferencePose.ReferencePose.Add(Transform);
		}
		this->AnimRetargetSources.Add(ReferencePose.PoseName, ReferencePose);
	}
}

bool FSkeletonCompareData::IsSkeletonUpToDate(USkeleton* Skeleton) const {
	//MAKE SURE WE HAVE ALL BONES AND MATCHING POSES IN REFERENCE SKELETON
	FReferenceSkeleton& ExistingReferenceSkeleton = const_cast<FReferenceSkeleton&>(Skeleton->GetReferenceSkeleton());
	FReferenceSkeletonModifier ReferenceSkeletonModifier(ExistingReferenceSkeleton, Skeleton);

	for (int32 RawBoneIndex = 0; RawBoneIndex < ReferenceSkeleton.GetRawBoneNum(); RawBoneIndex++) {
		const FMeshBoneInfo& NewBoneInfo = ReferenceSkeleton.GetRawRefBoneInfo()[RawBoneIndex];
		const FTransform& NewBonePose = ReferenceSkeleton.GetRawRefBonePose()[RawBoneIndex];

		const int32 ExistingBoneIndex = ExistingReferenceSkeleton.FindRawBoneIndex(NewBoneInfo.Name);
		
		if (ExistingBoneIndex == INDEX_NONE) {
			return false;
		}
		const FTransform& OldBonePose = ExistingReferenceSkeleton.GetRawRefBonePose()[ExistingBoneIndex];
			
		if (!OldBonePose.Equals(NewBonePose)) {
			return false;
		}
	}

	//MAKE SURE BONE TREE IS UP TO DATE WITH THE DUMP
	for (int32 BoneIndex = 0; BoneIndex < BoneTree.Num(); BoneIndex++) {
		const EBoneTranslationRetargetingMode::Type TranslationRetargetingMode = BoneTree[BoneIndex];
		
		//find a name correlating with the provided bone
		const FName BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
		check(BoneName != NAME_None);
		
		//Find bone index in the existing reference skeleton matching the provided bone name
		const int32 TargetBoneIndex = ExistingReferenceSkeleton.FindRawBoneIndex(BoneName);
		check(TargetBoneIndex != INDEX_NONE);

		const EBoneTranslationRetargetingMode::Type ExistingType = Skeleton->GetBoneTranslationRetargetingMode(TargetBoneIndex);
		if (ExistingType != TranslationRetargetingMode) {
			return false;
		}
	}
	
	//MAKE SURE WE HAVE VIRTUAL BONES MATCHING THE DUMP
	TArray<FName> VirtualBonesToRemove;
	for (const FVirtualBone& ExistingVirtualBone : Skeleton->GetVirtualBones()) {
		VirtualBonesToRemove.Add(ExistingVirtualBone.VirtualBoneName);
	}
	
	for (const FVirtualBone& VirtualBone : VirtualBones) {
		if (!VirtualBonesToRemove.Contains(VirtualBone.VirtualBoneName)) {
			return false;
		}
		VirtualBonesToRemove.Remove(VirtualBone.VirtualBoneName);
	}
	if (VirtualBonesToRemove.Num()) {
		return false;
	}

	//MAKE SURE RETARGET SOURCES ARE UP TO DATE
	for (const TPair<FName, FReferencePose>& NewReferencePose : AnimRetargetSources) {
		const FReferencePose& NewPose = NewReferencePose.Value;
		
		if (!Skeleton->AnimRetargetSources.Contains(NewPose.PoseName)) {
			return false;
		}
		
		FReferencePose ExistingPose = Skeleton->AnimRetargetSources.FindChecked(NewPose.PoseName);

		for (int32 i = 0; i < ExistingPose.ReferencePose.Num(); i++) {
			const FName BoneName = ExistingReferenceSkeleton.GetBoneName(i);
				
			const int32 OriginalBoneIndex = ReferenceSkeleton.FindRawBoneIndex(BoneName);
			check(OriginalBoneIndex != INDEX_NONE);

			if (!ExistingPose.ReferencePose[i].Equals(NewPose.ReferencePose[OriginalBoneIndex])) {
				return false;
			}
		}
	}
	return true;
}

bool FSkeletonCompareData::ApplySkeletonChanges(USkeleton* Skeleton) const {
	bool bModifiedSkeleton = false;

	//MODIFY REFERENCE SKELETON FIRST BY CHANGING BONE POSES/ADDING NEW BONES
	FReferenceSkeleton& ExistingReferenceSkeleton = const_cast<FReferenceSkeleton&>(Skeleton->GetReferenceSkeleton());
	FReferenceSkeletonModifier ReferenceSkeletonModifier(ExistingReferenceSkeleton, Skeleton);

	for (int32 RawBoneIndex = 0; RawBoneIndex < ReferenceSkeleton.GetRawBoneNum(); RawBoneIndex++) {
		const FMeshBoneInfo& NewBoneInfo = ReferenceSkeleton.GetRawRefBoneInfo()[RawBoneIndex];
		const FTransform& NewBonePose = ReferenceSkeleton.GetRawRefBonePose()[RawBoneIndex];

		const int32 ExistingBoneIndex = ExistingReferenceSkeleton.FindRawBoneIndex(NewBoneInfo.Name);
		
		if (ExistingBoneIndex == INDEX_NONE) {
			int32 RemappedParentBoneIndex = INDEX_NONE;

			if (NewBoneInfo.ParentIndex != INDEX_NONE) {
				const FMeshBoneInfo& ParentBoneInfo = ReferenceSkeleton.GetRawRefBoneInfo()[NewBoneInfo.ParentIndex];
				
				RemappedParentBoneIndex = ExistingReferenceSkeleton.FindRawBoneIndex(ParentBoneInfo.Name);
				check(RemappedParentBoneIndex != INDEX_NONE);
			}

			const FMeshBoneInfo RemappedBoneInfo(NewBoneInfo.Name, NewBoneInfo.Name.ToString(), RemappedParentBoneIndex);
			ReferenceSkeletonModifier.Add(RemappedBoneInfo, NewBonePose);
			bModifiedSkeleton = true;
			
		} else {
			const FTransform& OldBonePose = ExistingReferenceSkeleton.GetRawRefBonePose()[ExistingBoneIndex];
			
			if (!OldBonePose.Equals(NewBonePose)) {
				ReferenceSkeletonModifier.UpdateRefPoseTransform(ExistingBoneIndex, NewBonePose);
				bModifiedSkeleton = true;
			}
		}
	}

	//MAKE SURE BONE TREE HAS THE CORRECT SIZE
	FProperty* BoneTreeProperty = USkeleton::StaticClass()->FindPropertyByName(TEXT("BoneTree"));
	TArray<FBoneNode>* ExistingBoneTree = BoneTreeProperty->ContainerPtrToValuePtr<TArray<FBoneNode>>(Skeleton);
	
	if (ExistingBoneTree->Num() != BoneTree.Num()) {
		ExistingBoneTree->Empty();
		ExistingBoneTree->AddZeroed(BoneTree.Num());
	}
	
	//APPLY BONE TREE MODIFICATIONS AFTER WE HAVE CHANGED REFERENCE SKELETON
	for (int32 BoneIndex = 0; BoneIndex < BoneTree.Num(); BoneIndex++) {
		const EBoneTranslationRetargetingMode::Type TranslationRetargetingMode = BoneTree[BoneIndex];
		
		//find a name correlating with the provided bone
		const FName BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
		check(BoneName != NAME_None);
		
		//Find bone index in the existing reference skeleton matching the provided bone name
		const int32 TargetBoneIndex = ExistingReferenceSkeleton.FindRawBoneIndex(BoneName);
		check(TargetBoneIndex != INDEX_NONE);
		
		const EBoneTranslationRetargetingMode::Type ExistingType = Skeleton->GetBoneTranslationRetargetingMode(TargetBoneIndex);
		
		if (ExistingType != TranslationRetargetingMode) {
			Skeleton->SetBoneTranslationRetargetingMode(TargetBoneIndex, TranslationRetargetingMode, false);
			bModifiedSkeleton = true;
		}
	}
	
	//APPLY VIRTUAL BONE MODIFICATIONS NOW
	TArray<FName> VirtualBonesToRemove;
	for (const FVirtualBone& ExistingVirtualBone : Skeleton->GetVirtualBones()) {
		VirtualBonesToRemove.Add(ExistingVirtualBone.VirtualBoneName);
	}
	
	for (const FVirtualBone& VirtualBone : VirtualBones) {
		Skeleton->AddNewVirtualBone(VirtualBone.SourceBoneName, VirtualBone.TargetBoneName);
		VirtualBonesToRemove.Remove(VirtualBone.VirtualBoneName);
	}
	
	if (VirtualBonesToRemove.Num()) {
		Skeleton->RemoveVirtualBones(VirtualBonesToRemove);
	}

	//REBUILD REF SKELETON TO MAKE SURE FinalRefBoneInfo IS UP TO DATE
	ExistingReferenceSkeleton.RebuildRefSkeleton(Skeleton, true);

	//ADD NEW RETARGET SOURCES AND MAKE SURE EXISTING ONES ARE UP TO DATE
	for (const TPair<FName, FReferencePose>& NewReferencePose : AnimRetargetSources) {
		const FReferencePose& NewPose = NewReferencePose.Value;
		
		if (!Skeleton->AnimRetargetSources.Contains(NewPose.PoseName)) {
			FReferencePose RetargetedReferencePose{};
			RetargetedReferencePose.PoseName = NewPose.PoseName;
			RetargetedReferencePose.ReferencePose.AddDefaulted(ExistingReferenceSkeleton.GetRawBoneNum());

			for (int32 i = 0; i < RetargetedReferencePose.ReferencePose.Num(); i++) {
				const FName BoneName = ExistingReferenceSkeleton.GetBoneName(i);
				
				const int32 OriginalBoneIndex = ReferenceSkeleton.FindRawBoneIndex(BoneName);
				check(OriginalBoneIndex != INDEX_NONE);
				RetargetedReferencePose.ReferencePose[i] = NewPose.ReferencePose[OriginalBoneIndex];
			}

			Skeleton->AnimRetargetSources.Add(RetargetedReferencePose.PoseName, RetargetedReferencePose);
			bModifiedSkeleton = true;
		} else {
			FReferencePose& ExistingPose = Skeleton->AnimRetargetSources.FindChecked(NewPose.PoseName);

			for (int32 i = 0; i < ExistingPose.ReferencePose.Num(); i++) {
				const FName BoneName = ExistingReferenceSkeleton.GetBoneName(i);
				
				const int32 OriginalBoneIndex = ReferenceSkeleton.FindRawBoneIndex(BoneName);
				check(OriginalBoneIndex != INDEX_NONE);

				if (!ExistingPose.ReferencePose[i].Equals(NewPose.ReferencePose[OriginalBoneIndex])) {
					ExistingPose.ReferencePose[i] = NewPose.ReferencePose[OriginalBoneIndex];
					bModifiedSkeleton = true;
				}
			}
		}
	}

	//MARK PACKAGE DIRTY AND CLEAR CACHED DATA

	if (bModifiedSkeleton) {
		Skeleton->ClearCacheData();
		Skeleton->MarkPackageDirty();
	}
	return bModifiedSkeleton;
}

void USkeletonGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	USkeleton* NewAssetObject = NewObject<USkeleton>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewAssetObject);

	const FSkeletonCompareData SkeletonCompareData(GetAssetData());
	SkeletonCompareData.ApplySkeletonChanges(NewAssetObject);

	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));
	
	GetObjectSerializer()->DeserializeObjectProperties(AssetObjectProperties.ToSharedRef(), NewAssetObject);
	MarkAssetChanged();
}

void USkeletonGenerator::OnExistingPackageLoaded() {
	USkeleton* ExistingAssetObject = GetAsset<USkeleton>();
	const FSkeletonCompareData SkeletonCompareData(GetAssetData());

	if (!SkeletonCompareData.IsSkeletonUpToDate(ExistingAssetObject)) {
		UE_LOG(LogAssetGenerator, Display, TEXT("Skeleton %s Structure is not up to date"), *ExistingAssetObject->GetPathName());
		SkeletonCompareData.ApplySkeletonChanges(ExistingAssetObject);
		MarkAssetChanged();
	}

	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));
	
	if (!GetObjectSerializer()->AreObjectPropertiesUpToDate(AssetObjectProperties.ToSharedRef(), ExistingAssetObject)) {
		UE_LOG(LogAssetGenerator, Display, TEXT("Skeleton %s Properties are not up to date"), *ExistingAssetObject->GetPathName());
		GetObjectSerializer()->DeserializeObjectProperties(AssetObjectProperties.ToSharedRef(), ExistingAssetObject);
		MarkAssetChanged();
	}
}

FName USkeletonGenerator::GetAssetClass() {
	return USkeleton::StaticClass()->GetFName();
}
