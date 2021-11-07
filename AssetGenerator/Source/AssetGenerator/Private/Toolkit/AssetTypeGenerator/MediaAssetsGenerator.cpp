#include "Toolkit/AssetTypeGenerator/MediaAssetsGenerator.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Dom/JsonObject.h"

UClass* UMediaPlayerGenerator::GetAssetObjectClass() const {
	return UMediaPlayer::StaticClass();
}

FName UMediaPlayerGenerator::GetAssetClass() {
	return UMediaPlayer::StaticClass()->GetFName();
}

UClass* UMediaTextureGenerator::GetAssetObjectClass() const {
	return UMediaTexture::StaticClass();
}

FName UMediaTextureGenerator::GetAssetClass() {
	return UMediaTexture::StaticClass()->GetFName();
}

UClass* UFileMediaSourceGenerator::GetAssetObjectClass() const {
	return UFileMediaSource::StaticClass();
}

FName UFileMediaSourceGenerator::GetAssetClass() {
	return UFileMediaSource::StaticClass()->GetFName();
}

void UFileMediaSourceGenerator::PopulateSimpleAssetWithData(UObject* Asset) {
	Super::PopulateSimpleAssetWithData(Asset);

	const FString DumpPackageName = GetAssetData()->GetStringField(TEXT("FilePath"));

	FString DumpFilePath = DumpPackageName;
	DumpFilePath.RemoveAt(0);
	const FString FullDumpFilePath = FPaths::Combine(GetRootDumpDirectory(), DumpFilePath);

	FString ResultDestFilePath;
	check(FPackageName::TryConvertLongPackageNameToFilename(DumpPackageName, ResultDestFilePath));

	if (!IsGeneratingPublicProject()) {
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CopyFile(*ResultDestFilePath, *FullDumpFilePath);
	}
}
