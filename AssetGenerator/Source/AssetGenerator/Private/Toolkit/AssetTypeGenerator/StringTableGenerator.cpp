#include "Toolkit/AssetTypeGenerator/StringTableGenerator.h"
#include "Dom/JsonObject.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Toolkit/ObjectHierarchySerializer.h"

void UStringTableGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
		*GetPackageName().ToString());
	UStringTable* NewStringTable = NewObject<UStringTable>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewStringTable);
	
	PopulateStringTableWithData(NewStringTable);
}

void UStringTableGenerator::OnExistingPackageLoaded() {
	UStringTable* ExistingStringTable = GetAsset<UStringTable>();

	if (!IsStringTableUpToDate(ExistingStringTable)) {
		UE_LOG(LogAssetGenerator, Display, TEXT("StringTable %s is not up to date, regenerating data"), *ExistingStringTable->GetPathName());
		
		ExistingStringTable->GetMutableStringTable()->ClearSourceStrings();
		PopulateStringTableWithData(ExistingStringTable);
	}
}

void UStringTableGenerator::PopulateStringTableWithData(UStringTable* StringTable) {
	const TSharedPtr<FJsonObject> InAssetData = GetAssetData();
	const FStringTableRef MutableStringTable = StringTable->GetMutableStringTable();
	
	const FString TableNamespace = InAssetData->GetStringField(TEXT("TableNamespace"));
	MutableStringTable->SetNamespace(TableNamespace);

	
	const TSharedPtr<FJsonObject> SourceStrings = InAssetData->GetObjectField(TEXT("SourceStrings"));
	
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : SourceStrings->Values) {
		const FString TableKey = Pair.Key;
		const FString DisplayString = Pair.Value->AsString();

		MutableStringTable->SetSourceString(TableKey, DisplayString);
	}
	

	const TSharedPtr<FJsonObject> TableMetadataObjects = InAssetData->GetObjectField(TEXT("MetaData"));
	
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : TableMetadataObjects->Values) {
		const FString TableKey = Pair.Key;
		const TSharedPtr<FJsonObject> MetadataObject = Pair.Value->AsObject();

		for (const TPair<FString, TSharedPtr<FJsonValue>>& MetadataPair : MetadataObject->Values) {
			const FName MetadataKey = *MetadataPair.Key;
			const FString MetadataValue = MetadataPair.Value->AsString();
			
			MutableStringTable->SetMetaData(TableKey, MetadataKey, MetadataValue);
		}
	}
	
	MarkAssetChanged();
}

bool UStringTableGenerator::IsStringTableUpToDate(UStringTable* StringTable) const {
	const FStringTableConstRef StringTableRef = StringTable->GetStringTable();
	const TSharedPtr<FJsonObject> InAssetData = GetAssetData();

	const FString TableNamespace = InAssetData->GetStringField(TEXT("TableNamespace"));
	if (StringTableRef->GetNamespace() != TableNamespace) {
		return false;
	}

	TMap<FString, FString> TableSourceStrings;
	StringTableRef->EnumerateSourceStrings([&](const FString& InKey, const FString& DisplayString){
		TableSourceStrings.Add(InKey, DisplayString);
		return true;
	});

	const TSharedPtr<FJsonObject> SourceStrings = InAssetData->GetObjectField(TEXT("SourceStrings"));
	if (TableSourceStrings.Num() != SourceStrings->Values.Num()) {
		return false;
	}

	for (const TPair<FString, FString>& TablePair : TableSourceStrings) {
		FString JsonDisplayString;
		if (!SourceStrings->TryGetStringField(TablePair.Key, JsonDisplayString) ||
			TablePair.Value != JsonDisplayString) {
			return false;
		}
	}

	const TSharedPtr<FJsonObject> TableMetadataObjects = InAssetData->GetObjectField(TEXT("MetaData"));
	TArray<FString> InStringTableKeys;
	TableSourceStrings.GetKeys(InStringTableKeys);
	
	for (const FString& InKey : InStringTableKeys) {
		TMap<FString, FString> TableMetadataMap;
		StringTableRef->EnumerateMetaData(InKey, [&](FName MetaDataKey, const FString& Value){
			TableMetadataMap.Add(MetaDataKey.ToString(), Value);
			return true;
		});

		if (!TableMetadataObjects->HasField(InKey) && InStringTableKeys.Num()) {
			return false;
		}
		
		const TSharedPtr<FJsonObject> MetaDataObject = TableMetadataObjects->GetObjectField(InKey);
		if (TableMetadataMap.Num() != MetaDataObject->Values.Num()) {
			return false;
		}

		for (const TPair<FString, FString>& MetadataPair : TableMetadataMap) {
			FString MetadataValueString;
			if (!MetaDataObject->TryGetStringField(MetadataPair.Key, MetadataValueString) ||
				MetadataPair.Value != MetadataValueString) {
				return false;
			}
		}
	}
	
	return true;
}

FName UStringTableGenerator::GetAssetClass() {
	return UStringTable::StaticClass()->GetFName();
}
