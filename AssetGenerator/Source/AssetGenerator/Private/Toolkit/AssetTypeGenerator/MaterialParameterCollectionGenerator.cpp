#include "Toolkit/AssetTypeGenerator/MaterialParameterCollectionGenerator.h"
#include "Materials/MaterialParameterCollection.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"

void UMaterialParameterCollectionGenerator::PostInitializeAssetGenerator() {
	UPropertySerializer* Serializer = GetPropertySerializer();

	//transient and changed every time material parameter collection is changed
	DISABLE_SERIALIZATION(UMaterialParameterCollection, StateId);
}

UClass* UMaterialParameterCollectionGenerator::GetAssetObjectClass() const {
	return UMaterialParameterCollection::StaticClass();
}

void UMaterialParameterCollectionGenerator::PopulateSimpleAssetWithData(UObject* Asset) {
	Super::PopulateSimpleAssetWithData(Asset);
	UMaterialParameterCollection* ParameterCollection = CastChecked<UMaterialParameterCollection>(Asset);

	//Correct way would be calling CreateBufferStruct() + UpdateParameterCollectionInstances + UpdateDefaultResource
	//like in OnPropertyChanged, but we can just call PostLoad() which will do all of that
	//automatically without us having to access transform MPC class
	ParameterCollection->PostLoad();
}

FName UMaterialParameterCollectionGenerator::GetAssetClass() {
	return UMaterialParameterCollection::StaticClass()->GetFName();
}
