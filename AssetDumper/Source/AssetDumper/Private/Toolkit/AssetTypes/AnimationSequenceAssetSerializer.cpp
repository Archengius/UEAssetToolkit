#include "Toolkit/AssetTypes/AnimationSequenceAssetSerializer.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Toolkit/PropertySerializer.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"

void UAnimationSequenceAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UAnimSequence)

    DISABLE_SERIALIZATION(UAnimSequence, RawCurveData);
	DISABLE_SERIALIZATION(UAnimSequence, SequenceLength);
	DISABLE_SERIALIZATION_RAW(UAnimSequence, "TrackToSkeletonMapTable");
	DISABLE_SERIALIZATION_RAW(UAnimSequence, "NumFrames");
	
    SERIALIZE_ASSET_OBJECT

	//Serialize precomputed framerate because we skip NumFrames serialization
	Data->SetNumberField(TEXT("FrameRate"), (int32) Asset->GetFrameRate());
	Data->SetNumberField(TEXT("NumFrames"), Asset->GetNumberOfFrames());
	Data->SetNumberField(TEXT("SequenceLength"), Asset->SequenceLength);

    //Serialize animation data
    const FString OutFbxFileName = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
    FString OutErrorMessage;
    const bool bSuccess = FFbxMeshExporter::ExportAnimSequenceIntoFbxFile(Asset, OutFbxFileName, false, &OutErrorMessage);
    checkf(bSuccess, TEXT("Failed to export anim sequence %s: %s"), *Asset->GetPathName(), *OutErrorMessage);

	//Serialize exported model hash to avoid reading it during generation pass
	const FMD5Hash ModelFileHash = FMD5Hash::HashFile(*OutFbxFileName);
	Data->SetStringField(TEXT("ModelFileHash"), LexToString(ModelFileHash));
	
    END_ASSET_SERIALIZATION
}

FName UAnimationSequenceAssetSerializer::GetAssetClass() const {
    return UAnimSequence::StaticClass()->GetFName();
}

bool UAnimationSequenceAssetSerializer::SupportsParallelDumping() const {
	return false;
}
