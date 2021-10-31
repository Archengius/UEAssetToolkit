#pragma once
#include "ObjectHierarchySerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/Object.h"
#include "PropertySerializer.generated.h"

class UObjectHierarchySerializer;

UCLASS()
class ASSETDUMPER_API UPropertySerializer : public UObject {
    GENERATED_BODY()
private:
    friend class UObjectHierarchySerializer;
private:
    using FPropertySerializer = TFunction<TSharedRef<FJsonValue>(FProperty* Property, const void* Value)>;
    using FPropertyDeserializer = TFunction<void(FProperty* Property, TSharedRef<FJsonValue> Value, void* OutValue)>;
    
    UPROPERTY()
    UObjectHierarchySerializer* ObjectHierarchySerializer;

    UPROPERTY()
    TArray<UStruct*> PinnedStructs;
    TArray<FProperty*> BlacklistedProperties;
public:
    /** Disables property serialization entirely */
    void DisablePropertySerialization(UStruct* Struct, FName PropertyName);

    /** Checks whenever we should serialize property in question at all */
    bool ShouldSerializeProperty(FProperty* Property) const;

    TSharedRef<FJsonValue> SerializePropertyValue(FProperty* Property, const void* Value, TArray<int32>* OutReferencedSubobjects = NULL);
    TSharedRef<FJsonObject> SerializeStruct(UScriptStruct* Struct, const void* Value, TArray<int32>* OutReferencedSubobjects = NULL);
    
    void DeserializePropertyValue(FProperty* Property, const TSharedRef<FJsonValue>& Value, void* OutValue);
    void DeserializeStruct(UScriptStruct* Struct, const TSharedRef<FJsonObject>& Value, void* OutValue);

	bool ComparePropertyValues(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context = MakeShareable(new FObjectCompareContext));
	bool CompareStructs(UScriptStruct* Struct, const TSharedRef<FJsonObject>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context = MakeShareable(new FObjectCompareContext));
private:
	bool ComparePropertyValuesInner(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context);
    void DeserializePropertyValueInner(FProperty* Property, const TSharedRef<FJsonValue>& Value, void* OutValue);
    TSharedRef<FJsonValue> SerializePropertyValueInner(FProperty* Property, const void* Value, TArray<int32>* OutReferencedSubobjects);
};
