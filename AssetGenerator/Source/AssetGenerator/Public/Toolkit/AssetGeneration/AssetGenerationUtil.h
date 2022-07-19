#pragma once
#include "CoreMinimal.h"

#define REN_DoNothing REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_SkipGeneratedClasses | REN_DoNotDirty

struct ASSETGENERATOR_API FDeserializedProperty {
public:
	FName PropertyName;
	EPropertyFlags PropertyFlags;
	int32 ArrayDim;
	
	FEdGraphPinType GraphPinType;
	FName RepNotifyFunc;
	ELifetimeCondition BlueprintReplicationCondition;

	FORCEINLINE FDeserializedProperty() : PropertyFlags(CPF_None), ArrayDim(1), BlueprintReplicationCondition(COND_None) {}
	
	FDeserializedProperty(const TSharedPtr<FJsonObject>& Object, class UObjectHierarchySerializer* ObjectSerializer);

	FORCEINLINE bool HasAllPropertyFlags(EPropertyFlags Flags) const {
		return (PropertyFlags & Flags) == Flags;
	}

	FORCEINLINE bool HasAnyPropertyFlags(EPropertyFlags Flags) const {
		return (PropertyFlags & Flags) != 0;
	}

	FORCEINLINE bool IsFunctionInputParam() const {
		return !HasAnyPropertyFlags(CPF_OutParm) || HasAnyPropertyFlags(CPF_ReferenceParm);
	}
};

struct ASSETGENERATOR_API FDeserializedFunction {
public:
	FName FunctionName;
	EFunctionFlags FunctionFlags;

	TArray<FDeserializedProperty> AllProperties;
	TArray<FDeserializedProperty> Parameters;
	FDeserializedProperty ReturnValue;
	

	bool bIsUberGraphFunction;
	bool bIsDelegateSignatureFunction;
	bool bIsCallingIntoUbergraph;
	
	FORCEINLINE FDeserializedFunction() : FunctionFlags(FUNC_None), bIsUberGraphFunction(false), bIsDelegateSignatureFunction(false), bIsCallingIntoUbergraph(false) {}

	FDeserializedFunction(const TSharedPtr<FJsonObject>& Object, class UObjectHierarchySerializer* ObjectSerializer, bool bDeserializeOnlySignatureProperties = false);
	
	FORCEINLINE bool HasAllFunctionFlags(EFunctionFlags Flags) const {
		return (FunctionFlags & Flags) == Flags;
	}

	FORCEINLINE bool HasAnyFunctionFlags(EFunctionFlags Flags) const {
		return (FunctionFlags & Flags) != 0;
	}

	FORCEINLINE bool HasAnyOutputParams() const {
		for (const FDeserializedProperty& Param : Parameters) {
			if (!Param.IsFunctionInputParam()) {
				return true;
			}
		}
		return false;
	}
};

class ASSETGENERATOR_API FAssetGenerationUtil {
public:
	static void ConvertPropertyObjectToGraphPinType(const TSharedPtr<FJsonObject> PropertyObject, FEdGraphPinType& OutPinType, class UObjectHierarchySerializer* ObjectSerializer);

	static void GetPropertyDependencies(const TSharedPtr<FJsonObject> PropertyObject, UObjectHierarchySerializer* ObjectSerializer, TArray<FString>& OutDependencies);

	static bool PopulateStructVariable(const TSharedPtr<FJsonObject>& PropertyObject, UObjectHierarchySerializer* ObjectSerializer, struct FStructVariableDescription& OutStructVariable);

	static bool AreStructDescriptionsEqual(const FStructVariableDescription& A, const FStructVariableDescription& B);
	
	static bool IsFunctionSignatureRelevantProperty(const TSharedPtr<FJsonObject>& PropertyObject);
};