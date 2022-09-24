#include "Toolkit/AssetTypeGenerator/DataTableGenerator.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "UObject/StructOnScope.h"

void UDataTableGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UDataTable* NewDataTable = NewObject<UDataTable>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewDataTable);

	const int32 RowStructIndex = GetAssetData()->GetIntegerField(TEXT("RowStruct"));
	UScriptStruct* RowStruct = CastChecked<UScriptStruct>(GetObjectSerializer()->DeserializeObject(RowStructIndex));
	NewDataTable->RowStruct = RowStruct;
}

void UDataTableGenerator::OnExistingPackageLoaded() {
	UDataTable* ExistingDataTable = GetAsset<UDataTable>();

	const int32 RowStructIndex = GetAssetData()->GetIntegerField(TEXT("RowStruct"));
	UScriptStruct* RowStruct = CastChecked<UScriptStruct>(GetObjectSerializer()->DeserializeObject(RowStructIndex));

	if (ExistingDataTable->RowStruct != RowStruct) {
		UE_LOG(LogAssetGenerator, Log, TEXT("DataTable %s has incorrect RowStruct, replacing it to %s"), *ExistingDataTable->GetPathName(), *RowStruct->GetPathName());
		
		ExistingDataTable->EmptyTable();
		ExistingDataTable->RowStruct = RowStruct;
		MarkAssetChanged();
	}
}

void UDataTableGenerator::PopulateAssetWithData() {
	UPropertySerializer* InPropertySerializer = GetPropertySerializer();
	UDataTable* DataTable = GetAsset<UDataTable>();
	UScriptStruct* RowStruct = DataTable->RowStruct;

	FTableRowMap RowStructs;
	const TSharedPtr<FJsonObject> RowData = GetAssetData()->GetObjectField(TEXT("RowData"));

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : RowData->Values) {
		const TSharedPtr<FStructOnScope> Struct = MakeShareable(new FStructOnScope(RowStruct));
		const TSharedPtr<FJsonObject> StructData = Pair.Value->AsObject();

		InPropertySerializer->DeserializeStruct(RowStruct, StructData.ToSharedRef(), Struct->GetStructMemory());
		RowStructs.Add(*Pair.Key, Struct);
	}

	//If we are using existing package, only populate data when it's not up to date
	if (IsUsingExistingPackage()) {
		if (!IsDataTableUpToDate(DataTable, RowStructs)) {
			UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing DataTable %s because contents changed"), *DataTable->GetPathName());

			DataTable->EmptyTable();
			PopulateDataTableWithData(DataTable, RowStructs);
		}
		
	} else {
		//Otherwise we are making a new package and need full population regardless
		PopulateDataTableWithData(DataTable, RowStructs);
	}
}

void UDataTableGenerator::PopulateDataTableWithData(UDataTable* DataTable, const FTableRowMap& TableRowMap) {

	for (const TPair<FName, TSharedPtr<FStructOnScope>>& Pair : TableRowMap) {
		const TSharedPtr<FStructOnScope> StructValue = Pair.Value;
		const FTableRowBase* TableRowBase = (const FTableRowBase*) StructValue->GetStructMemory();
		DataTable->AddRow(Pair.Key, *TableRowBase);
	}
	MarkAssetChanged();
}

bool UDataTableGenerator::IsDataTableUpToDate(UDataTable* DataTable, const FTableRowMap& TableRowMap) const {
	UScriptStruct* StructType = DataTable->RowStruct;
	const TMap<FName, uint8*>& CurrentRowMap = DataTable->GetRowMap();

	if (CurrentRowMap.Num() != TableRowMap.Num()) {
		return false;
	}

	for (const TPair<FName, uint8*>& ExistingRowPair : CurrentRowMap) {
		const TSharedPtr<FStructOnScope>* NewRowData = TableRowMap.Find(ExistingRowPair.Key);

		if (NewRowData == NULL) {
			return false;
		}
		
		const uint8* NewStructMemory = (*NewRowData)->GetStructMemory();
		const uint8* ExistingStructMemory = ExistingRowPair.Value;

		if (!StructType->CompareScriptStruct(NewStructMemory, ExistingStructMemory, PPF_None)) {
			return false;
		}
	}
	return true;
}

void UDataTableGenerator::PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		TArray<FString> OutReferencedPackages;
		const int32 RowStructObjectIndex = GetAssetData()->GetIntegerField(TEXT("RowStruct"));
		GetObjectSerializer()->CollectObjectPackages(RowStructObjectIndex, OutReferencedPackages);

		for (const FString& DependencyPackageName : OutReferencedPackages) {
			OutDependencies.Add(FPackageDependency{*DependencyPackageName, EAssetGenerationStage::CDO_FINALIZATION});
		}
	}
	if (GetCurrentStage() == EAssetGenerationStage::DATA_POPULATION) {
		const TArray<TSharedPtr<FJsonValue>> ReferencedObjects = GetAssetData()->GetArrayField(TEXT("ReferencedObjects"));
		
		TArray<FString> OutReferencedPackages;
		GetObjectSerializer()->CollectReferencedPackages(ReferencedObjects, OutReferencedPackages);
		
		for (const FString& DependencyPackageName : OutReferencedPackages) {
			OutDependencies.Add(FPackageDependency{*DependencyPackageName, EAssetGenerationStage::CDO_FINALIZATION});
		}
	}
}

FName UDataTableGenerator::GetAssetClass() {
	return UDataTable::StaticClass()->GetFName();
}
