#include "Toolkit/AssetTypeGenerator/TextureGenerator.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Toolkit/ObjectHierarchySerializer.h"

void UTextureGenerator::CreateAssetPackage() {
	UPackage* Package = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UTexture* Texture = NewObject<UTexture>(Package, GetTextureClass(), GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(Package, Texture);

	UpdateTextureInfo(Texture);
	UpdateTextureSource(Texture);
}

void UTextureGenerator::OnExistingPackageLoaded() {
	UTexture* Asset = GetAsset<UTexture>();
	
	UObjectHierarchySerializer* ObjectSerializer = GetObjectSerializer();
	const TSharedPtr<FJsonObject> AssetData = GetAssetObjectData();

	if (!ObjectSerializer->AreObjectPropertiesUpToDate(AssetData, Asset)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refresing Texture %s properties"), *Asset->GetPathName());
		UpdateTextureInfo(Asset);
	}

	const FString ExistingTextureHash = ComputeTextureHash(Asset);

	const int32 TextureWidth = GetAssetData()->GetIntegerField(TEXT("TextureWidth"));
	const int32 TextureHeight = GetAssetData()->GetIntegerField(TEXT("TextureHeight"));
	const int32 NumSlices = GetAssetData()->GetIntegerField(TEXT("NumSlices"));
	
	FString NewTextureHash;
	if (!IsGeneratingPublicProject()) {
		NewTextureHash = GetAssetData()->GetStringField(TEXT("SourceImageHash"));
	} else {
		NewTextureHash = ComputeBlankTextureHash(TextureWidth, TextureHeight, NumSlices);
	}

	if (ExistingTextureHash != NewTextureHash) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing source art for Texture %s"), *Asset->GetPathName());
		UpdateTextureSource(Asset);
	}
}

void UTextureGenerator::UpdateTextureInfo(UTexture* Texture) {
	GetObjectSerializer()->DeserializeObjectProperties(GetAssetObjectData(), Texture);
	MarkAssetChanged();
}

void UTextureGenerator::UpdateTextureSource(UTexture* Texture) {
	const int32 TextureWidth = GetAssetData()->GetIntegerField(TEXT("TextureWidth"));
	const int32 TextureHeight = GetAssetData()->GetIntegerField(TEXT("TextureHeight"));
	const int32 NumSlices = GetAssetData()->GetIntegerField(TEXT("NumSlices"));

	Texture->Source.Init(TextureWidth, TextureHeight, NumSlices, 1, TSF_BGRA8);

	if (!IsGeneratingPublicProject()) {
		SetTextureSourceToDumpFile(Texture);
	} else {
		SetTextureSourceToWhite(Texture);
	}
	Texture->UpdateResource();
	MarkAssetChanged();
}

void UTextureGenerator::SetTextureSourceToDumpFile(UTexture* Texture) {
	const FString ImageFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("png"));
	
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	TArray<uint8> CompressedFileData;
	checkf(FFileHelper::LoadFileToArray(CompressedFileData, *ImageFilePath), TEXT("Failed to read dump image file %s"), *ImageFilePath);

	check(ImageWrapper->SetCompressed(CompressedFileData.GetData(), CompressedFileData.Num() * sizeof(uint8)));
	CompressedFileData.Empty();

	TArray64<uint8> ResultUncompressedData;
	check(ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, ResultUncompressedData));
	
	uint8* LockedMipData = Texture->Source.LockMip(0);
	const int64 MipMapSize = Texture->Source.CalcMipSize(0);
	check(ResultUncompressedData.Num() == MipMapSize);
	
	FMemory::Memcpy(LockedMipData, ResultUncompressedData.GetData(), MipMapSize);
	Texture->Source.UnlockMip(0);
}

FString UTextureGenerator::ComputeTextureHash(UTexture* Texture) {
	TArray64<uint8> OutSourceMipMapData;
	check(Texture->Source.GetMipData(OutSourceMipMapData, 0));

	FString TextureHash = FMD5::HashBytes(OutSourceMipMapData.GetData(), OutSourceMipMapData.Num() * sizeof(uint8));
	TextureHash.Append(FString::Printf(TEXT("%llx"), OutSourceMipMapData.Num()));
	return TextureHash;
}

void UTextureGenerator::SetTextureSourceToWhite(UTexture* Texture) {
	const int64 MipMapSize = Texture->Source.CalcMipSize(0);
	uint8* LockedMipData = Texture->Source.LockMip(0);

	FMemory::Memset(LockedMipData, 0xFF, MipMapSize);
	Texture->Source.UnlockMip(0);
}

FString UTextureGenerator::ComputeBlankTextureHash(int32 Width, int32 Height, int32 NumTextures) {
	static TMap<int64, FString> PrecomputedCaches;
	const int64 TextureSize = Width * Height * NumTextures;

	if (!PrecomputedCaches.Contains(TextureSize)) {
		TArray64<uint8> BlankDataArray;
		BlankDataArray.AddUninitialized(TextureSize);
		FMemory::Memset(BlankDataArray.GetData(), 0xFF, TextureSize);

		FString TextureHash = FMD5::HashBytes(BlankDataArray.GetData(), BlankDataArray.Num() * sizeof(uint8));
		TextureHash.Append(FString::Printf(TEXT("%llx"), BlankDataArray.Num()));

		PrecomputedCaches.Add(TextureSize, TextureHash);
		return TextureHash;
	}
	return PrecomputedCaches.FindChecked(TextureSize);	
}


