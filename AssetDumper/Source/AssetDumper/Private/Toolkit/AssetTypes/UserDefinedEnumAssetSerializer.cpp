#include "Toolkit/AssetTypes/UserDefinedEnumAssetSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Engine/UserDefinedEnum.h"
#include "Toolkit/AssetTypes/AssetHelper.h"

void UUserDefinedEnumAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UUserDefinedEnum)
     FAssetHelper::SerializeEnum(Data, Asset);

    //Serialize display name map
    TArray<TSharedPtr<FJsonValue>> DisplayNameMap;
    for (const TPair<FName, FText>& DisplayName : Asset->DisplayNameMap) {
    	TSharedPtr<FJsonObject> DisplayNamePair = MakeShareable(new FJsonObject);
    	DisplayNamePair->SetStringField(TEXT("Name"), DisplayName.Key.ToString());
    	DisplayNamePair->SetStringField(TEXT("DisplayName"), DisplayName.Value.ToString());
    	
        DisplayNameMap.Add(MakeShareable(new FJsonValueObject(DisplayNamePair)));
    }
    Data->SetArrayField(TEXT("DisplayNameMap"), DisplayNameMap);
    END_ASSET_SERIALIZATION
}

FName UUserDefinedEnumAssetSerializer::GetAssetClass() const {
    return UUserDefinedEnum::StaticClass()->GetFName();
}
