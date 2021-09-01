#pragma once
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "BlueprintAssetSerializer.generated.h"

UCLASS(MinimalAPI)
class UBlueprintAssetSerializer : public UAssetTypeSerializer {
    GENERATED_BODY()
public:
    virtual void SerializeAsset(TSharedRef<FSerializationContext> Context) const override;

    /** Collects names of generated variables for the provided blueprint class */
    static void CollectGeneratedVariables(class UBlueprintGeneratedClass* Asset, TArray<FName>& OutGeneratedVariableNames);
    
    /** Serializes UBlueprintGeneratedClass instance */
    static void SerializeBlueprintClass(class UBlueprintGeneratedClass* Asset, TSharedPtr<class FJsonObject> Data, TSharedRef<FSerializationContext> Context);
    
    virtual FName GetAssetClass() const override;
};
