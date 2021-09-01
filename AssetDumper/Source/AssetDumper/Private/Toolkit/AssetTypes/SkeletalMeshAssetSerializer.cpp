#include "Toolkit/AssetTypes/SkeletalMeshAssetSerializer.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Toolkit/PropertySerializer.h"
#include "Engine/SkeletalMesh.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void USkeletalMeshAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(USkeletalMesh)
	
	DISABLE_SERIALIZATION_RAW(USkeletalMesh, "SamplingInfo");
	DISABLE_SERIALIZATION_RAW(USkeletalMesh, "LODInfo");
	
	DISABLE_SERIALIZATION(USkeletalMesh, bHasVertexColors);
	DISABLE_SERIALIZATION(USkeletalMesh, bHasBeenSimplified);

	//TODO support physic asset dumping/generation
	DISABLE_SERIALIZATION(USkeletalMesh, PhysicsAsset);
	DISABLE_SERIALIZATION(USkeletalMesh, ShadowPhysicsAsset);
	
    //Serialize normal asset data
    SERIALIZE_ASSET_OBJECT
    
	//Serialize material slot names and values
	TArray<TSharedPtr<FJsonValue>> Materials;

	for (const FSkeletalMaterial& SkeletalMaterial : Asset->Materials) {
		TSharedPtr<FJsonObject> Material = MakeShareable(new FJsonObject());
		Material->SetStringField(TEXT("MaterialSlotName"), SkeletalMaterial.MaterialSlotName.ToString());
		Material->SetNumberField(TEXT("MaterialInterface"), ObjectSerializer->SerializeObject(SkeletalMaterial.MaterialInterface));

		Materials.Add(MakeShareable(new FJsonValueObject(Material)));
	}
	Data->SetArrayField(TEXT("Materials"), Materials);
	
    //Serialize BodySetup if per poly collision is enabled
    if (Asset->bEnablePerPolyCollision) {
        Data->SetNumberField(TEXT("BodySetup"), ObjectSerializer->SerializeObject(Asset->BodySetup));
    }

    //Export raw mesh data into separate FBX file that can be imported back into UE
    const FString OutFbxMeshFileName = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));

	FString OutErrorMessage;
    const bool bSuccess = FFbxMeshExporter::ExportSkeletalMeshIntoFbxFile(Asset, OutFbxMeshFileName, false, &OutErrorMessage);
    checkf(bSuccess, TEXT("Failed to export skeletal mesh %s: %s"), *Asset->GetPathName(), *OutErrorMessage);

	//Serialize exported model hash to avoid reading it during generation pass
	const FMD5Hash ModelFileHash = FMD5Hash::HashFile(*OutFbxMeshFileName);
	Data->SetStringField(TEXT("ModelFileHash"), LexToString(ModelFileHash));
	
    END_ASSET_SERIALIZATION
}

void USkeletalMeshAssetSerializer::SerializeReferenceSkeleton(const FReferenceSkeleton& ReferenceSkeleton, TSharedPtr<FJsonObject> OutObject) {
    //Serialize pose together with bone info
    TArray<TSharedPtr<FJsonValue>> SkeletonBones;
    for (int32 i = 0; i < ReferenceSkeleton.GetRawBoneNum(); i++) {
        const FMeshBoneInfo& BoneInfo = ReferenceSkeleton.GetRawRefBoneInfo()[i];
        const FTransform& PoseTransform = ReferenceSkeleton.GetRawRefBonePose()[i];
        
        TSharedPtr<FJsonObject> BoneObject = MakeShareable(new FJsonObject());
        BoneObject->SetStringField(TEXT("Name"), BoneInfo.Name.ToString());
        BoneObject->SetNumberField(TEXT("ParentIndex"), BoneInfo.ParentIndex);
        BoneObject->SetNumberField(TEXT("Index"), i);
        BoneObject->SetStringField(TEXT("Pose"), PoseTransform.ToString());
        
        SkeletonBones.Add(MakeShareable(new FJsonValueObject(BoneObject)));
    }
    OutObject->SetArrayField(TEXT("Bones"), SkeletonBones);
    
    //Do not serialize RawNameToIndexMap because it can be reconstructed
    //from serialize bone info structure
}

FName USkeletalMeshAssetSerializer::GetAssetClass() const {
    return USkeletalMesh::StaticClass()->GetFName();
}

bool USkeletalMeshAssetSerializer::SupportsParallelDumping() const {
	return false;
}
