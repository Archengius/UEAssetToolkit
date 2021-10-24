#include "Toolkit/AssetTypes/StaticMeshAssetSerializer.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Toolkit/AssetTypes/FbxMeshExporter.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UStaticMeshAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UStaticMesh)

	DISABLE_SERIALIZATION(UStaticMesh, bAllowCPUAccess);
	DISABLE_SERIALIZATION(UStaticMesh, MinLOD);
	DISABLE_SERIALIZATION(UStaticMesh, ExtendedBounds);
	DISABLE_SERIALIZATION(UStaticMesh, LightmapUVDensity);
	DISABLE_SERIALIZATION(UStaticMesh, StaticMaterials);
	
    //Just serialize normal properties into root object
    SERIALIZE_ASSET_OBJECT

	//Serialize material slot names and values
	TArray<TSharedPtr<FJsonValue>> Materials;

	for (const FStaticMaterial& StaticMaterial : Asset->StaticMaterials) {
		TSharedPtr<FJsonObject> Material = MakeShareable(new FJsonObject());
		Material->SetStringField(TEXT("MaterialSlotName"), StaticMaterial.MaterialSlotName.ToString());
		Material->SetNumberField(TEXT("MaterialInterface"), ObjectSerializer->SerializeObject(StaticMaterial.MaterialInterface));

		Materials.Add(MakeShareable(new FJsonValueObject(Material)));
	}
	Data->SetArrayField(TEXT("Materials"), Materials);

    //Serialize extra properties using native serialization
    Data->SetNumberField(TEXT("NavCollision"), ObjectSerializer->SerializeObject(Asset->NavCollision));
    Data->SetNumberField(TEXT("BodySetup"), ObjectSerializer->SerializeObject(Asset->BodySetup));

	//Serialize screen LOD sizes to carry them to the editor
	Data->SetNumberField(TEXT("MinimumLodNumber"), Asset->MinLOD.Default);
	Data->SetNumberField(TEXT("LodNumber"), Asset->GetNumLODs());
	
	TArray<TSharedPtr<FJsonValue>> ScreenSize;
	for (int32 i = 0; i < MAX_STATIC_MESH_LODS; i++) {
		ScreenSize.Add(MakeShareable(new FJsonValueNumber(Asset->RenderData->ScreenSize[i].Default)));
	}
	Data->SetArrayField(TEXT("ScreenSize"), ScreenSize);

    //Export raw mesh data into separate FBX file that can be imported back into UE
    const FString OutFbxMeshFileName = Context->GetDumpFilePath(TEXT(""), TEXT("fbx"));
    FString OutErrorMessage;
    const bool bSuccess = FFbxMeshExporter::ExportStaticMeshIntoFbxFile(Asset, OutFbxMeshFileName, false, &OutErrorMessage);
    checkf(bSuccess, TEXT("Failed to export static mesh %s: %s"), *Asset->GetPathName(), *OutErrorMessage);

	//Serialize exported model hash to avoid reading it during generation pass
	const FMD5Hash ModelFileHash = FMD5Hash::HashFile(*OutFbxMeshFileName);
	Data->SetStringField(TEXT("ModelFileHash"), LexToString(ModelFileHash));
    
    END_ASSET_SERIALIZATION
}

FName UStaticMeshAssetSerializer::GetAssetClass() const {
    return UStaticMesh::StaticClass()->GetFName();
}

bool UStaticMeshAssetSerializer::SupportsParallelDumping() const {
	return false;
}
