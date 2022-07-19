#include "Toolkit/AssetTypes/UserWidgetAssetSerializer.h"
#include "Blueprint/WidgetTree.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "Toolkit/AssetTypes/BlueprintAssetSerializer.h"
#include "Components/NamedSlot.h"
#include "Channels/MovieSceneEvent.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"

void UUserWidgetAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION_BP(UWidgetBlueprintGeneratedClass)
    
    DISABLE_SERIALIZATION(FMovieSceneEvent, Ptrs);
    DISABLE_SERIALIZATION_RAW(UUserWidget, "bHasScriptImplementedTick");
    DISABLE_SERIALIZATION_RAW(UUserWidget, "bHasScriptImplementedPaint");
    
    UBlueprintAssetSerializer::SerializeBlueprintClass(Asset, Data, Context);

    //Write list of generated variables that blueprint generator should skip
    TArray<FName> GeneratedVariableNames;
    UBlueprintAssetSerializer::CollectGeneratedVariables(Asset, GeneratedVariableNames);


    
    //Also append widget tree generated variables
#if ENGINE_MINOR_VERSION >= 26
    Asset->GetWidgetTreeArchetype()->ForEachWidget([&](const UWidget* Widget){
#else
    Asset->WidgetTree->ForEachWidget([&](const UWidget* Widget){
#endif 
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
        
        //BPGC animation names are actually different from the original ones, they have _INST postfix appended
        //We need to strip it to get the original blueprint variable name
        FString OriginalAnimationName = Animation->GetName();
        
        if (OriginalAnimationName.EndsWith(TEXT("_INST"))) {
            OriginalAnimationName = OriginalAnimationName.Mid(0, OriginalAnimationName.Len() - 5);
        }
        GeneratedVariableNames.Add(*OriginalAnimationName);
    }

    //Serialize mapping of movie scene events to their bound functions
    //Since we cannot serialize raw compiled function pointers, we need to just record function names
    TSharedPtr<FJsonObject> MovieSceneEventTriggerSectionFunctions = MakeShareable(new FJsonObject);
    
    for (const UWidgetAnimation* Animation : Asset->Animations) {
        ForEachObjectWithOuter(Animation, [&](UObject* Object){
            if (UMovieSceneEventTriggerSection* EventSection = Cast<UMovieSceneEventTriggerSection>(Object)) {
                FMovieSceneEventChannel& EventChannel = EventSection->EventChannel;
                TArray<TSharedPtr<FJsonValue>> EventChannelValues;

                for (int32 i = 0; i < EventChannel.GetNumKeys(); i++) {
                    const FMovieSceneEvent& MovieSceneEvent = EventChannel.GetData().GetValues()[i];

                    const TSharedPtr<FJsonObject> Value = MakeShareable(new FJsonObject);
                    Value->SetNumberField(TEXT("KeyIndex"), i);
                    Value->SetStringField(TEXT("FunctionName"), MovieSceneEvent.Ptrs.Function->GetName());
                    Value->SetStringField(TEXT("BoundObjectProperty"), MovieSceneEvent.Ptrs.BoundObjectProperty.ToString());

                    EventChannelValues.Add(MakeShareable(new FJsonValueObject(Value)));
                }
                MovieSceneEventTriggerSectionFunctions->SetArrayField(EventSection->GetName(), EventChannelValues);
            }
        });
    }
    Data->SetObjectField(TEXT("MovieSceneEventTriggerSectionFunctions"), MovieSceneEventTriggerSectionFunctions);
    
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
