#include "Toolkit/AssetTypeGenerator/BlueprintGenerator.h"
#include "K2Node_FunctionEntry.h"
#include "Dom/JsonObject.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetGeneration/AssetGenerationUtil.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "BlueprintCompilationManager.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionResult.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Animation/AnimBlueprint.h"

UBlueprint* UBlueprintGenerator::CreateNewBlueprint(UPackage* Package, UClass* ParentClass) {
	EBlueprintType BlueprintType = BPTYPE_Normal;

	if (ParentClass->HasAnyClassFlags(CLASS_Const)) {
		BlueprintType = BPTYPE_Const;
	}
	if (ParentClass == UBlueprintFunctionLibrary::StaticClass()) {
		BlueprintType = BPTYPE_FunctionLibrary;
	}
	if (ParentClass == UInterface::StaticClass()) {
		BlueprintType = BPTYPE_Interface;
	}
	return FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, GetAssetName(), BlueprintType, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
}

void UBlueprintGenerator::CreateAssetPackage() {
	const int32 SuperStructIndex = GetAssetData()->GetIntegerField(TEXT("SuperStruct"));
	UClass* ParentClass = CastChecked<UClass>(GetObjectSerializer()->DeserializeObject(SuperStructIndex));

	UPackage* NewPackage = CreatePackage(*GetPackageName().ToString());
	UBlueprint* NewBlueprint = CreateNewBlueprint(NewPackage, ParentClass);
	SetPackageAndAsset(NewPackage, NewBlueprint, false);

	UpdateDeserializerBlueprintClassObject(true);
	MarkAssetChanged();
	
	PostConstructOrUpdateAsset(NewBlueprint);
}

void UBlueprintGenerator::OnExistingPackageLoaded() {
	UBlueprint* Blueprint = GetAsset<UBlueprint>();

	const int32 SuperStructIndex = GetAssetData()->GetIntegerField(TEXT("SuperStruct"));
	UClass* ParentClass = CastChecked<UClass>(GetObjectSerializer()->DeserializeObject(SuperStructIndex));

	//Make sure blueprint has correct parent class, and if it doesn't re-parent it, hopefully without causing too many issues
	if (Blueprint->ParentClass != ParentClass) {
		UE_LOG(LogAssetGenerator, Warning, TEXT("Reparenting Blueprint %s (%s -> %s)"), *Blueprint->GetPathName(), *Blueprint->ParentClass->GetPathName(), *ParentClass->GetPathName());

		FBlueprintGeneratorUtils::EnsureBlueprintUpToDate(Blueprint);
		Blueprint->ParentClass = ParentClass;
		FBlueprintGeneratorUtils::EnsureBlueprintUpToDate(Blueprint);
		
		FBlueprintCompilationManager::CompileSynchronously(FBPCompileRequest(Blueprint, EBlueprintCompileOptions::None, NULL));
		MarkAssetChanged();
	}
	
	PostConstructOrUpdateAsset(Blueprint);
}

void UBlueprintGenerator::PostConstructOrUpdateAsset(UBlueprint* Blueprint) {
	TArray<TSharedPtr<FJsonValue>> ImplementedInterfaces = GetAssetData()->GetArrayField(TEXT("Interfaces"));

	//Make sure blueprint implements are required interfaces
	for (int32 i = 0; i < ImplementedInterfaces.Num(); i++) {
		const TSharedPtr<FJsonObject> InterfaceObject = ImplementedInterfaces[i]->AsObject();
		const int32 ClassObjectIndex = InterfaceObject->GetIntegerField(TEXT("Class"));

		UClass* InterfaceClass = CastChecked<UClass>(GetObjectSerializer()->DeserializeObject(ClassObjectIndex));
		if (FBlueprintGeneratorUtils::ImplementNewInterface(Blueprint, InterfaceClass)) {
			MarkAssetChanged();
		}
	}

	EClassFlags ClassFlags = (EClassFlags) FCString::Atoi64(*GetAssetData()->GetStringField(TEXT("ClassFlags")));
	
	const bool bGenerateConstClass = (ClassFlags & CLASS_Const) != 0;
	const bool bGenerateAbstractClass = (ClassFlags & CLASS_Abstract) != 0;
	const bool bDeprecatedClass = (ClassFlags & CLASS_Deprecated) != 0;

	//Update blueprint flags by looking at the generated class flags
	if (bGenerateConstClass != Blueprint->bGenerateConstClass) {
		Blueprint->bGenerateConstClass = bGenerateConstClass;
		MarkAssetChanged();
	}
	if (bGenerateAbstractClass != Blueprint->bGenerateAbstractClass) {
		Blueprint->bGenerateAbstractClass = bGenerateAbstractClass;
		MarkAssetChanged();
	}
	if (bDeprecatedClass != Blueprint->bDeprecate) {
		Blueprint->bDeprecate = bDeprecatedClass;
		MarkAssetChanged();
	}

	//Recompile blueprint if asset has actually been changed
	UpdateDeserializerBlueprintClassObject(HasAssetBeenEverChanged());
}

void UBlueprintGenerator::PopulateAssetWithData() {
	UpdateDeserializerBlueprintClassObject(false);
	
	UBlueprint* Blueprint = GetAsset<UBlueprint>();

	const TArray<TSharedPtr<FJsonValue>>& ChildProperties = GetAssetData()->GetArrayField(TEXT("ChildProperties"));
	const TArray<TSharedPtr<FJsonValue>>& Children = GetAssetData()->GetArrayField(TEXT("Children"));
	const TArray<TSharedPtr<FJsonValue>>& GeneratedVariablesArray = GetAssetData()->GetArrayField(TEXT("GeneratedVariableNames"));

	TSet<FName> GeneratedVariableNames;
	for (const TSharedPtr<FJsonValue>& VariableName : GeneratedVariablesArray) {
		GeneratedVariableNames.Add(*VariableName->AsString());
	}
	
	TArray<FDeserializedProperty> Properties;
	TMap<FName, FDeserializedFunction> FunctionMap;

	for (int32 i = 0; i < ChildProperties.Num(); i++) {
		const TSharedPtr<FJsonObject> PropertyObject = ChildProperties[i]->AsObject();
		new (Properties) FDeserializedProperty(PropertyObject, GetObjectSerializer());
	}

	for (int32 i = 0; i < Children.Num(); i++) {
		const TSharedPtr<FJsonObject> FunctionObject = Children[i]->AsObject();
		FDeserializedFunction DeserializedFunction(FunctionObject, GetObjectSerializer());
		FunctionMap.Add(DeserializedFunction.FunctionName, MoveTemp(DeserializedFunction));
	}

	TArray<FDeserializedFunction> Functions;
	FunctionMap.GenerateValueArray(Functions);

	//Regenerate blueprint properties
	const bool bChangedProperties = FBlueprintGeneratorUtils::CreateBlueprintVariablesForProperties(Blueprint, Properties, FunctionMap,
		[&](const FDeserializedProperty& Property){
		return !GeneratedVariableNames.Contains(Property.PropertyName);
	});

	//Force blueprint compilation after we have changed any properties
	if (bChangedProperties) {
		UpdateDeserializerBlueprintClassObject(true);
		MarkAssetChanged();
	}

	//Rebuild any functions that blueprint has
	const bool bChangedFunctions = FBlueprintGeneratorUtils::CreateNewBlueprintFunctions(Blueprint, Functions,
		[&](const FDeserializedFunction& Function){
			return true;
	}, true);

	//Recompile blueprint so all the function changed are visible
	if (bChangedFunctions) {
		UpdateDeserializerBlueprintClassObject(true);
		MarkAssetChanged();
	}
}

void UBlueprintGenerator::FinalizeAssetCDO() {
	UpdateDeserializerBlueprintClassObject(false);
	
	UBlueprint* Blueprint = GetAsset<UBlueprint>();
	USimpleConstructionScript* OldSimpleConstructionScript = Blueprint->SimpleConstructionScript;
	
	//TEMPFIX: Move SCS to the correct outer if it does not have one already
	if (OldSimpleConstructionScript != NULL && OldSimpleConstructionScript->GetOuter() != Blueprint->GeneratedClass) {
		OldSimpleConstructionScript->Rename(NULL, Blueprint->GeneratedClass, REN_DoNothing);
	}

	//Flush CDO changes into the existing class default object
	UObject* ClassDefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
	const int32 ClassDefaultObjectIndex = GetAssetData()->GetIntegerField(TEXT("ClassDefaultObject"));
	const bool bCDOChanged = !GetObjectSerializer()->CompareUObjects(
		ClassDefaultObjectIndex, ClassDefaultObject, false, false);
	
	if (ClassDefaultObject && bCDOChanged) {
		GetObjectSerializer()->FlushPropertiesIntoObject(ClassDefaultObjectIndex, ClassDefaultObject, false, false);

		UpdateDeserializerBlueprintClassObject(true);
		MarkAssetChanged();
	}

	//Flush SimpleConstructionScript changes too
	const TSharedPtr<FJsonObject> AssetObjectData = GetAssetData()->GetObjectField(TEXT("AssetObjectData"));
	const int32 SimpleConstructionScriptIndex = AssetObjectData->GetIntegerField(TEXT("SimpleConstructionScript"));
	
	const bool bScriptObjectChanged = !GetObjectSerializer()->CompareUObjects(
		SimpleConstructionScriptIndex, OldSimpleConstructionScript, false, false);
	
	if (bScriptObjectChanged) {
		//Trash out old SimpleConstructionScript so we can straight up replace it with the new one
		MoveToTransientPackageAndRename(Blueprint->SimpleConstructionScript);

		//Deserialize new SCS, update the flags accordingly and assign it to the blueprint
		//There is no need to duplicate it because it's owner is actually supposed to be the BPGC (for whatever reason)
		UObject* SimpleConstructionScript = GetObjectSerializer()->DeserializeObject(SimpleConstructionScriptIndex);
		SimpleConstructionScript->SetFlags(RF_Transactional);
		Blueprint->SimpleConstructionScript = CastChecked<USimpleConstructionScript>(SimpleConstructionScript);
		
		UpdateDeserializerBlueprintClassObject(true);
		MarkAssetChanged();
	}
}

void UBlueprintGenerator::UpdateDeserializerBlueprintClassObject(bool bRecompileBlueprint) {
	UBlueprint* Blueprint = GetAsset<UBlueprint>();
	if (bRecompileBlueprint) {
		FBlueprintCompilationManager::CompileSynchronously(FBPCompileRequest(Blueprint, EBlueprintCompileOptions::None, NULL));
	}

	UClass* BlueprintGeneratedClass = Blueprint->GeneratedClass;
	GetObjectSerializer()->SetObjectMark(CastChecked<UClass>(BlueprintGeneratedClass), TEXT("$AssetObject$"));
}

void UBlueprintGenerator::PopulateStageDependencies(TArray<FAssetDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		//For construction we want parent class to be FULLY generated
		TArray<FString> ReferencedPackages;
		
		const int32 SuperStructIndex = GetAssetData()->GetIntegerField(TEXT("SuperStruct"));
		GetObjectSerializer()->CollectObjectPackages(SuperStructIndex, ReferencedPackages);

		//Same applies to interfaces, we want them ready by that time too
		TArray<TSharedPtr<FJsonValue>> ImplementedInterfaces = GetAssetData()->GetArrayField(TEXT("Interfaces"));

		for (int32 i = 0; i < ImplementedInterfaces.Num(); i++) {
			const TSharedPtr<FJsonObject> InterfaceObject = ImplementedInterfaces[i]->AsObject();
			const int32 ClassObjectIndex = InterfaceObject->GetIntegerField(TEXT("Class"));
			GetObjectSerializer()->CollectObjectPackages(ClassObjectIndex, ReferencedPackages);
		}

		for (const FString& PackageName : ReferencedPackages) {
			OutDependencies.Add(FAssetDependency{*PackageName, EAssetGenerationStage::CDO_FINALIZATION});
		}
	}
	
	if (GetCurrentStage() == EAssetGenerationStage::DATA_POPULATION) {
		//For data population we want to have all objects referenced by properties fully constructed
		
		const TArray<TSharedPtr<FJsonValue>>& ChildProperties = GetAssetData()->GetArrayField(TEXT("ChildProperties"));
		const TArray<TSharedPtr<FJsonValue>>& Children = GetAssetData()->GetArrayField(TEXT("Children"));
		
		TArray<FString> AllDependencyNames;
		for (const TSharedPtr<FJsonValue>& PropertyPtr : ChildProperties) {
			const TSharedPtr<FJsonObject> PropertyObject = PropertyPtr->AsObject();
			check(PropertyObject->GetStringField(TEXT("FieldKind")) == TEXT("Property"));
			
			FAssetGenerationUtil::GetPropertyDependencies(PropertyObject, GetObjectSerializer(), AllDependencyNames);
		}

		for (int32 i = 0; i < Children.Num(); i++) {
			const TSharedPtr<FJsonObject> FunctionObject = Children[i]->AsObject();
			check(FunctionObject->GetStringField(TEXT("FieldKind")) == TEXT("Function"));
			
			const TArray<TSharedPtr<FJsonValue>> FunctionProperties = FunctionObject->GetArrayField(TEXT("ChildProperties"));
			
			for (int32 j = 0; j < FunctionProperties.Num(); j++) {
				const TSharedPtr<FJsonObject> FunctionProperty = FunctionProperties[j]->AsObject();
				check(FunctionProperty->GetStringField(TEXT("FieldKind")) == TEXT("Property"));
			
				if (FAssetGenerationUtil::IsFunctionSignatureRelevantProperty(FunctionProperty)) {
					FAssetGenerationUtil::GetPropertyDependencies(FunctionProperty, GetObjectSerializer(), AllDependencyNames);
				}
			}
		}
		for (const FString& PackageName : AllDependencyNames) {
			OutDependencies.Add(FAssetDependency{*PackageName, EAssetGenerationStage::CONSTRUCTION});
		}
	}

	if (GetCurrentStage() == EAssetGenerationStage::CDO_FINALIZATION) {
		//For CDO finalization we want to have pretty much everything resolved, primarily CDO and SimpleConstructionScript
		const int32 CDOIndex = GetAssetData()->GetIntegerField(TEXT("ClassDefaultObject"));
		const int32 SCSIndex = GetAssetData()->GetObjectField(TEXT("AssetObjectData"))->GetIntegerField(TEXT("SimpleConstructionScript"));

		TArray<FString> AllDependencyNames;
		GetObjectSerializer()->CollectObjectPackages(CDOIndex, AllDependencyNames);
		GetObjectSerializer()->CollectObjectPackages(SCSIndex, AllDependencyNames);
		
		for (const FString& PackageName : AllDependencyNames) {
        	OutDependencies.Add(FAssetDependency{*PackageName, EAssetGenerationStage::CONSTRUCTION});
        }
	}
}

FName UBlueprintGenerator::GetAssetClass() {
	return UBlueprint::StaticClass()->GetFName();
}

//Never generate __DelegateSignature methods, they are automatically handled
//ExecuteUbergraph methods should also never be generated, since they have corresponding function entries
bool FBlueprintGeneratorUtils::IsFunctionNameGenerated(const FString& FunctionName) {
	return FunctionName.EndsWith(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX) ||
		FunctionName.StartsWith(UEdGraphSchema_K2::FN_ExecuteUbergraphBase.ToString()) ||
		FunctionName.StartsWith(TEXT("SequenceEvent__ENTRYPOINT")) ||
		FunctionName.StartsWith(TEXT("BndEvt__")) ||
		FunctionName.StartsWith(TEXT("InpActEvt_"));
}

//UberGraphFrame properties should never be generated
bool FBlueprintGeneratorUtils::IsPropertyNameGenerated(const FString& PropertyName) {
	return PropertyName == TEXT("UberGraphFrame");
}

void FBlueprintGeneratorUtils::EnsureBlueprintUpToDate(UBlueprint* Blueprint) {
	// Purge any NULL graphs
	FBlueprintEditorUtils::PurgeNullGraphs(Blueprint);

	// Make sure the blueprint is cosmetically up to date
	FKismetEditorUtilities::UpgradeCosmeticallyStaleBlueprint(Blueprint);

	// Make sure that this blueprint is up-to-date with regards to its parent functions
	FBlueprintEditorUtils::ConformCallsToParentFunctions(Blueprint);

	// Make sure that this blueprint is up-to-date with regards to its implemented events
	FBlueprintEditorUtils::ConformImplementedEvents(Blueprint);

	// Make sure that this blueprint is up-to-date with regards to its implemented interfaces
	FBlueprintEditorUtils::ConformImplementedInterfaces(Blueprint);

	// Update old composite nodes(can't do this in PostLoad)
	FBlueprintEditorUtils::UpdateOutOfDateCompositeNodes(Blueprint);

	// Update any nodes which might have dropped their RF_Transactional flag due to copy-n-paste issues
	FBlueprintEditorUtils::UpdateTransactionalFlags(Blueprint);
}

bool FBlueprintGeneratorUtils::ImplementNewInterface(UBlueprint* Blueprint, UClass* InterfaceClass) {
	
	//Check to make sure we haven't already implemented it
	for(int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++) {
		if(Blueprint->ImplementedInterfaces[i].Interface == InterfaceClass) {
			return false;
		}
	}

	//Make sure it is also not implemented by the parent class
	if (Blueprint->ParentClass->ImplementsInterface(InterfaceClass)) {
		return false;
	}

	// Make a new entry for this interface
	FBPInterfaceDescription NewInterface;
	NewInterface.Interface = InterfaceClass;

	// Add the graphs for the functions required by this interface
	for(TFieldIterator<UFunction> FunctionIter(InterfaceClass, EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter) {
		UFunction* Function = *FunctionIter;
		const bool bIsAnimFunction = Function->HasMetaData(FBlueprintMetadata::MD_AnimBlueprintFunction) && Blueprint->IsA<UAnimBlueprint>();
		const bool bShouldImplementAsFunction = UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function);
		
		if(bShouldImplementAsFunction || bIsAnimFunction) {
			FName FunctionName = Function->GetFName();
			UEdGraph* FuncGraph = FindObject<UEdGraph>(Blueprint, *FunctionName.ToString());

			//If it already exists, scrap existing graph and rename it so it does not cause any conflicts
			if (FuncGraph != nullptr) {
				FBlueprintEditorUtils::RemoveGraph(Blueprint, FuncGraph, EGraphRemoveFlags::MarkTransient);
				continue;
			}

			UEdGraph* NewGraph;
			if(bIsAnimFunction) {
				NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
			} else {
				NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			}
			
			NewGraph->bAllowDeletion = false;
			NewGraph->InterfaceGuid = FBlueprintEditorUtils::FindInterfaceFunctionGuid(Function, InterfaceClass);
			NewInterface.Graphs.Add(NewGraph);
			
			FBlueprintEditorUtils::AddInterfaceGraph(Blueprint, NewGraph, InterfaceClass);
		}
	}

	Blueprint->ImplementedInterfaces.Add(NewInterface);
	return true;
}

bool FBlueprintGeneratorUtils::CreateParentFunctionCall(UBlueprint* Blueprint, UFunction* Function, UK2Node* FunctionEntryOrEventNode, UK2Node_FunctionResult* FunctionReturnNode) {
	UFunction* CallableParentFunction = Blueprint->ParentClass->FindFunctionByName(Function->GetFName());

	//We can get NULL if function in question is an interface function, in that case we don't have parent function to call
	//Otherwise we should always get a valid function, since function passed should always come from one of parent classes original declaration
	if (CallableParentFunction == NULL) {
		return false;
	}

	//Spawn parent function call node and set it's function, plus allocate default pins
	UEdGraph* TargetGraph = FunctionEntryOrEventNode->GetGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(TargetGraph->GetSchema());
	
	FGraphNodeCreator<UK2Node_CallParentFunction> FunctionNodeCreator(*TargetGraph);
	
	UK2Node_CallParentFunction* ParentFunctionNode = FunctionNodeCreator.CreateNode();
	ParentFunctionNode->SetFromFunction(CallableParentFunction);
	ParentFunctionNode->AllocateDefaultPins();

	const int32 NewNodeSpawnOffset = 400;
	ParentFunctionNode->NodePosX = FunctionEntryOrEventNode->NodePosX + NewNodeSpawnOffset;
	ParentFunctionNode->NodePosY = FunctionEntryOrEventNode->NodePosY;
	FunctionNodeCreator.Finalize();
	
	//Move function result node to actually match location of newly spawned parent call node
	if (FunctionReturnNode) {
		FunctionReturnNode->NodePosX = FunctionEntryOrEventNode->NodePosX + NewNodeSpawnOffset * 2;
		FunctionReturnNode->NodePosY = FunctionEntryOrEventNode->NodePosY;
	}

	//Connect parent function pins
	for (UEdGraphPin* ParentPin : ParentFunctionNode->Pins) {
		
		//Skip all hidden pins we encounter, they will be filled by kismet compiler automatically
		if (ParentPin->bHidden) {
			continue;
		}

		//Output pins we always want to connect with the function result node
		if (ParentPin->Direction == EGPD_Output) {
		
			//Connect Then pin with the Execute pin of the function result node if we have any
			if (UEdGraphSchema_K2::IsExecPin(*ParentPin)) {
				if (FunctionReturnNode) {
					UEdGraphPin* ReturnNodeExecInputPin = GraphSchema->FindExecutionPin(*FunctionReturnNode, EGPD_Input);
					ParentPin->MakeLinkTo(ReturnNodeExecInputPin);
				}
				continue;
			}

			//Otherwise we should try to find corresponding pin on the function return node
			if (FunctionReturnNode) {
				UEdGraphPin* ReturnInputPin = FunctionReturnNode->FindPin(ParentPin->PinName, EGPD_Input);
				
				if (ReturnInputPin) {
					ReturnInputPin->BreakAllPinLinks(true);
					ReturnInputPin->MakeLinkTo(ParentPin);
				}
			}

		//Input pins we connect with function entry or event node
		} else if (ParentPin->Direction == EGPD_Input) {

			//Connect parent call exec pin with the then pin of the function entry
			if (UEdGraphSchema_K2::IsExecPin(*ParentPin)) {
				UEdGraphPin* FunctionEntryThenPin = GraphSchema->FindExecutionPin(*FunctionEntryOrEventNode, EGPD_Output);
				
				if (FunctionEntryThenPin) {
					FunctionEntryThenPin->BreakAllPinLinks(true);
					FunctionEntryThenPin->MakeLinkTo(ParentPin);
				}
				continue;
			}

			//Otherwise try to find entry pin matching our input pin name
			UEdGraphPin* FunctionEntryPin = FunctionEntryOrEventNode->FindPin(ParentPin->PinName, EGPD_Output);
			if (FunctionEntryPin) {
				FunctionEntryPin->MakeLinkTo(ParentPin);
			}
		}
	}
	return true;
}

UK2Node_Event* FBlueprintGeneratorUtils::CreateCustomEventNode(UBlueprint* Blueprint, const FName& EventName) {
	//Find all event nodes inside of the ubergraph
	TArray<UK2Node_Event*> AllEventNodes;
	for (const UEdGraph* Graph : Blueprint->UbergraphPages) {
		Graph->GetNodesOfClass(AllEventNodes);
	}

	//Make effort to find existing event node first
	for (UK2Node_Event* EventNode : AllEventNodes) {
		if (EventNode->GetFunctionName() == EventName) {
			return EventNode;
		}
	}

	//We cannot really create any events without event graph
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph) {
		return NULL;
	}

	//Allocate node and set custom function name
	FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*EventGraph);
	UK2Node_CustomEvent* CustomEventNode = NodeCreator.CreateNode();
	CustomEventNode->CustomFunctionName = EventName;
	CustomEventNode->bIsEditable = true;

	const FVector2D GoodPlaceForNode = EventGraph->GetGoodPlaceForNewNode();
	CustomEventNode->NodePosX = GoodPlaceForNode.X;
	CustomEventNode->NodePosY = GoodPlaceForNode.Y;
	
	NodeCreator.Finalize();
	return CustomEventNode;
}

UK2Node_FunctionResult* FBlueprintGeneratorUtils::CreateFunctionResultNode(const UK2Node_FunctionEntry* FunctionEntry, bool bAutoConnectNode) {
	UEdGraph* Graph = FunctionEntry->GetGraph();
	const UEdGraphSchema_K2* GraphSchema = CastChecked<UEdGraphSchema_K2>(Graph->GetSchema());
	FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*Graph);
	
	UK2Node_FunctionResult* ReturnNode = NodeCreator.CreateNode();
	ReturnNode->FunctionReference = FunctionEntry->FunctionReference;
	ReturnNode->NodePosX = FunctionEntry->NodePosX + FunctionEntry->NodeWidth + 256;
	ReturnNode->NodePosY = FunctionEntry->NodePosY;
	NodeCreator.Finalize();

	const UFunction* FunctionSignature = FunctionEntry->FindSignatureFunction();
	ReturnNode->CreateUserDefinedPinsForFunctionEntryExit(FunctionSignature, false);

	//Auto-connect execution pins if we have been asked to
	if (bAutoConnectNode) {
		UEdGraphPin* EntryNodeExec = GraphSchema->FindExecutionPin(*FunctionEntry, EGPD_Output);
		UEdGraphPin* ResultNodeExec = GraphSchema->FindExecutionPin(*ReturnNode, EGPD_Input);

		EntryNodeExec->BreakAllPinLinks(true);
		EntryNodeExec->MakeLinkTo(ResultNodeExec);
	}
	return ReturnNode;
}

UK2Node_FunctionEntry* FBlueprintGeneratorUtils::CreateFunctionGraph(UBlueprint* Blueprint, const FName& FunctionName) {
	//If we already have a function graph with the same name, return function from it
	if (const UEdGraph* ExistingGraph = FindObject<UEdGraph>(Blueprint, *FunctionName.ToString())) {
		TArray<UK2Node_FunctionEntry*> FunctionEntries;
		ExistingGraph->GetNodesOfClass(FunctionEntries);
		
		return FunctionEntries[0];
	}
	
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	const UEdGraphSchema_K2* GraphSchema = CastChecked<const UEdGraphSchema_K2>(NewGraph->GetSchema());

	//Create default graph nodes for a new function graph
	GraphSchema->CreateDefaultNodesForGraph(*NewGraph);
	GraphSchema->CreateFunctionGraphTerminators(*NewGraph, (UClass*) NULL);

	//Add default function flags for kismet function, since we do not inherit any flags from parent function
	int32 ExtraFunctionFlags = FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public;
	if (Blueprint->BlueprintType == BPTYPE_FunctionLibrary) {
		ExtraFunctionFlags |= FUNC_Static;
	}
	GraphSchema->AddExtraFunctionFlags(NewGraph, ExtraFunctionFlags);
	
	//Mark function entry as editable because it's a custom function and not an override
	GraphSchema->MarkFunctionEntryAsEditable(NewGraph, true);
	Blueprint->FunctionGraphs.Add(NewGraph);

	TArray<UK2Node_FunctionEntry*> FunctionEntries;
	NewGraph->GetNodesOfClass(FunctionEntries);
	return FunctionEntries[0];
}

UK2Node* FBlueprintGeneratorUtils::CreateFunctionOverride(UBlueprint* Blueprint, UFunction* Function, bool bOverrideAsEvent, bool bCreateParentCall) {
	const FName FunctionName = Function->GetFName();
	UClass* FunctionOwnerClass = Function->GetOuterUClass()->GetAuthoritativeClass();
	
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	
	//Attempt to implement function as event if we can, have event graph and it's desired
	if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function) && EventGraph && bOverrideAsEvent) {
	
		//Return existing event node first, in case of us having it already
		UK2Node_Event* ExistingNode = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, FunctionOwnerClass, FunctionName);
		if (ExistingNode) {
			return ExistingNode;
		}
		
		UK2Node_Event* Event = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_Event>(EventGraph, EventGraph->GetGoodPlaceForNewNode(), EK2NewNodeFlags::None,
			[&](UK2Node_Event* NewInstance){
        	NewInstance->EventReference.SetExternalMember(FunctionName, FunctionOwnerClass);
        	NewInstance->bOverrideFunction = true;
        });

		if (bCreateParentCall) {
			CreateParentFunctionCall(Blueprint, Function, Event, NULL);
		}
		return Event;
	}

	//Otherwise create function override using separate graph
	//Returning existing function node if we already have an override
	if (const UEdGraph* ExistingGraph = FindObject<UEdGraph>(Blueprint, *FunctionName.ToString())) {
		TArray<UK2Node_FunctionEntry*> FunctionEntries;
		ExistingGraph->GetNodesOfClass(FunctionEntries);
		
		return FunctionEntries[0];
	}
		
	//Implement the function graph and spawn default nodes
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	const UEdGraphSchema_K2* GraphSchema = CastChecked<const UEdGraphSchema_K2>(NewGraph->GetSchema());
	
	GraphSchema->CreateDefaultNodesForGraph(*NewGraph);
	GraphSchema->CreateFunctionGraphTerminators(*NewGraph, Function);
	
	Blueprint->FunctionGraphs.Add(NewGraph);

	UK2Node_FunctionEntry* FunctionEntry = NULL;
	UK2Node_FunctionResult* FunctionResult = NULL;

	//Find function entry and result nodes
	for (UEdGraphNode* GraphNode : NewGraph->Nodes) {
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(GraphNode)) {
			FunctionEntry = Entry;
		}
		if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(GraphNode)) {
			FunctionResult = Result;
		}
	}
	checkf(FunctionEntry, TEXT("K2Node_FunctionEntry not found on newly created function graph"));

	//Make parent function call if we've been asked to
	if (bCreateParentCall) {
		CreateParentFunctionCall(Blueprint, Function, FunctionEntry, FunctionResult);
	}
	return FunctionEntry;
}

FName FBlueprintGeneratorUtils::GetFunctionNameForNode(UK2Node* Node) {
	if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node)) {
		return Event->GetFunctionName();
	}
	if (const UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node)) {
		return FunctionEntry->CustomGeneratedFunctionName != NAME_None ? FunctionEntry->CustomGeneratedFunctionName : FunctionEntry->GetGraph()->GetFName();
	}
	return NAME_None;
}

UFunction* FBlueprintGeneratorUtils::FindFunctionInParentClassOrInterfaces(UBlueprint* Blueprint, const FName& FunctionName) {
	//Attempt to find function inside of the implemented interfaces first
	UFunction* ResultFunction = FBlueprintEditorUtils::GetInterfaceFunction(Blueprint, FunctionName);

	//Search up the class hierarchy, we want to find the original declaration of the function
	//because that way we do not depend on overrides being removed or added, and also match the behavior of original BP editor
	if (ResultFunction == NULL) {
		const UClass* ParentClass = Blueprint->ParentClass;

		const UClass* CurrentClass = ParentClass->GetSuperClass();
		ResultFunction = ParentClass->FindFunctionByName(FunctionName);
		
		while (CurrentClass != NULL && ResultFunction != NULL) {
			if (UFunction* ParentFunction = CurrentClass->FindFunctionByName(FunctionName)) {
				ResultFunction = ParentFunction;
			} else {
				break;
			}
			CurrentClass = CurrentClass->GetSuperClass();
		}
	}
	return ResultFunction;
}


bool FBlueprintGeneratorUtils::CreateNewBlueprintFunctions(UBlueprint* Blueprint, const TArray<FDeserializedFunction>& Functions, const TFunctionRef<bool(const FDeserializedFunction& Function)>& FunctionFilter, bool bCreateFunctionOverrides) {
	TSet<FName> ObsoleteBlueprintFunctions;
	bool bChangedFunctionsOrParameters = false;

	//Map all existing functions to their names, taking into account both function overrides and events
	TMap<FName, UK2Node_EditablePinBase*> FunctionAndEventNodes;

	TArray<UEdGraph*> AllBlueprintGraphs;
	Blueprint->GetAllGraphs(AllBlueprintGraphs);

	for (UEdGraph* Graph : AllBlueprintGraphs) {
		for (UEdGraphNode* GraphNode : Graph->Nodes) {
			if (UK2Node_Event* Event = Cast<UK2Node_Event>(GraphNode)) {
				FunctionAndEventNodes.Add(Event->GetFunctionName(), Event);
			}
			if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(GraphNode)) {
				FunctionAndEventNodes.Add(GetFunctionNameForNode(FunctionEntry), FunctionEntry);
			}
		}
	}

	//Populate a list of existing functions and events in the blueprint
	TArray<FName> ObsoleteFunctionNames;
	FunctionAndEventNodes.GetKeys(ObsoleteFunctionNames);
	
	//Create new functions and make sure signature of existing ones matches
	for (const FDeserializedFunction& Function : Functions) {
		ObsoleteFunctionNames.Add(Function.FunctionName);

		//Discard functions that are generated, e.g. ubergraph
		if (IsFunctionNameGenerated(Function.FunctionName.ToString())) {
			continue;
		}
		//Discard functions that don't pass property filter
		if (!FunctionFilter(Function)) {
			continue;
		}

		//Determine whenever we're dealing with function override or new function
		UFunction* OverridenFunction = FindFunctionInParentClassOrInterfaces(Blueprint, Function.FunctionName);

		//If we're overriding a function, then we do not actually need to do anything except than checking that override exists
		if (OverridenFunction) {
			UK2Node_EditablePinBase* const* FunctionImplementation = FunctionAndEventNodes.Find(Function.FunctionName);

			if (FunctionImplementation == NULL && bCreateFunctionOverrides) {
				const bool bShouldGenerateAsEvent = Function.bIsCallingIntoUbergraph && UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(OverridenFunction);
				
				CreateFunctionOverride(Blueprint, OverridenFunction, bShouldGenerateAsEvent, true);
				bChangedFunctionsOrParameters = true;
			}
			continue;
		}
		
		//Try to find existing function entry or event first
		UK2Node_EditablePinBase* FunctionEntryOrEventNode;
		TArray<UK2Node_FunctionResult*> FunctionResultNodes;

		//Try to find existing function or event node first
		if (UK2Node_EditablePinBase* const* ExistingNode = FunctionAndEventNodes.Find(Function.FunctionName)) {
			FunctionEntryOrEventNode = *ExistingNode;

			//Retrieve function result nodes if we're dealing with function graph
			if (const UK2Node_FunctionEntry* FunctionEntry = Cast<const UK2Node_FunctionEntry>(FunctionEntryOrEventNode)) {
				FunctionEntry->GetGraph()->GetNodesOfClass(FunctionResultNodes);
			}
		} else {
			const bool bShouldImplementAsEvent = Function.bIsCallingIntoUbergraph;
			
			if (!bShouldImplementAsEvent) {
				//If we should create new function as an actual function graph, make sure to also create function result node
				UK2Node_FunctionEntry* FunctionEntry = CreateFunctionGraph(Blueprint, Function.FunctionName);
				FunctionEntryOrEventNode = FunctionEntry;
				
				//Create function result node if function has any output parameters
				if (Function.HasAnyOutputParams()) {
					UK2Node_FunctionResult* FunctionResult = CreateFunctionResultNode(FunctionEntry, true);
					FunctionResultNodes.Add(FunctionResult);
				}
			} else {
				//Implement function as event, event functions cannot really have function return nodes
				FunctionEntryOrEventNode = CreateCustomEventNode(Blueprint, Function.FunctionName);
			}
			bChangedFunctionsOrParameters = true;
		}

		//Make sure we have all flags from the deserialized function
		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(FunctionEntryOrEventNode)) {
			if (EventNode->FunctionFlags != Function.FunctionFlags) {
				
				EventNode->FunctionFlags = (Function.FunctionFlags & ~FUNC_Native);
				bChangedFunctionsOrParameters = true;
			}
		} else if (UK2Node_FunctionEntry* FunctionEntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryOrEventNode)) {
			if (FunctionEntryNode->GetExtraFlags() != Function.FunctionFlags) {

				FunctionEntryNode->SetExtraFlags(Function.FunctionFlags);
				bChangedFunctionsOrParameters = true;
			}
		}

		//Make sure parameters on the function declaration stay up to date
		bChangedFunctionsOrParameters |= SetFunctionEntryParameters(FunctionEntryOrEventNode, Function, true);

		//Update parameters on function result nodes if we have any
		if (FunctionResultNodes.Num()) {
			bChangedFunctionsOrParameters |= SetFunctionEntryParameters(FunctionResultNodes[0], Function, false);
		}
	}

	//Remove functions we actually used to implement in the past but no longer do
	for (const FName& ObsoleteFunctionName : ObsoleteBlueprintFunctions) {
		UK2Node* ExistingFunctionNode = FunctionAndEventNodes.FindChecked(ObsoleteFunctionName);
		UEdGraph* NodeGraph = ExistingFunctionNode->GetGraph();

		//We can remove event nodes as long as they're located inside of the ubergraph
		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(ExistingFunctionNode)) {
			if (Blueprint->UbergraphPages.Contains(NodeGraph)) {
				
				FBlueprintEditorUtils::RemoveNode(Blueprint, EventNode, true);
				bChangedFunctionsOrParameters = true;
				continue;
			}
		}

		//For function nodes, we need to remove entire graphs, but make sure they're function graphs beforehand
		if (Cast<UK2Node_FunctionEntry>(ExistingFunctionNode)) {
			if (Blueprint->FunctionGraphs.Contains(NodeGraph)) {
				
				FBlueprintEditorUtils::RemoveGraph(Blueprint, NodeGraph, EGraphRemoveFlags::MarkTransient);
				bChangedFunctionsOrParameters = true;
				continue;
			}
		}

		//Log nodes we couldn't really delete because they are not located in any known places
		UE_LOG(LogAssetGenerator, Error, TEXT("Couldn't delete obsolete function %s inside of the graph %s because Graph is not an ubergraph or function graph"),
			*ObsoleteFunctionName.ToString(), *NodeGraph->GetPathName());
	}

	return bChangedFunctionsOrParameters;
}

bool FBlueprintGeneratorUtils::CreateBlueprintVariablesForProperties(UBlueprint* Blueprint, const TArray<FDeserializedProperty>& Properties,
                                                                     const TMap<FName, FDeserializedFunction>& Functions, const TFunctionRef<bool(const FDeserializedProperty& Property)>& PropertyFilter) {
	TSet<FName> ObsoleteBlueprintProperties;
	bool bChangedProperties = false;

	//Populate a list of all currently existing variables in the blueprint
	for (const FBPVariableDescription& Desc : Blueprint->NewVariables) {
		ObsoleteBlueprintProperties.Add(Desc.VarName);
	}
	
	//Create new variables and update existing ones
	for (const FDeserializedProperty& Property : Properties) {
		ObsoleteBlueprintProperties.Remove(Property.PropertyName);

		//Discard properties that are 100% kismet compiler generated
		if (IsPropertyNameGenerated(Property.PropertyName.ToString())) {
			continue;
		}
		//Discard variables that didn't pass the provided property filter
		if (!PropertyFilter(Property)) {
			continue;
		}
		
		//Try to find existing variable using it's name
		FBPVariableDescription* VariableDescription = FindBlueprintVariableByName(Blueprint, Property.PropertyName);

		//Create new variable if we didn't find existing one
		if (VariableDescription == NULL) {
			VariableDescription = &Blueprint->NewVariables.AddDefaulted_GetRef();

			VariableDescription->VarName = Property.PropertyName;
			VariableDescription->VarGuid = FGuid::NewGuid();
			VariableDescription->FriendlyName = Property.PropertyName.ToString();
			
			VariableDescription->Category = UEdGraphSchema_K2::VR_DefaultCategory;
			VariableDescription->SetMetaData(TEXT("MultiLine"), TEXT("true"));
			bChangedProperties = true;
		} 

		//Apply data to the variable description
		if (VariableDescription->VarType != Property.GraphPinType) {
			VariableDescription->VarType = Property.GraphPinType;
			bChangedProperties = true;
		}
		
		if (VariableDescription->PropertyFlags != (uint64) Property.PropertyFlags) {
			VariableDescription->PropertyFlags = (uint64) Property.PropertyFlags;
			bChangedProperties = true;
		}
		
		if (VariableDescription->RepNotifyFunc != Property.RepNotifyFunc) {
			VariableDescription->RepNotifyFunc = Property.RepNotifyFunc;
			bChangedProperties = true;
		}
		
		if (VariableDescription->ReplicationCondition != Property.BlueprintReplicationCondition) {
			VariableDescription->ReplicationCondition = Property.BlueprintReplicationCondition;
			bChangedProperties = true;
		}
		
		//Make sure multicast delegates actually have valid graphs
		if (VariableDescription->VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate) {
			UEdGraph* DelegateSignatureGraph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(Blueprint, VariableDescription->VarName);
			
			if (DelegateSignatureGraph == NULL) {
				DelegateSignatureGraph = CreateNewBlueprintEventDispatcherSignatureGraph(Blueprint, VariableDescription->VarName);
			}

			//Make sure delegate function parameters are up to date
			TArray<UK2Node_FunctionEntry*> FunctionEntries;
			DelegateSignatureGraph->GetNodesOfClass(FunctionEntries);
			check(FunctionEntries.Num());

			const FString DelegateSignatureFunctionName = FString::Printf(TEXT("%s%s"), *VariableDescription->VarName.ToString(), HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX);
			const FDeserializedFunction& DelegateSignature = Functions.FindChecked(*DelegateSignatureFunctionName);
			
			if (SetFunctionEntryParameters(FunctionEntries[0], DelegateSignature, true)) {
				bChangedProperties = true;
			}
		}
	}

	if (ObsoleteBlueprintProperties.Num()) {
		bChangedProperties = true;
	}

	//Remove delegate signature graphs for obsolete properties
	TArray<UEdGraph*> ObsoleteDelegateSignatureGraphs;
	
	for (UEdGraph* DelegateSignatureGraph : Blueprint->DelegateSignatureGraphs) {
		if (ObsoleteBlueprintProperties.Contains(DelegateSignatureGraph->GetFName())) {
			ObsoleteDelegateSignatureGraphs.Add(DelegateSignatureGraph);
		}
	}
	for (UEdGraph* DelegateSignatureGraph : ObsoleteDelegateSignatureGraphs) {
		FBlueprintEditorUtils::RemoveGraph(Blueprint, DelegateSignatureGraph, EGraphRemoveFlags::MarkTransient);
	}
	
	//Remove obsolete variables descriptors
	Blueprint->NewVariables.RemoveAll([&](const FBPVariableDescription& Desc){
		return ObsoleteBlueprintProperties.Contains(Desc.VarName);
	});
	return bChangedProperties;
}

bool FBlueprintGeneratorUtils::SetFunctionEntryParameters(UK2Node_EditablePinBase* FunctionEntry, const FDeserializedFunction& Function, bool bIsFunctionEntry) {
	TArray<FDeserializedProperty> ParameterArray;

	//Collect all parameters matching our input parameter boolean (because we can be dealing with function result node instead)
	for (const FDeserializedProperty& Parameter : Function.Parameters) {
		if (Parameter.IsFunctionInputParam() == bIsFunctionEntry) {
			ParameterArray.Add(Parameter);
		}
	}
	
	const TArray<TSharedPtr<FUserPinInfo>> OldUserDefinedPins = FunctionEntry->UserDefinedPins;
	bool bNeedToDeleteExistingPins = false;
	
	for (int32 i = 0; i < OldUserDefinedPins.Num(); i++) {
		//Cleanup properties if there are more of them than we expect
		if (!ParameterArray.IsValidIndex(i)) {
			bNeedToDeleteExistingPins = true;
			break;
		}

		const TSharedPtr<FUserPinInfo>& UserPinInfo = OldUserDefinedPins[i];
		const FDeserializedProperty& Property = ParameterArray[i];

		//Cleanup properties if names or pin types of one of them do not match
		if (UserPinInfo->PinName != Property.PropertyName ||
			UserPinInfo->PinType != Property.GraphPinType) {
			bNeedToDeleteExistingPins = true;
			break;
		}
	}
	
	//Cleanup pins if we need to because they overlap with new ones
	if (bNeedToDeleteExistingPins) {
		for (const TSharedPtr<FUserPinInfo>& UserPinInfo : OldUserDefinedPins) {
			FunctionEntry->RemoveUserDefinedPin(UserPinInfo);
		}
	}

	//Add new pins, but skip over the ones we already have except if we cleaned them up
	const int32 FirstPinIndex = bNeedToDeleteExistingPins ? 0 : OldUserDefinedPins.Num();
	
	for (int32 i = FirstPinIndex; i < ParameterArray.Num(); i++) {
		const FDeserializedProperty& Property = ParameterArray[i];
		FunctionEntry->CreateUserDefinedPin(Property.PropertyName, Property.GraphPinType, EGPD_Output);
	}

	//We have changed something as long as first pin index is not the last one
	return FirstPinIndex != ParameterArray.Num();
}

UEdGraph* FBlueprintGeneratorUtils::CreateNewBlueprintEventDispatcherSignatureGraph(UBlueprint* Blueprint, const FName& DispatcherName) {
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, DispatcherName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	checkf(NewGraph, TEXT("Failed to create new graph %s for Event Dispatcher in BLueprint %s"), *DispatcherName.ToString(), *Blueprint->GetPathName());
	
	NewGraph->bEditable = false;
	K2Schema->CreateDefaultNodesForGraph(*NewGraph);
	K2Schema->CreateFunctionGraphTerminators(*NewGraph, (UClass*) NULL);
	
	K2Schema->AddExtraFunctionFlags(NewGraph, FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public);
	K2Schema->MarkFunctionEntryAsEditable(NewGraph, true);

	Blueprint->DelegateSignatureGraphs.Add(NewGraph);
	return NewGraph;
}

FBPVariableDescription* FBlueprintGeneratorUtils::FindBlueprintVariableByName(UBlueprint* Blueprint, const FName& VariableName) {
	for (FBPVariableDescription& VariableDescription : Blueprint->NewVariables) {
		if (VariableDescription.VarName == VariableName) {
			return &VariableDescription;
		}
	}
	return NULL;
}