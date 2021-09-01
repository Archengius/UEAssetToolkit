#include "Toolkit/AssetTypes/UserDefinedStructAssetSerializer.h"
#include "Engine/UserDefinedStruct.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"

void UUserDefinedStructAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UUserDefinedStruct)
    FAssetHelper::SerializeScriptStruct(Data, Asset, ObjectSerializer);

    //Serialize Struct GUID
    Data->SetStringField(TEXT("Guid"), Asset->Guid.ToString());

    //Serialize struct default instance
	TArray<int32> ReferencedSubobjects;
    const TSharedRef<FJsonObject> DefaultInstance = Serializer->SerializeStruct(Asset, Asset->GetDefaultInstance(), &ReferencedSubobjects);
    Data->SetObjectField(TEXT("StructDefaultInstance"), DefaultInstance);

	TArray<TSharedPtr<FJsonValue>> ReferencedSubobjectsArray;
	for (const int32 ObjectIndex : ReferencedSubobjects) {
		ReferencedSubobjectsArray.Add(MakeShareable(new FJsonValueNumber(ObjectIndex)));
	}
	Data->SetArrayField(TEXT("ReferencedObjects"), ReferencedSubobjectsArray);
    
    END_ASSET_SERIALIZATION
}

FName UUserDefinedStructAssetSerializer::GetAssetClass() const {
    return UUserDefinedStruct::StaticClass()->GetFName();
}
