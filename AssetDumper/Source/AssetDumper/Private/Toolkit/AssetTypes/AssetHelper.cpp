#include "Toolkit/AssetTypes/AssetHelper.h"
#include "UObject/Class.h"
#include "Toolkit/KismetBytecodeDisassemblerJson.h"
#include "Toolkit/ObjectHierarchySerializer.h"

bool FAssetHelper::HasCustomSerializeOnStruct(UScriptStruct* Struct) {
    return (Struct->StructFlags & STRUCT_SerializeNative) != 0;
}

void FAssetHelper::SerializeClass(TSharedPtr<FJsonObject> OutObject, UClass* Class, UObjectHierarchySerializer* ObjectHierarchySerializer) {
    
    //Serialize native Struct class data first
    SerializeStruct(OutObject, Class, ObjectHierarchySerializer);
    
    //Skip serializing FuncMap as there is no point really
    //It can be regenerated on deserialization quickly

    //Save class flags (excluding those which should never get loaded)
    EClassFlags SavedClassFlags = Class->ClassFlags;
    SavedClassFlags &= ~CLASS_ShouldNeverBeLoaded;
    OutObject->SetStringField(TEXT("ClassFlags"), FString::Printf(TEXT("%d"), SavedClassFlags));

    //Class inside of which this class should always exist
    const int32 ClassWithinObjectIndex = ObjectHierarchySerializer->SerializeObject(Class->ClassWithin);
    OutObject->SetNumberField(TEXT("ClassWithin"), ClassWithinObjectIndex);

    //Class configuration name (e.g Engine for Engine.ini)
    OutObject->SetStringField(TEXT("ClassConfigName"), Class->ClassConfigName.ToString());

    //Skip ClassGeneratedBy - always NULL in shipping anyways
    
    //Serialize implemented interfaces
    TArray<TSharedPtr<FJsonValue>> ImplementedInterfaces;
    
    for (const FImplementedInterface& ImplementedInterface : Class->Interfaces) {
        TSharedPtr<FJsonObject> Interface = MakeShareable(new FJsonObject());
        const int32 ClassObjectIndex = ObjectHierarchySerializer->SerializeObject(ImplementedInterface.Class);
        Interface->SetNumberField(TEXT("Class"), ClassObjectIndex);
        Interface->SetNumberField(TEXT("PointerOffset"), ImplementedInterface.PointerOffset);
        Interface->SetBoolField(TEXT("bImplementedByK2"), ImplementedInterface.bImplementedByK2);
        ImplementedInterfaces.Add(MakeShareable(new FJsonValueObject(Interface)));
    }
    OutObject->SetArrayField(TEXT("Interfaces"), ImplementedInterfaces);
    
    //Serialize ClassDefaultObject
    const int32 ClassDefaultObjectIndex = ObjectHierarchySerializer->SerializeObject(Class->ClassDefaultObject);
    OutObject->SetNumberField(TEXT("ClassDefaultObject"), ClassDefaultObjectIndex);
}


void FAssetHelper::SerializeStruct(TSharedPtr<FJsonObject> OutObject, UStruct* Struct, UObjectHierarchySerializer* ObjectHierarchySerializer) {
    //Do not serialize UField parent object
    //It doesn't have anything special in it's Serialize anyway,
    //just support for legacy field serialization, which we don't need
    
    //Serialize SuperStruct first
    const int32 SuperStructIndex = ObjectHierarchySerializer->SerializeObject(Struct->GetSuperStruct());
    OutObject->SetNumberField(TEXT("SuperStruct"), SuperStructIndex);

    //Serialize Children now (these can only be UFunctions, properties are no longer UObjects)
    TArray<TSharedPtr<FJsonValue>> Children;
    UField* Child = Struct->Children;
    while (Child) {
        const TSharedPtr<FJsonObject> FieldObject = MakeShareable(new FJsonObject());

        if (Child->IsA<UFunction>()) {
            FieldObject->SetStringField(TEXT("FieldKind"), TEXT("Function"));
            SerializeFunction(FieldObject, Cast<UFunction>(Child), ObjectHierarchySerializer);
        } else {
            checkf(0, TEXT("Unsupported Children object type: %s"), *Child->GetClass()->GetPathName());
        }
        
        Children.Add(MakeShareable(new FJsonValueObject(FieldObject)));
        Child = Child->Next;
    }
    OutObject->SetArrayField(TEXT("Children"), Children);

    //Serialize ChildProperties (these can only be FProperties, representing what old UProperties used to)
    TArray<TSharedPtr<FJsonValue>> ChildProperties;
    FField* ChildField = Struct->ChildProperties;
    while (ChildField) {
        const TSharedPtr<FJsonObject> FieldObject = MakeShareable(new FJsonObject());

        if (ChildField->IsA<FProperty>()) {
            FieldObject->SetStringField(TEXT("FieldKind"), TEXT("Property"));
            SerializeProperty(FieldObject, CastField<FProperty>(ChildField), ObjectHierarchySerializer);
        } else {
            checkf(0, TEXT("Unsupported ChildProperties object type: %s"), *ChildField->GetClass()->GetName());
        }
        
        ChildProperties.Add(MakeShareable(new FJsonValueObject(FieldObject)));
        ChildField = ChildField->Next;
    }
    OutObject->SetArrayField(TEXT("ChildProperties"), ChildProperties);

    //Serialize script bytecode now
    //but only if we actually have some, since normal classes and script structs never have any
    if (Struct->Script.Num()) {
        FKismetBytecodeDisassemblerJson BytecodeDisassembler;
        const TArray<TSharedPtr<FJsonValue>> Statements = BytecodeDisassembler.SerializeFunction(Struct);
        OutObject->SetArrayField(TEXT("Script"), Statements);
    }
}

void FAssetHelper::SerializeScriptStruct(TSharedPtr<FJsonObject> OutObject, UScriptStruct* Struct, UObjectHierarchySerializer* ObjectHierarchySerializer) {

    //Serialize normal Struct data first
    SerializeStruct(OutObject, Struct, ObjectHierarchySerializer);

    //Serialize struct flags
    OutObject->SetNumberField(TEXT("StructFlags"), Struct->StructFlags);
}

void FAssetHelper::SerializeProperty(TSharedPtr<FJsonObject> OutObject, FProperty* Property, UObjectHierarchySerializer* ObjectHierarchySerializer) {
    OutObject->SetStringField(TEXT("ObjectClass"), Property->GetClass()->GetName());
    OutObject->SetStringField(TEXT("ObjectName"), Property->GetName());
    
    //Serialize array dimensions
    OutObject->SetNumberField(TEXT("ArrayDim"), Property->ArrayDim);
    
    //Serialize property flags
    const EPropertyFlags SaveFlags = Property->PropertyFlags & ~CPF_ComputedFlags;
    OutObject->SetStringField(TEXT("PropertyFlags"), FString::Printf(TEXT("%lld"), SaveFlags));

    //Serialize RepNotify function name
    OutObject->SetStringField(TEXT("RepNotifyFunc"), Property->RepNotifyFunc.ToString());
    OutObject->SetNumberField(TEXT("BlueprintReplicationCondition"), Property->GetBlueprintReplicationCondition());
    
    //Serialize additional data depending on property type
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property)) {
        //For enum properties, we serialize Enum and UnderlyingProperty
        const int32 EnumObjectIndex = ObjectHierarchySerializer->SerializeObject(EnumProperty->GetEnum());
        OutObject->SetNumberField(TEXT("Enum"), EnumObjectIndex);

        const TSharedPtr<FJsonObject> UnderlyingProp = MakeShareable(new FJsonObject());
        SerializeProperty(UnderlyingProp, EnumProperty->GetUnderlyingProperty(), ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("UnderlyingProp"), UnderlyingProp);

    } else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property)) {
        //Serialize structure type of this property
        const int32 StructObjectIndex = ObjectHierarchySerializer->SerializeObject(StructProperty->Struct);
        OutObject->SetNumberField(TEXT("Struct"), StructObjectIndex);

    } else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property)) {
        //Serialize inner property for array
        const TSharedPtr<FJsonObject> InnerProperty = MakeShareable(new FJsonObject());
        SerializeProperty(InnerProperty, ArrayProperty->Inner, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("Inner"), InnerProperty);
        
    } else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property)) {
        //Serialize bool property native type and size
        OutObject->SetNumberField(TEXT("BoolSize"), BoolProperty->ElementSize);
        OutObject->SetBoolField(TEXT("NativeBool"), BoolProperty->IsNativeBool());
        
    } else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property)) {
        //Serialize Enum for legacy enum properties (TByteAsEnum)
        const int32 EnumObjectIndex = ObjectHierarchySerializer->SerializeObject(ByteProperty->Enum);
        OutObject->SetNumberField(TEXT("Enum"), EnumObjectIndex);
        
    } else if (FClassProperty* ClassProperty = CastField<FClassProperty>(Property)) {
        //For class property, we need to serialize MetaClass
        const int32 MetaClassIndex = ObjectHierarchySerializer->SerializeObject(ClassProperty->MetaClass);
        OutObject->SetNumberField(TEXT("MetaClass"), MetaClassIndex);
        
    } else if (FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(Property)) {
        //For delegate properties, we need to serialize signature function
        //Since it will always be present in the Child array too, we serialize just it's name
        //and not actual full UFunction object
        OutObject->SetStringField(TEXT("SignatureFunction"), DelegateProperty->SignatureFunction->GetName());
        
    } else if (FInterfaceProperty* InterfaceProperty = CastField<FInterfaceProperty>(Property)) {
        //For interface properties, we serialize interface type class
        const int32 InterfaceClassIndex = ObjectHierarchySerializer->SerializeObject(InterfaceProperty->InterfaceClass);
        OutObject->SetNumberField(TEXT("InterfaceClass"), InterfaceClassIndex);
        
    } else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property)) {
        //For map properties, we just serialize key property type and value property type
        const TSharedPtr<FJsonObject> KeyProperty = MakeShareable(new FJsonObject());
        SerializeProperty(KeyProperty, MapProperty->KeyProp, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("KeyProp"), KeyProperty);

        const TSharedPtr<FJsonObject> ValueProperty = MakeShareable(new FJsonObject());
        SerializeProperty(ValueProperty, MapProperty->ValueProp, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("ValueProp"), ValueProperty);
        
    } else if (FMulticastDelegateProperty* MulticastDelegateProperty = CastField<FMulticastDelegateProperty>(Property)) {
        //For multicast delegate properties, record signature function name
        OutObject->SetStringField(TEXT("SignatureFunction"), MulticastDelegateProperty->SignatureFunction->GetName());
        
    } else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property)) {
        //For set properties, serialize element type
        const TSharedPtr<FJsonObject> ElementType = MakeShareable(new FJsonObject());
        SerializeProperty(ElementType, SetProperty->ElementProp, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("ElementType"), ElementType);
        
    } else if (FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property)) {
        //For soft class property, we serialize MetaClass
        const int32 MetaClassIndex = ObjectHierarchySerializer->SerializeObject(SoftClassProperty->MetaClass);
        OutObject->SetNumberField(TEXT("MetaClass"), MetaClassIndex);
    
    } else if (FFieldPathProperty* FieldPathProperty = CastField<FFieldPathProperty>(Property)) {
        //Serialize field class
        OutObject->SetStringField(TEXT("PropertyClass"), FieldPathProperty->PropertyClass->GetName());
        
    } else if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property)) {
        //For object properties, we serialize object type
        const int32 ObjectClassIndex = ObjectHierarchySerializer->SerializeObject(ObjectProperty->PropertyClass);
        OutObject->SetNumberField(TEXT("PropertyClass"), ObjectClassIndex);
        
    } else {
        //Other property classes don't override Serialize, so do nothing
    }
}

void FAssetHelper::SerializeEnum(TSharedPtr<FJsonObject> OutObject, UEnum* Enum) {
    //Serialize Names map as objects
    TArray<TSharedPtr<FJsonValue>> Names;
    for (int32 i = 0; i < Enum->NumEnums(); i++) {
        TSharedPtr<FJsonObject> NameObject = MakeShareable(new FJsonObject());
        NameObject->SetNumberField(TEXT("Value"), Enum->GetValueByIndex(i));
        NameObject->SetStringField(TEXT("Name"), Enum->GetNameStringByIndex(i));
        Names.Add(MakeShareable(new FJsonValueObject(NameObject)));
    }
    OutObject->SetArrayField(TEXT("Names"), Names);
    
    //Serialize cppform as byte
    OutObject->SetNumberField(TEXT("CppForm"), (uint8) Enum->GetCppForm());
}

void FAssetHelper::SerializeFunction(TSharedPtr<FJsonObject> OutObject, UFunction* Function, UObjectHierarchySerializer* ObjectHierarchySerializer) {
    OutObject->SetStringField(TEXT("ObjectClass"), UFunction::StaticClass()->GetName());
    OutObject->SetStringField(TEXT("ObjectName"), Function->GetName());
    
    //Serialize super Struct data
    //It will also serialize script bytecode for function
    SerializeStruct(OutObject, Function, ObjectHierarchySerializer);

    //Save function flags
    const EFunctionFlags FunctionFlags = Function->FunctionFlags;
    OutObject->SetStringField(TEXT("FunctionFlags"), FString::Printf(TEXT("%d"), FunctionFlags));

    //Serialize event graph call information to aid with blueprint decompilation
    if (Function->EventGraphFunction) {
        OutObject->SetStringField(TEXT("EventGraphFunction"), Function->EventGraphFunction->GetName());
        OutObject->SetNumberField(TEXT("EventGraphCallOffset"), Function->EventGraphCallOffset);
    }
}

FString FAssetHelper::ComputePayloadHash(const TArray64<uint8>& Payload) {
	FString Hash = FMD5::HashBytes(Payload.GetData(), Payload.Num() * sizeof(uint8));
	Hash.Append(FString::Printf(TEXT("%llx"), Payload.Num()));
	return Hash;
}

FString FAssetHelper::ComputePayloadHash(const TArray<uint8>& Payload) {
	FString Hash = FMD5::HashBytes(Payload.GetData(), Payload.Num() * sizeof(uint8));
	Hash.Append(FString::Printf(TEXT("%x"), Payload.Num()));
	return Hash;
}




