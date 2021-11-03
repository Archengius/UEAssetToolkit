#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetGenerationUtil.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "BlueprintGenerator.generated.h"

UCLASS()
class ASSETGENERATOR_API UBlueprintGenerator : public UAssetTypeGenerator {
GENERATED_BODY()
protected:
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;

	virtual UBlueprint* CreateNewBlueprint(UPackage* Package, UClass* ParentClass);
	virtual void PostConstructOrUpdateAsset(UBlueprint* Blueprint);
	virtual void PopulateAssetWithData() override;
	virtual void FinalizeAssetCDO() override;
	void UpdateDeserializerBlueprintClassObject(bool bRecompileBlueprint);
	virtual UClass* GetFallbackParentClass() const;
public:
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
	virtual FName GetAssetClass() override;
};

class ASSETGENERATOR_API FBlueprintGeneratorUtils {
public:
	/** Determines whenever function with provided name is generated and should not be auto implemented */
	static bool IsFunctionNameGenerated(const FString& FunctionName);

	/** Determines whenever property name is generated and should not be created */
	static bool IsPropertyNameGenerated(const FString& PropertyName);
	
	/** Makes sure blueprint is up to date and performs various validation routines */
	static void EnsureBlueprintUpToDate(UBlueprint* Blueprint);
	
	/** Adds new interface to the list of implemented interfaces and automatically spawns all required graphs */
	static bool ImplementNewInterface(UBlueprint* Blueprint, UClass* InterfaceClass);

	/** Generates super function call and plugs it automatically, if it is possible */
	static bool CreateParentFunctionCall(UBlueprint* Blueprint, UFunction* Function, UK2Node* FunctionEntryOrEventNode, class UK2Node_FunctionResult* FunctionReturnNode);

	/** Creates new event node with the provided name, or returns existing one */
	static class UK2Node_Event* CreateCustomEventNode(UBlueprint* Blueprint, const FName& EventName);
	
	/** Creates function result node for the provided function entry node */
	static class UK2Node_FunctionResult* CreateFunctionResultNode(const class UK2Node_FunctionEntry* FunctionEntry, bool bAutoConnectNode);

	/** Creates and initializes a new function graph with provided name and possibly allocates K2Node_FunctionResult if asked */
	static class UK2Node_FunctionEntry* CreateFunctionGraph(UBlueprint* Blueprint, const FName& FunctionName);

	/** Creates an override for the provided function, or returns the existing one */
	static UK2Node* CreateFunctionOverride(UBlueprint* Blueprint, UFunction* Function, bool bOverrideAsEvent, bool bCreateParentCall);

	/** Retrieves function name from the node, assuming it's either K2Node_Event or K2Node_FunctionEntry */
	static FName GetFunctionNameForNode(UK2Node* Node);
	
	/** Attempts to find function in the parent class or interfaces */
	static UFunction* FindFunctionInParentClassOrInterfaces(UBlueprint* Blueprint, const FName& FunctionName);

	/** Creates list of blueprint functions for blueprint */
	static bool CreateNewBlueprintFunctions(UBlueprint* Blueprint, const TArray<FDeserializedFunction>& Functions, const TFunctionRef<bool(const FDeserializedFunction& Function)>& FunctionFilter, bool bCreateFunctionOverrides);

	/** Creates a list of blueprint variables from properties, also removes obsolete properties */
	static bool CreateBlueprintVariablesForProperties(UBlueprint* Blueprint, const TArray<FDeserializedProperty>& Properties, const TMap<FName, FDeserializedFunction>& Functions, const TFunctionRef<bool(const FDeserializedProperty& Property)>& PropertyFilter);

	/** Updates FunctionEntry node parameters to match the supplied array */
	static bool SetFunctionEntryParameters(class UK2Node_EditablePinBase* FunctionEntry, const FDeserializedFunction& Function, bool bIsFunctionEntry);

	/** Retrieves blueprint variable description using it's name, or NULL if it's not found */
	static FBPVariableDescription* FindBlueprintVariableByName(UBlueprint* Blueprint, const FName& VariableName);

	/** Resets "Ghost" node state for the provided node */
	static void ResetNodeDisabledState(UEdGraphNode* GraphNode);
private:
	/** Creates and initializes event graph for new event dispatcher property with provided name */
	static UEdGraph* CreateNewBlueprintEventDispatcherSignatureGraph(UBlueprint* Blueprint, const FName& DispatcherName);
};