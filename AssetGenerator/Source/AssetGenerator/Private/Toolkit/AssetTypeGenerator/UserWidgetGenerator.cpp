#include "Toolkit/AssetTypeGenerator/UserWidgetGenerator.h"
#include "Blueprint/WidgetTree.h"
#include "Channels/MovieSceneEvent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UMGEditor/Public/WidgetBlueprint.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Dom/JsonObject.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"

void UUserWidgetGenerator::PostInitializeAssetGenerator() {
	//TEMPFIX: Fix for old dumps that have this data serialized, we should never attempt to load it
	UPropertySerializer* Serializer = GetPropertySerializer();
	DISABLE_SERIALIZATION(FMovieSceneEvent, Ptrs);

	//TEMPFIX: Disable deserialization of these 2 properties, they are set by the editor automatically
	DISABLE_SERIALIZATION_RAW(UUserWidget, "bHasScriptImplementedTick");
	DISABLE_SERIALIZATION_RAW(UUserWidget, "bHasScriptImplementedPaint");
}

UBlueprint* UUserWidgetGenerator::CreateNewBlueprint(UPackage* Package, UClass* ParentClass) {
	EBlueprintType BlueprintType = BPTYPE_Normal;
	
	if (ParentClass == UInterface::StaticClass()) {
		BlueprintType = BPTYPE_Interface;
	}
	return CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, GetAssetName(),
		BlueprintType, UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
}

void UUserWidgetGenerator::FinalizeAssetCDO() {
	Super::FinalizeAssetCDO();

	UWidgetBlueprint* WidgetBlueprint = GetAsset<UWidgetBlueprint>();
	const TSharedPtr<FJsonObject> AssetObjectData = GetAssetData()->GetObjectField(TEXT("AssetObjectData"));

	const int32 WidgetTreeObject = AssetObjectData->GetIntegerField(TEXT("WidgetTree"));
	const bool bWidgetTreeChanged = !GetObjectSerializer()->CompareUObjects(WidgetTreeObject, WidgetBlueprint->WidgetTree, false, false);

	if (bWidgetTreeChanged) {
		//Deserialize widget tree as it was present on the generated class
		UObject* NewWidgetTree = GetObjectSerializer()->DeserializeObject(WidgetTreeObject);

		//Trash out old widget tree so it does not interfere with the newly generated one
		MoveToTransientPackageAndRename(WidgetBlueprint->WidgetTree);
		
		//Since deserialized widget tree is the original tree copied to BPGC, renamed and with flags changed,
		//we need to duplicate it with the correct name, outer and flags, and only then assign to the blueprint
		UObject* DuplicatedWidgetTree = DuplicateObject(NewWidgetTree, WidgetBlueprint, TEXT("WidgetTree"));
		DuplicatedWidgetTree->ClearFlags(RF_Transient);
		DuplicatedWidgetTree->SetFlags(RF_Public | RF_DefaultSubObject | RF_Transactional | RF_ArchetypeObject);
		
		WidgetBlueprint->WidgetTree = CastChecked<UWidgetTree>(DuplicatedWidgetTree);

		UpdateDeserializerBlueprintClassObject(true);
		MarkAssetChanged();
	}

	const TArray<TSharedPtr<FJsonValue>> Animations = AssetObjectData->GetArrayField(TEXT("Animations"));
	bool bWidgetAnimationsChanged = true;
	
	if (Animations.Num() == WidgetBlueprint->Animations.Num()) {
		bWidgetAnimationsChanged = false;
		
		for (int32 i = 0; i < Animations.Num(); i++) {
			UObject* Animation = WidgetBlueprint->Animations[i];
			const int32 AnimationObjectIndex = (int32) Animations[i]->AsNumber();

			const TSharedRef<FObjectCompareContext> CompareContext = MakeShareable(new FObjectCompareContext);
			CompareContext->SetObjectSettings(AnimationObjectIndex, FObjectCompareSettings(false, false));

			if (!GetObjectSerializer()->CompareObjectsWithContext(AnimationObjectIndex, Animation, CompareContext)) {
				bWidgetAnimationsChanged = true;
				break;
			}
		}
	}

	if (bWidgetAnimationsChanged) {
		//Rename old animations to temporary names, plus clear their array
		for (UObject* AnimationObject : WidgetBlueprint->Animations) {
			MoveToTransientPackageAndRename(AnimationObject);
		}
		WidgetBlueprint->Animations.Empty();

		// Deserializing the animation will find these ones, so we move them to avoid that. The class will be regenerated afterwards anyway
		UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass);
		for (UObject* AnimationObject : GeneratedClass->Animations) {
			MoveToTransientPackageAndRename(AnimationObject);
		}

		//Deserialize new animations into the array directly
		for (const TSharedPtr<FJsonValue>& Animation : Animations) {
			UObject* AnimationObject = GetObjectSerializer()->DeserializeObject((int32) Animation->AsNumber());

			//Rename animation object because it has the _INST suffix emitted by the blueprint compiler
			const FString AnimationName = AnimationObject->GetName();
			if (AnimationName.EndsWith(TEXT("_INST"))) {
				AnimationObject->Rename(*AnimationName.Mid(0, AnimationName.Len() - 5), WidgetBlueprint, REN_DoNothing);
			}
			WidgetBlueprint->Animations.Add(CastChecked<UWidgetAnimation>(AnimationObject));
		}

		UpdateDeserializerBlueprintClassObject(true);
		MarkAssetChanged();
	}
}

UClass* UUserWidgetGenerator::GetFallbackParentClass() const {
	return UUserWidget::StaticClass();
}

FName UUserWidgetGenerator::GetAssetClass() {
	return TEXT("WidgetBlueprint");
}

void UUserWidgetGenerator::PopulateStageDependencies(TArray<FPackageDependency>& AssetDependencies) const {
	Super::PopulateStageDependencies(AssetDependencies);

	if (GetCurrentStage() == EAssetGenerationStage::CDO_FINALIZATION) {
		TArray<FString> AdditionalWidgetDependencies;
		const TSharedPtr<FJsonObject> AssetObjectData = GetAssetData()->GetObjectField(TEXT("AssetObjectData"));

		const int32 WidgetTreeObject = AssetObjectData->GetIntegerField(TEXT("WidgetTree"));
		GetObjectSerializer()->CollectObjectPackages(WidgetTreeObject, AdditionalWidgetDependencies);

		const TArray<TSharedPtr<FJsonValue>> Animations = AssetObjectData->GetArrayField(TEXT("Animations"));
		for (const TSharedPtr<FJsonValue>& AnimationValue : Animations) {
			GetObjectSerializer()->CollectObjectPackages((int32) AnimationValue->AsNumber(), AdditionalWidgetDependencies);
		}

		for (const FString& PackageName : AdditionalWidgetDependencies) {
			AssetDependencies.Add(FPackageDependency{*PackageName, EAssetGenerationStage::CDO_FINALIZATION});
		}
	}
}

