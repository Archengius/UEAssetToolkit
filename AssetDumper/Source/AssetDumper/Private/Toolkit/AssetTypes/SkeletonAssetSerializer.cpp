#include "Toolkit/AssetTypes/SkeletonAssetSerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/SkeletalMeshAssetSerializer.h"
#include "Animation/Skeleton.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void USkeletonAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(USkeleton)

	DISABLE_SERIALIZATION_RAW(USkeleton, "VirtualBoneGuid");
	DISABLE_SERIALIZATION_RAW(USkeleton, "BoneTree");
	DISABLE_SERIALIZATION_RAW(USkeleton, "VirtualBones");
    
    //Serialize reference skeleton object
    const TSharedPtr<FJsonObject> ReferenceSkeleton = MakeShareable(new FJsonObject());
    USkeletalMeshAssetSerializer::SerializeReferenceSkeleton(Asset->GetReferenceSkeleton(), ReferenceSkeleton);
    Data->SetObjectField(TEXT("ReferenceSkeleton"), ReferenceSkeleton);

	//Serialize virtual bones
	TArray<TSharedPtr<FJsonValue>> VirtualBones;
	
	for (const FVirtualBone& VirtualBone : Asset->GetVirtualBones()) {
		const TSharedPtr<FJsonObject> VirtualBoneNode = MakeShareable(new FJsonObject());

		VirtualBoneNode->SetStringField(TEXT("SourceBoneName"), VirtualBone.SourceBoneName.ToString());
		VirtualBoneNode->SetStringField(TEXT("TargetBoneName"), VirtualBone.TargetBoneName.ToString());

		VirtualBones.Add(MakeShareable(new FJsonValueObject(VirtualBoneNode)));
	}
	Data->SetArrayField(TEXT("VirtualBones"), VirtualBones);

	//Serialize bone tree translation retargeting modes
	TArray<TSharedPtr<FJsonValue>> BoneTree;

	for (int32 i = 0; i < Asset->GetReferenceSkeleton().GetRawBoneNum(); i++) {
		const EBoneTranslationRetargetingMode::Type RetargetingType = Asset->GetBoneTranslationRetargetingMode(i);
		BoneTree.Add(MakeShareable(new FJsonValueNumber((int32) RetargetingType)));
	}
	Data->SetArrayField(TEXT("BoneTree"), BoneTree);

    //Serialize animation retarget sources
	TArray<TSharedPtr<FJsonValue>> AnimRetargetSources;

	for (const TPair<FName, FReferencePose>& Pair : Asset->AnimRetargetSources) {
		TSharedRef<FJsonObject> Value = MakeShareable(new FJsonObject());
		Value->SetStringField(TEXT("PoseName"), Pair.Value.PoseName.ToString());

		TArray<TSharedPtr<FJsonValue>> ReferencePose;
		for (const FTransform& Transform : Pair.Value.ReferencePose) {
			ReferencePose.Add(MakeShareable(new FJsonValueString(Transform.ToString())));
		}
		Value->SetArrayField(TEXT("ReferencePose"), ReferencePose);
		AnimRetargetSources.Add(MakeShareable(new FJsonValueObject(Value)));
	}
    Data->SetArrayField(TEXT("AnimRetargetSources"), AnimRetargetSources);

    //Serialize normal asset data
    SERIALIZE_ASSET_OBJECT

    //Serialize skeleton itself into the fbx file
    const FString OutFbxFilename = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
    FString OutErrorMessage;
    const bool bSuccess = FFbxMeshExporter::ExportSkeletonIntoFbxFile(Asset, OutFbxFilename, false, &OutErrorMessage);
    checkf(bSuccess, TEXT("Failed to export skeleton %s: %s"), *Asset->GetPathName(), *OutErrorMessage);
    
    END_ASSET_SERIALIZATION
}

void USkeletonAssetSerializer::SerializeSmartNameContainer(const FSmartNameContainer& Container, TSharedPtr<FJsonObject> OutObject) {
    //Serialize smart name mappings
    TArray<TSharedPtr<FJsonValue>> NameMappings;

	TArray<FName> MappingNames { USkeleton::AnimCurveMappingName, USkeleton::AnimTrackCurveMappingName };
	
    for (FName SmartNameId : MappingNames) {
        const FSmartNameMapping* Mapping = Container.GetContainer(SmartNameId);
    	if (Mapping == NULL) {
    		continue;
    	}
        
        TSharedPtr<FJsonObject> NameMapping = MakeShareable(new FJsonObject());
        NameMapping->SetStringField(TEXT("Name"), SmartNameId.ToString());
        
        TArray<FName> MetaDataKeys;
        Mapping->FillUIDToNameArray(MetaDataKeys);

        TArray<TSharedPtr<FJsonValue>> CurveMetaDataMap;
        
        for (const FName& MetaDataKey : MetaDataKeys) {
            const TSharedPtr<FJsonObject> MetaDataObject = MakeShareable(new FJsonObject());
            const FCurveMetaData* MetaData = Mapping->GetCurveMetaData(MetaDataKey);
            
            MetaDataObject->SetStringField(TEXT("MetaDataKey"), MetaDataKey.ToString());
            MetaDataObject->SetBoolField(TEXT("bMaterial"), MetaData->Type.bMaterial);
            MetaDataObject->SetBoolField(TEXT("bMorphtarget"), MetaData->Type.bMorphtarget);
            MetaDataObject->SetNumberField(TEXT("MaxLOD"), MetaData->MaxLOD);
            
            TArray<TSharedPtr<FJsonValue>> LinkedBones;
            for (const FBoneReference& BoneReference : MetaData->LinkedBones) {
                LinkedBones.Add(MakeShareable(new FJsonValueString(BoneReference.BoneName.ToString())));
            }
            
            MetaDataObject->SetArrayField(TEXT("LinkedBones"), LinkedBones);
            CurveMetaDataMap.Add(MakeShareable(new FJsonValueObject(MetaDataObject)));
        }
        
        NameMapping->SetArrayField(TEXT("CurveMetaDataMap"), CurveMetaDataMap);
        NameMappings.Add(MakeShareable(new FJsonValueObject(NameMapping)));
    }
    OutObject->SetArrayField(TEXT("NameMappings"), NameMappings);
}

FName USkeletonAssetSerializer::GetAssetClass() const {
    return USkeleton::StaticClass()->GetFName();
}

bool USkeletonAssetSerializer::SupportsParallelDumping() const {
	return false;
}
