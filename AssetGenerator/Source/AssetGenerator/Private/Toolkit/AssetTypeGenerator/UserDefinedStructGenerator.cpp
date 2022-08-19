#include "Toolkit/AssetTypeGenerator/UserDefinedStructGenerator.h"
#include "Dom/JsonObject.h"
#include "Engine/UserDefinedStruct.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetGeneration/AssetGenerationUtil.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

void UUserDefinedStructGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());

	UUserDefinedStruct* NewStruct = NewObject<UUserDefinedStruct>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	check(NewStruct);
	NewStruct->EditorData = NewObject<UUserDefinedStructEditorData>(NewStruct, NAME_None, RF_Transactional);
	check(NewStruct->EditorData);

	const FString StructGuid = GetAssetData()->GetStringField(TEXT("Guid"));
	check(FGuid::Parse(StructGuid, NewStruct->Guid));
	
	NewStruct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
	NewStruct->Bind();
	NewStruct->StaticLink(true);
	NewStruct->Status = UDSS_Error;

	SetPackageAndAsset(NewPackage, NewStruct);

	PopulateStructWithData(NewStruct);
}

void UUserDefinedStructGenerator::OnExistingPackageLoaded() {
	UUserDefinedStruct* ExistingStruct = GetAsset<UUserDefinedStruct>();

	if (!IsStructUpToDate(ExistingStruct)) {
		UE_LOG(LogAssetGenerator, Display, TEXT("UserDefinedStruct %s is not up to date, regenerating data"), *ExistingStruct->GetPathName());

		//Wipe any existing property data from the struct
		UUserDefinedStructEditorData* EditorData = CastChecked<UUserDefinedStructEditorData>(ExistingStruct->EditorData);
		EditorData->VariablesDescriptions.Empty();
		ExistingStruct->Status = UDSS_Error;

		//Regenerate structure variable descriptors from scratch and compile it
		PopulateStructWithData(ExistingStruct);
	}
}

void UUserDefinedStructGenerator::PopulateStructWithData(UUserDefinedStruct* Struct) {
	UUserDefinedStructEditorData* EditorData = CastChecked<UUserDefinedStructEditorData>(Struct->EditorData);
	
	//We ignore StructFlags, because UserDefinedStructs cannot have any manually set flags
	//UserDefinedStructs can never have any SuperStruct definitions or functions, so we validate it here
	check(GetAssetData()->GetIntegerField(TEXT("SuperStruct")) == INDEX_NONE);
	check(GetAssetData()->GetArrayField(TEXT("Children")).Num() == 0);

	const TArray<TSharedPtr<FJsonValue>>& ChildProperties = GetAssetData()->GetArrayField(TEXT("ChildProperties"));

	for (const TSharedPtr<FJsonValue>& PropertyPtr : ChildProperties) {
		const TSharedPtr<FJsonObject> PropertyObject = PropertyPtr->AsObject();
		check(PropertyObject->GetStringField(TEXT("FieldKind")) == TEXT("Property"));

		FStructVariableDescription& VariableDesc = EditorData->VariablesDescriptions.AddDefaulted_GetRef();
		FAssetGenerationUtil::PopulateStructVariable(PropertyObject, GetObjectSerializer(), VariableDesc);
	}

	//Force struct recompilation
	Struct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
    FStructureEditorUtils::CompileStructure(Struct);
	check(Struct->Status == EUserDefinedStructureStatus::UDSS_UpToDate);
	
	MarkAssetChanged();
}

bool UUserDefinedStructGenerator::IsStructUpToDate(UUserDefinedStruct* Struct) const {
	UUserDefinedStructEditorData* EditorData = CastChecked<UUserDefinedStructEditorData>(Struct->EditorData);
	
	const TArray<TSharedPtr<FJsonValue>>& ChildProperties = GetAssetData()->GetArrayField(TEXT("ChildProperties"));
	TMap<FName, FStructVariableDescription> NewVariableDescriptions;

	if (EditorData->VariablesDescriptions.Num() != ChildProperties.Num()) {
		return false;
	}
	
	for (const TSharedPtr<FJsonValue>& PropertyPtr : ChildProperties) {
		const TSharedPtr<FJsonObject> PropertyObject = PropertyPtr->AsObject();
		check(PropertyObject->GetStringField(TEXT("FieldKind")) == TEXT("Property"));
		const FString PropertyName = PropertyObject->GetStringField(TEXT("ObjectName"));

		FStructVariableDescription& VariableDesc = NewVariableDescriptions.Add(FName(*PropertyName));
		FAssetGenerationUtil::PopulateStructVariable(PropertyObject, GetObjectSerializer(), VariableDesc);
	}

	for (const FStructVariableDescription& Description : EditorData->VariablesDescriptions) {
		const FStructVariableDescription* NewDescription = NewVariableDescriptions.Find(Description.VarName);
		if (NewDescription == nullptr ||
			!FAssetGenerationUtil::AreStructDescriptionsEqual(*NewDescription, Description)) {
			return false;
		}
	}
	
	return true;
}

void UUserDefinedStructGenerator::FinalizeAssetCDO() {
	UUserDefinedStruct* Struct = GetAsset<UUserDefinedStruct>();

	//Struct should be up to date at this point (but when generating from BP, it usually isn't)
	if (!Struct->Status == EUserDefinedStructureStatus::UDSS_UpToDate) {
		UE_LOG(LogAssetGenerator, Error, TEXT("Struct %s not up to date!"),
				*Struct->GetPathName());
		return;
	}
	//check(Struct->Status == EUserDefinedStructureStatus::UDSS_UpToDate);
	
	const TSharedPtr<FJsonObject> DefaultInstance = GetAssetData()->GetObjectField(TEXT("StructDefaultInstance"));

	//Allocate struct instance and deserialize default struct instance data into it
	FStructOnScope DeserializedInstance(Struct);
	void* InstanceMemory = DeserializedInstance.GetStructMemory();
	GetPropertySerializer()->DeserializeStruct(Struct, DefaultInstance.ToSharedRef(), InstanceMemory);

	//Record read default values into the map, serializing them as exported text
	TMap<FString, FString> SerializedStructDefaults;
	
	for (FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext) {
		FString ExportedPropertyText;
		const void* PropertyValue = Property->ContainerPtrToValuePtr<void>(InstanceMemory);
		
		Property->ExportTextItem(ExportedPropertyText, PropertyValue, NULL, NULL, PPF_None);
		SerializedStructDefaults.Add(Property->GetName(), ExportedPropertyText);
	}

	//Iterate property descriptors and replace outdated default values
	bool bAppliedAnyChanges = false;
	UUserDefinedStructEditorData* EditorData = CastChecked<UUserDefinedStructEditorData>(Struct->EditorData);

	for (FStructVariableDescription& Description : EditorData->VariablesDescriptions) {
		const FString* NewDefaultValue = SerializedStructDefaults.Find(Description.VarName.ToString());
		const FString& CurrentDefaultValue = Description.DefaultValue;

		if (NewDefaultValue && *NewDefaultValue != CurrentDefaultValue) {
			UE_LOG(LogAssetGenerator, Log, TEXT("Replaced UserDefinedStruct %s Variable %s DefaultValue From '%s' to '%s'"),
				*Struct->GetPathName(), *Description.VarName.ToString(), *CurrentDefaultValue, **NewDefaultValue);
			Description.DefaultValue = *NewDefaultValue;
			bAppliedAnyChanges = true;
		}
	}

	//If we actually applied any changes to default values, recompile struct
	//TODO do we really need to recompile struct on default value changes? can't we just set CurrentDefaultValue and call ReinitializeDefaultInstance?
	if (bAppliedAnyChanges) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshed defaults values of UserDefinedStruct %s"), *Struct->GetPathName());
		Struct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
		FStructureEditorUtils::CompileStructure(Struct);
		
		//check(Struct->Status == EUserDefinedStructureStatus::UDSS_UpToDate);
		MarkAssetChanged();
	}
}

void UUserDefinedStructGenerator::PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		const TArray<TSharedPtr<FJsonValue>>& ChildProperties = GetAssetData()->GetArrayField(TEXT("ChildProperties"));

		TArray<FString> AllDependencyNames;
		for (const TSharedPtr<FJsonValue>& PropertyPtr : ChildProperties) {
			const TSharedPtr<FJsonObject> PropertyObject = PropertyPtr->AsObject();
			FAssetGenerationUtil::GetPropertyDependencies(PropertyObject, GetObjectSerializer(), AllDependencyNames);
		}

		for (const FString& DependencyName : AllDependencyNames) {
			OutDependencies.Add(FPackageDependency{FName(*DependencyName), EAssetGenerationStage::CONSTRUCTION});
		}

	} else if (GetCurrentStage() == EAssetGenerationStage::CDO_FINALIZATION) {
		const TArray<TSharedPtr<FJsonValue>> ReferencedObjects = GetAssetData()->GetArrayField(TEXT("ReferencedObjects"));
		
		TArray<FString> OutReferencedPackages;
		GetObjectSerializer()->CollectReferencedPackages(ReferencedObjects, OutReferencedPackages);

		for (const FString& DependencyPackageName : OutReferencedPackages) {
			OutDependencies.Add(FPackageDependency{*DependencyPackageName, EAssetGenerationStage::CDO_FINALIZATION});
		}
	}
}

FName UUserDefinedStructGenerator::GetAssetClass() {
	return UUserDefinedStruct::StaticClass()->GetFName();
}
