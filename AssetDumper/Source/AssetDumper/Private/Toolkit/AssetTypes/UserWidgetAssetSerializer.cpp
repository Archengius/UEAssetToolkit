#include "Toolkit/AssetTypes/UserWidgetAssetSerializer.h"
#include "Blueprint/WidgetTree.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/BlueprintAssetSerializer.h"
#include "Components/NamedSlot.h"

void UUserWidgetAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION_BP(UWidgetBlueprintGeneratedClass)
    //DISABLE_SERIALIZATION_RAW(UWidgetBlueprintGeneratedClass, "TemplateAsset");
    
    UBlueprintAssetSerializer::SerializeBlueprintClass(Asset, Data, Context);

    //Write list of generated variables that blueprint generator should skip
    TArray<FName> GeneratedVariableNames;
    UBlueprintAssetSerializer::CollectGeneratedVariables(Asset, GeneratedVariableNames);

    //Also append widget tree generated variables
    Asset->GetWidgetTreeArchetype()->ForEachWidget([&](const UWidget* Widget){
        bool bIsVariable = Widget->bIsVariable;
         bIsVariable |= Asset->Bindings.ContainsByPredicate([&Widget] (const FDelegateRuntimeBinding& Binding) {
             return Binding.ObjectName == Widget->GetName();
         });
         bIsVariable |= Widget->IsA<UNamedSlot>();

        if (bIsVariable) {
            GeneratedVariableNames.Add(Widget->GetFName());
        }
    });

    //Also append animations, they always have backing variables too
    for (const UWidgetAnimation* Animation : Asset->Animations) {
        GeneratedVariableNames.Add(Animation->GetFName());
    }
    
    TArray<TSharedPtr<FJsonValue>> GeneratedVariablesArray;
    for (const FName& VariableName : GeneratedVariableNames) {
        GeneratedVariablesArray.Add(MakeShareable(new FJsonValueString(VariableName.ToString())));
    }
    Data->SetArrayField(TEXT("GeneratedVariableNames"), GeneratedVariablesArray);
    
    END_ASSET_SERIALIZATION
}

FName UUserWidgetAssetSerializer::GetAssetClass() const {
    return TEXT("WidgetBlueprint"); //UWidgetBlueprint::StaticClass()->GetFName();
}
