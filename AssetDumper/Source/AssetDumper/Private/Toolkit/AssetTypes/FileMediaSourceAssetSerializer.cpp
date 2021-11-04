#include "Toolkit/AssetTypes/FileMediaSourceAssetSerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"
#include "FileMediaSource.h"

void UFileMediaSourceAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
	BEGIN_ASSET_SERIALIZATION(UFileMediaSource)
	Data->SetStringField(TEXT("PlayerName"), Asset->GetDesiredPlayerName().ToString());
	SERIALIZE_ASSET_OBJECT

	//Carry over referenced media asset from the game to the resulting dump folder
	const FString FullFilePath = Asset->GetFullPath();
	FString FilePackageName;
	check(FPackageName::TryConvertFilenameToLongPackageName(FullFilePath, FilePackageName));
	FilePackageName.Append(FPaths::GetExtension(FullFilePath, true));

	Data->SetStringField(TEXT("FilePath"), FilePackageName);
	const FString ResultFilePath = FPaths::Combine(Context->GetRootOutputDirectory(), FilePackageName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	PlatformFile.CreateDirectory(*FPaths::GetPath(ResultFilePath));
	PlatformFile.CopyFile(*ResultFilePath, *FullFilePath);
	
	END_ASSET_SERIALIZATION
}

FName UFileMediaSourceAssetSerializer::GetAssetClass() const {
	return UFileMediaSource::StaticClass()->GetFName();
}