#include "Toolkit/AssetTypes/DataTableAssetSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Engine/DataTable.h"

void UDataTableAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UDataTable)

	TArray<int32> ReferencedSubobjects;
    const TSharedPtr<FJsonObject> RowData = MakeShareable(new FJsonObject());
    const TMap<FName, uint8*>& RowDataMap = Asset->GetRowMap();
	
    for (const TPair<FName, uint8*>& RowDataPair : RowDataMap) {
        TSharedPtr<FJsonObject> StructData = Serializer->SerializeStruct(Asset->RowStruct, RowDataPair.Value, &ReferencedSubobjects);
        RowData->SetObjectField(RowDataPair.Key.ToString(), StructData);
    }

	const int32 RowStructIndex = ObjectSerializer->SerializeObject(Asset->RowStruct);
	ReferencedSubobjects.Add(RowStructIndex);

	TArray<TSharedPtr<FJsonValue>> ReferencedSubobjectsArray;
	for (const int32 ObjectIndex : ReferencedSubobjects) {
		ReferencedSubobjectsArray.Add(MakeShareable(new FJsonValueNumber(ObjectIndex)));
	}

    Data->SetNumberField(TEXT("RowStruct"), RowStructIndex);
    Data->SetObjectField(TEXT("RowData"), RowData);
	Data->SetArrayField(TEXT("ReferencedObjects"), ReferencedSubobjectsArray);

    END_ASSET_SERIALIZATION
}

FName UDataTableAssetSerializer::GetAssetClass() const {
    return UDataTable::StaticClass()->GetFName();
}
