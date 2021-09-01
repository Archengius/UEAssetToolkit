#pragma once
#include "UObject/Object.h"
#include "Json.h"
#include "ObjectHierarchySerializer.generated.h"

class UPropertySerializer;

UCLASS()
class ASSETDUMPER_API UObjectHierarchySerializer : public UObject {
    GENERATED_BODY()
private:
    UPROPERTY()
    UPackage* SourcePackage;
    UPROPERTY()
    TMap<UObject*, int32> ObjectIndices;
    UPROPERTY()
    TMap<int32, UObject*> LoadedObjects;
    int32 LastObjectIndex;
    UPROPERTY()
    UPropertySerializer* PropertySerializer;
    TMap<int32, TSharedPtr<FJsonObject>> SerializedObjects;
    UPROPERTY()
    TMap<UObject*, FString> ObjectMarks;
public:
    UObjectHierarchySerializer();

    FORCEINLINE UPropertySerializer* GetPropertySerializer() const { return PropertySerializer; }
    
    TSharedRef<FJsonObject> SerializeObjectProperties(UObject* Object);
    void SerializeObjectPropertiesIntoObject(UObject* Object, TSharedPtr<FJsonObject> OutObject);

	bool CompareUObjects(const int32 ObjectIndex, UObject* Object, bool bCheckExportName, bool bCheckExportOuter);
	bool AreObjectPropertiesUpToDate(const TSharedRef<FJsonObject>& Properties, UObject* Object);

	void FlushPropertiesIntoObject(const int32 ObjectIndex, UObject* Object, bool bVerifyNameAndRename, bool bVerifyOuterAndMove);
    void DeserializeObjectProperties(const TSharedRef<FJsonObject>& Properties, UObject* Object);

	void SetPropertySerializer(UPropertySerializer* NewPropertySerializer);
	
    void InitializeForSerialization(UPackage* NewSourcePackage);

    /**
     * Sets object mark for provided object instance
     * Instances of this object will be serialized as a simple object mark string
     * During deserialization, it is used to lookup object by mark
     */
    void SetObjectMark(UObject* Object, const FString& ObjectMark);

    void InitializeForDeserialization(const TArray<TSharedPtr<FJsonValue>>& ObjectsArray);
	void SetPackageForDeserialization(UPackage* SelfPackage);
	
    UObject* DeserializeObject(int32 Index);
	
    int32 SerializeObject(UObject* Object);
    
    TArray<TSharedPtr<FJsonValue>> FinalizeSerialization();

	void CollectReferencedPackages(const TArray<TSharedPtr<FJsonValue>>& ReferencedSubobjects, TArray<FString>& OutReferencedPackageNames);

	void CollectReferencedPackages(const TArray<TSharedPtr<FJsonValue>>& ReferencedSubobjects, TArray<FString>& OutReferencedPackageNames, TArray<int32>& ObjectsAlreadySerialized);

	FORCEINLINE void CollectObjectPackages(const int32 ObjectIndex, TArray<FString>& OutReferencedPackageNames) {
		TArray<int32> ObjectsAlreadySerialized;
		CollectObjectPackages(ObjectIndex, OutReferencedPackageNames, ObjectsAlreadySerialized);
	}

	void CollectObjectPackages(const int32 ObjectIndex, TArray<FString>& OutReferencedPackageNames, TArray<int32>& ObjectsAlreadySerialized);

    FORCEINLINE static const TSet<FName>& GetUnhandledNativeClasses() { return UnhandledNativeClasses; }
private:
    static TSet<FName> UnhandledNativeClasses;
    
    void SerializeImportedObject(TSharedPtr<FJsonObject> ResultJson, UObject* Object);
    void SerializeExportedObject(TSharedPtr<FJsonObject> ResultJson, UObject* Object);

    UObject* DeserializeImportedObject(TSharedPtr<FJsonObject> ObjectJson);
    UObject* DeserializeExportedObject(TSharedPtr<FJsonObject> ObjectJson);
};
