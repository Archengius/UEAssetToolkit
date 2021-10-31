#pragma once
#include "Toolkit/AssetTypes/MaterialAssetSerializer.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "AssetDumperModule.h"

void UMaterialAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UMaterial)
	
    //TODO we do not serialize shaders yet, but information exposed by normal object serialization should be enough for reasonable stubs
    //obviously they will be unable to show material in editor, but they can be used to reference it and even create new instances on top of it

	DisableMaterialExpressionProperties(Serializer);
	DisableMaterialFunctionSerialization(Serializer);
	SerializeReferencedFunctions(Asset->GetCachedExpressionData(), Data);
	
	SERIALIZE_ASSET_OBJECT
	
    END_ASSET_SERIALIZATION
}

void UMaterialAssetSerializer::SerializeReferencedFunctions(const FMaterialCachedExpressionData& ExpressionData, const TSharedPtr<FJsonObject> Data) {
	
	TArray<TSharedPtr<FJsonValue>> ReferencedFunctions;
	for (const FMaterialFunctionInfo& FunctionInfo : ExpressionData.FunctionInfos) {
		ReferencedFunctions.Add(MakeShareable(new FJsonValueString(FunctionInfo.Function->GetPathName())));
	}

	TArray<TSharedPtr<FJsonValue>> MaterialLayers;
	for (UMaterialFunctionInterface* Function : ExpressionData.DefaultLayers) {
		MaterialLayers.Add(MakeShareable(new FJsonValueString(Function->GetPathName())));
	}

	TArray<TSharedPtr<FJsonValue>> MaterialLayerBlends;
	for (UMaterialFunctionInterface* Function : ExpressionData.DefaultLayerBlends) {
		MaterialLayerBlends.Add(MakeShareable(new FJsonValueString(Function->GetPathName())));
	}

	Data->SetArrayField(TEXT("ReferencedFunctions"), ReferencedFunctions);
	Data->SetArrayField(TEXT("MaterialLayers"), MaterialLayers);
	Data->SetArrayField(TEXT("MaterialLayerBlends"), MaterialLayerBlends);
}

void UMaterialAssetSerializer::DisableMaterialFunctionSerialization(UPropertySerializer* Serializer) {
	DISABLE_SERIALIZATION(FMaterialCachedExpressionData, FunctionInfos);
	DISABLE_SERIALIZATION(FMaterialCachedExpressionData, DefaultLayers);
	DISABLE_SERIALIZATION(FMaterialCachedExpressionData, DefaultLayerBlends);
}

void UMaterialAssetSerializer::DisableMaterialExpressionProperties(UPropertySerializer* Serializer) {
	DISABLE_SERIALIZATION(UMaterial, Metallic);
	DISABLE_SERIALIZATION(UMaterial, Specular);
	DISABLE_SERIALIZATION(UMaterial, Anisotropy);
	DISABLE_SERIALIZATION(UMaterial, Normal);
	DISABLE_SERIALIZATION(UMaterial, Tangent);
	DISABLE_SERIALIZATION(UMaterial, EmissiveColor);
	DISABLE_SERIALIZATION(UMaterial, WorldPositionOffset);
	DISABLE_SERIALIZATION(UMaterial, Refraction);
	DISABLE_SERIALIZATION(UMaterial, PixelDepthOffset);
	DISABLE_SERIALIZATION(UMaterial, ShadingModelFromMaterialExpression);
	DISABLE_SERIALIZATION(UMaterial, MaterialAttributes);
}

FName UMaterialAssetSerializer::GetAssetClass() const {
    return UMaterial::StaticClass()->GetFName();
}
