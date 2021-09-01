#pragma once
#include "CoreMinimal.h"

class UObjectHierarchySerializer;
class FJsonObject;

class ASSETDUMPER_API FAssetHelper {
public:
    /**
     * Determines whenever structure implements custom serialization routine
     * Structs with custom serialization require special handling during asset generation and dumping
     * since just restoring/saving properties is not enough for them
     * 
     * It uses structure flag internally to determine whenever struct implements custom serializer
     */
    static bool HasCustomSerializeOnStruct(UScriptStruct* Struct);

    /** Serializes Class object in a way mirroring native UClass::Serialize implementation */
    static void SerializeClass(TSharedPtr<FJsonObject> OutObject, UClass* Class, UObjectHierarchySerializer* ObjectHierarchySerializer);

    /** Serializes Struct object in a way mirroring native UStruct::Serialize implementation */
    static void SerializeStruct(TSharedPtr<FJsonObject> OutObject, UStruct* Struct, UObjectHierarchySerializer* ObjectHierarchySerializer);

    /** Serializes ScriptStruct object in a way mirroring native UStruct::Serialize implementation */
    static void SerializeScriptStruct(TSharedPtr<FJsonObject> OutObject, UScriptStruct* Struct, UObjectHierarchySerializer* ObjectHierarchySerializer);

    /** Serializes UProperty object in a way mirroring native UProperty::Serialize implementation */
    static void SerializeProperty(TSharedPtr<FJsonObject> OutObject, FProperty* Property, UObjectHierarchySerializer* ObjectHierarchySerializer);

    /** Serializes UFunction object in a way mirroring native UFunction::Serialize implementation */
    static void SerializeFunction(TSharedPtr<FJsonObject> OutObject, UFunction* Function, UObjectHierarchySerializer* ObjectHierarchySerializer);

    /** Serializes UEnum object in a way mirroring native UEnum::Serialize implementation */
    static void SerializeEnum(TSharedPtr<FJsonObject> OutObject, UEnum* Enum);

	/** Computes hash for the provided payload */
	static FString ComputePayloadHash(const TArray64<uint8>& Payload);

	/* Computes hash for the provided payload */
	static FString ComputePayloadHash(const TArray<uint8>& Payload);
};
