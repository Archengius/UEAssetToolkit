#pragma once
#include "ObjectHierarchySerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/Object.h"
#include "PropertySerializer.generated.h"

class UObjectHierarchySerializer;

/** Handles struct serialization */
class ASSETDUMPER_API FStructSerializer {
public:
    virtual ~FStructSerializer() = default;
    virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, TArray<int32>* OutReferencedSubobjects) = 0;
    virtual void Deserialize(UScriptStruct* Struct, void* StructData, const TSharedPtr<FJsonObject> JsonValue) = 0;
    virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) = 0;
};

/** Fallback struct serializer using default UE reflection */
class FFallbackStructSerializer : public FStructSerializer {
    UPropertySerializer* PropertySerializer;
public:
    FFallbackStructSerializer(UPropertySerializer* PropertySerializer);
	
    virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructValue, TArray<int32>* OutReferencedSubobjects) override;
    virtual void Deserialize(UScriptStruct* Struct, void* StructValue, const TSharedPtr<FJsonObject> JsonValue) override;
    virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) override;
};

class FDateTimeSerializer : public FStructSerializer {
public:
    virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, TArray<int32>* OutReferencedSubobjects) override;
    virtual void Deserialize(UScriptStruct* Struct, void* StructData, const TSharedPtr<FJsonObject> JsonValue) override;
    virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) override;
};

class FTimespanSerializer : public FStructSerializer {
public:
    virtual void Serialize(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, TArray<int32>* OutReferencedSubobjects) override;
    virtual void Deserialize(UScriptStruct* Struct, void* StructData, const TSharedPtr<FJsonObject> JsonValue) override;
    virtual bool Compare(UScriptStruct* Struct, const TSharedPtr<FJsonObject> JsonValue, const void* StructData, const TSharedPtr<FObjectCompareContext> Context) override;
};

UCLASS()
class ASSETDUMPER_API UPropertySerializer : public UObject {
    GENERATED_BODY()
private:
    friend class UObjectHierarchySerializer;
private:
    UPROPERTY()
    UObjectHierarchySerializer* ObjectHierarchySerializer;

    UPROPERTY()
    TArray<UStruct*> PinnedStructs;
    TArray<FProperty*> BlacklistedProperties;

    TSharedPtr<FStructSerializer> FallbackStructSerializer;
    TMap<UScriptStruct*, TSharedPtr<FStructSerializer>> StructSerializers;
public:
    UPropertySerializer();
    
    /** Disables property serialization entirely */
    void DisablePropertySerialization(UStruct* Struct, FName PropertyName);

    void AddStructSerializer(UScriptStruct* Struct, const TSharedPtr<FStructSerializer>& Serializer);
    
    /** Checks whenever we should serialize property in question at all */
    bool ShouldSerializeProperty(FProperty* Property) const;

    TSharedRef<FJsonValue> SerializePropertyValue(FProperty* Property, const void* Value, TArray<int32>* OutReferencedSubobjects = NULL);
    TSharedRef<FJsonObject> SerializeStruct(UScriptStruct* Struct, const void* Value, TArray<int32>* OutReferencedSubobjects = NULL);
    
    void DeserializePropertyValue(FProperty* Property, const TSharedRef<FJsonValue>& Value, void* OutValue);
    void DeserializeStruct(UScriptStruct* Struct, const TSharedRef<FJsonObject>& Value, void* OutValue);

	bool ComparePropertyValues(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context = MakeShareable(new FObjectCompareContext));
	bool CompareStructs(UScriptStruct* Struct, const TSharedRef<FJsonObject>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context = MakeShareable(new FObjectCompareContext));
private:
    FStructSerializer* GetStructSerializer(UScriptStruct* Struct) const;
	bool ComparePropertyValuesInner(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, const void* CurrentValue, const TSharedPtr<FObjectCompareContext> Context);
    void DeserializePropertyValueInner(FProperty* Property, const TSharedRef<FJsonValue>& Value, void* OutValue);
    TSharedRef<FJsonValue> SerializePropertyValueInner(FProperty* Property, const void* Value, TArray<int32>* OutReferencedSubobjects);
};
