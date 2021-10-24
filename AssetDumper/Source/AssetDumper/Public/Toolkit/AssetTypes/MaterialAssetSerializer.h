#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "MaterialAssetSerializer.generated.h"

class UPropertySerializer;

UCLASS(MinimalAPI)
class UMaterialAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

	static void DisableMaterialExpressionProperties(UPropertySerializer* Serializer);
	static void DisableMaterialFunctionSerialization(UPropertySerializer* Serializer);
	static void SerializeReferencedFunctions(const FMaterialCachedExpressionData& ExpressionData, const TSharedPtr<FJsonObject> Data);
	
    virtual FName GetAssetClass() const override;
};