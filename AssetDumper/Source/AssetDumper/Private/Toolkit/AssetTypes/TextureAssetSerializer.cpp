#include "Toolkit/AssetTypes/TextureAssetSerializer.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "Toolkit/AssetTypes/TextureDecompressor.h"
#include "IImageWrapper.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UTextureAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UTexture2D)
    SerializeTexture2D(Asset, Data, Context, TEXT(""));
    END_ASSET_SERIALIZATION
}

void ClearAlphaFromBGRA8Texture(void* TextureData, int32 NumPixels) {
    FColor* TextureDataColor = static_cast<FColor*>(TextureData);

    for (int i = 0; i < NumPixels; i++) {
        FColor* CurrentColor = TextureDataColor++;
        CurrentColor->A = 255;
    }
}

void UTextureAssetSerializer::SerializeTextureData(const FString& ContextString, FTexturePlatformData* PlatformData, TSharedPtr<FJsonObject> Data, TSharedRef<FSerializationContext> Context, bool bResetAlpha, const FString& FileNamePostfix) {
    UEnum* PixelFormatEnum = UTexture2D::GetPixelFormatEnum();

    check(PlatformData);
    check(PlatformData->Mips.Num());

    const EPixelFormat PixelFormat = PlatformData->PixelFormat;
    FTexture2DMipMap& FirstMipMap = PlatformData->Mips[0];
    const FString PixelFormatName = PixelFormatEnum->GetNameStringByValue((int64) PixelFormat);
    
	const int32 NumSlices = PlatformData->GetNumSlices();
    const int32 TextureWidth = FirstMipMap.SizeX;
    const int32 TextureHeight = FirstMipMap.SizeY;
	const int32 TextureDepth = FirstMipMap.SizeZ;
	//check(FirstMipMap.SizeZ == 1);
    
    //Write basic information about texture
    Data->SetNumberField(TEXT("TextureWidth"), TextureWidth);
    Data->SetNumberField(TEXT("TextureHeight"), TextureHeight);
	Data->SetNumberField(TEXT("TextureDepth"), TextureDepth);
    Data->SetNumberField(TEXT("NumSlices"), NumSlices);
    Data->SetStringField(TEXT("CookedPixelFormat"), PixelFormatName);

	//TODO: Different texture classes give different meanings to SizeZ/NumSlices
	//For Texture2D, SizeZ=1 and NumSlices=1
	//For TextureCube, SizeZ=1 and NumSlices=6
	//For Texture2DArray, SizeZ= NumSlices= NumOfTexturesInArray
	const int32 NumTexturesInBulkData = NumSlices;
	
    //When we are operating on one slice only, we can perform some optimizations to avoid unnecessary copying
    const int32 NumBytesPerSlice = FirstMipMap.BulkData.GetBulkDataSize() / NumTexturesInBulkData;

	//Retrieve a copy of the bulk data and use it for decompression
	void* RawCompressedDataCopy = NULL;
	FirstMipMap.BulkData.GetCopy(&RawCompressedDataCopy, false);
	check(RawCompressedDataCopy);

	TArray<uint8> OutDecompressedData;
	uint8* CurrentCompressedData = (uint8*) RawCompressedDataCopy;
		
	//Extract every slice and stitch them into the single texture
	for (int i = 0; i < NumTexturesInBulkData; i++) {
		FString OutErrorMessage;
        
		//Append texture data into the output array, which results in texture being stitched vertically
		const bool bSuccess = FTextureDecompressor::DecompressTextureData(PixelFormat, CurrentCompressedData, TextureWidth, TextureHeight, OutDecompressedData, &OutErrorMessage);

		//Make sure extraction was successful. Theoretically only failure reason would be unsupported format, but we should support most of the used formats
		checkf(bSuccess, TEXT("Failed to extract Texture %s (%dx%d, format %s): %s"), *ContextString, TextureWidth, TextureHeight, *PixelFormatName, *OutErrorMessage);
			
		//Skip amount of bytes read per slice from compressed data buffer
		CurrentCompressedData += NumBytesPerSlice;
	}	
    
    //Free bulk data copy that was allocated by GetCopy call
    FMemory::Free(RawCompressedDataCopy);

    if (bResetAlpha) {
        //Reset alpha if we have been requested to
        const int32 TotalPixelsWithSlices = TextureWidth * TextureHeight * NumTexturesInBulkData;
        ClearAlphaFromBGRA8Texture(OutDecompressedData.GetData(), TotalPixelsWithSlices);
    }

	//Write hash of the source texture filename so asset generator can easily figure out whenever refresh is needed
	FString Hash = FMD5::HashBytes(OutDecompressedData.GetData(), OutDecompressedData.Num() * sizeof(uint8));
	Hash.Append(FString::Printf(TEXT("%x"), OutDecompressedData.Num()));
	Data->SetStringField(TEXT("SourceImageHash"), Hash);

    //Save data in PNG format and store bytes in serialization context
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

    //TextureHeight should be multiplied by amount of splices because we basically stack textures vertically by appending data to the end of buffer
    const int32 ActualTextureHeight = TextureHeight * NumTexturesInBulkData;
    check(ImageWrapper->SetRaw(OutDecompressedData.GetData(), OutDecompressedData.Num(), TextureWidth, ActualTextureHeight, ERGBFormat::BGRA, 8));
    const TArray64<uint8>& PNGResultData = ImageWrapper->GetCompressed();

    //Store data in serialization context
    const FString ImageFilename = Context->GetDumpFilePath(FileNamePostfix, TEXT("png"));
    check(FFileHelper::SaveArrayToFile(PNGResultData, *ImageFilename));
}

void UTextureAssetSerializer::SerializeTexture2D(UTexture2D* Asset, TSharedPtr<FJsonObject> Data, TSharedRef<FSerializationContext> Context, const FString& Postfix) {
    UObjectHierarchySerializer* ObjectSerializer = Context->GetObjectSerializer();
	UPropertySerializer* Serializer = ObjectSerializer->GetPropertySerializer();
	
	DISABLE_SERIALIZATION_RAW(UTexture2D, TEXT("LightingGuid"));
	DISABLE_SERIALIZATION_RAW(UTexture2D, TEXT("ImportedSize"));
	DISABLE_SERIALIZATION(UTexture2D, FirstResourceMemMip);
	
    SERIALIZE_ASSET_OBJECT
    SerializeTextureData(Asset->GetPathName(), Asset->PlatformData, Data, Context, false, Postfix);   
}

FName UTextureAssetSerializer::GetAssetClass() const {
    return UTexture2D::StaticClass()->GetFName();
}
