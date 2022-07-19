#include "Toolkit/AssetTypeGenerator/Texture2DGenerator.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "AssetGeneration/AssetGeneratorSettings.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Toolkit/ObjectHierarchySerializer.h"

void UTexture2DGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UTexture2D* NewTexture = NewObject<UTexture2D>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewTexture);
	
	RebuildTextureData(NewTexture);
}

void UTexture2DGenerator::OnExistingPackageLoaded() {
	UObjectHierarchySerializer* InObjectSerializer = GetObjectSerializer();
	UTexture2D* ExistingTexture = GetAsset<UTexture2D>();

	if (!IsTextureUpToDate(ExistingTexture, InObjectSerializer, GetAssetData(), IsGeneratingPublicProject())) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing source art data for Texture2D %s, signature or properties changed"), *ExistingTexture->GetPathName());
		RebuildTextureData(ExistingTexture);
	}
}

void UTexture2DGenerator::RebuildTextureData(UTexture2D* Texture) {
	const FString ImageFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("png"));
	RebuildTextureData(Texture, ImageFilePath, GetObjectSerializer(), GetAssetData(), IsGeneratingPublicProject());

	MarkAssetChanged();
}


FString UTexture2DGenerator::ComputeTextureHash(UTexture2D* Texture) {
	TArray64<uint8> OutSourceMipMapData;
	check(Texture->Source.GetMipData(OutSourceMipMapData, 0));

	FString TextureHash = FMD5::HashBytes(OutSourceMipMapData.GetData(), OutSourceMipMapData.Num() * sizeof(uint8));
	TextureHash.Append(FString::Printf(TEXT("%llx"), OutSourceMipMapData.Num()));
	return TextureHash;
}

FString ComputeBlankTextureHash(const int64 TextureSize) {
	static TMap<int64, FString> PrecomputedCaches;

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

void FillBlankTextureData(UTexture2D* Texture) {
	uint8* LockedMipData = Texture->Source.LockMip(0);
	
	const int64 MipMapSize = Texture->Source.CalcMipSize(0);
	FMemory::Memset(LockedMipData, 0xFF, MipMapSize);

	Texture->Source.UnlockMip(0);
}

void FillTextureDataFromDump(UTexture2D* Texture, const FString& ImageFilePath) {
	//Read contents of the PNG file provided with the dump and decompress it
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	TArray<uint8> CompressedFileData;
	checkf(FFileHelper::LoadFileToArray(CompressedFileData, *ImageFilePath), TEXT("Failed to read dump image file %s"), *ImageFilePath);

	check(ImageWrapper->SetCompressed(CompressedFileData.GetData(), CompressedFileData.Num() * sizeof(uint8)));
	CompressedFileData.Empty();

	TArray64<uint8> ResultUncompressedData;
	check(ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, ResultUncompressedData));

	//Populate first texture mipmap with the decompressed data from the file
	uint8* LockedMipData = Texture->Source.LockMip(0);
	
	const int64 MipMapSize = Texture->Source.CalcMipSize(0);
	check(ResultUncompressedData.Num() == MipMapSize);
	FMemory::Memcpy(LockedMipData, ResultUncompressedData.GetData(), MipMapSize);

	Texture->Source.UnlockMip(0);
}

void UTexture2DGenerator::RebuildTextureData(UTexture2D* Texture, const FString& TextureFilePath,
	UObjectHierarchySerializer* ObjectSerializer, const TSharedPtr<FJsonObject> AssetData, bool bIsGeneratingPublicProject) {

	const int32 TextureWidth = AssetData->GetIntegerField(TEXT("TextureWidth"));
	const int32 TextureHeight = AssetData->GetIntegerField(TEXT("TextureHeight"));

	//Reinitialize texture data with new dimensions and format
	Texture->Source.Init2DWithMipChain(TextureWidth, TextureHeight, ETextureSourceFormat::TSF_BGRA8);
	
	//Use dump file if we're not doing public project, otherwise use blank texture
	if (!bIsGeneratingPublicProject) {
		FillTextureDataFromDump(Texture, TextureFilePath);
	} else {
		FillBlankTextureData(Texture);
	}

	//Apply settings from the serialized texture object
	const TSharedPtr<FJsonObject> TextureProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));
	ObjectSerializer->DeserializeObjectProperties(TextureProperties.ToSharedRef(), Texture);

	//Disable mips by default if we are not sized appropriately for their generation
	const int32 Log2Int = (int32) FMath::Log2(TextureWidth);
	const int32 ClosestPowerOfTwoSize = 1 << Log2Int;
	
	if (TextureWidth != TextureHeight || TextureWidth != ClosestPowerOfTwoSize) {
		Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	}
	
	//Force no MipMaps policy if specified in the settings
	const UAssetGeneratorSettings* Settings = UAssetGeneratorSettings::Get();
	if (Settings->PackagesToForceNoMipMaps.Contains(Texture->GetOutermost()->GetName())) {
		Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	}

	//Update texture resource, which will update existing resource, invalidate platform data and rebuild it
	Texture->UpdateResource();
}

bool UTexture2DGenerator::IsTextureUpToDate(UTexture2D* ExistingTexture, UObjectHierarchySerializer* ObjectSerializer, const TSharedPtr<FJsonObject> AssetData, const bool bIsPublicProject) {
	const TSharedRef<FJsonObject> TextureProperties = AssetData->GetObjectField(TEXT("AssetObjectData")).ToSharedRef();
	FString SourceFileHash = AssetData->GetStringField(TEXT("SourceImageHash"));
	const FString CurrentFileHash = ComputeTextureHash(ExistingTexture);

	//Override source file hash with blank texture hash if we're doing public project build
	if (bIsPublicProject) {
		const int64 MipMapSize = ExistingTexture->Source.CalcMipSize(0);
		SourceFileHash = ComputeBlankTextureHash(MipMapSize);
	}

	//Return false if texture source art data does not match
	if (SourceFileHash != CurrentFileHash) {
		return false;
	}

	//Make sure object attributes on texture objects match too
	if (!ObjectSerializer->AreObjectPropertiesUpToDate(TextureProperties, ExistingTexture)) {
		return false;
	}
	
	return true;
}

FName UTexture2DGenerator::GetAssetClass() {
	return UTexture2D::StaticClass()->GetFName();
}
