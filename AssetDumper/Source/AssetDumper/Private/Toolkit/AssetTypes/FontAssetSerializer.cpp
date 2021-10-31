#include "Toolkit/AssetTypes/FontAssetSerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/TextureAssetSerializer.h"
#include "Engine/Font.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetDumping/AssetDumpProcessor.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

void UFontAssetSerializer::SerializeAsset(TSharedRef<FSerializationContext> Context) const {
    BEGIN_ASSET_SERIALIZATION(UFont)
	DISABLE_SERIALIZATION(UFont, Characters)
	DISABLE_SERIALIZATION(UFont, Textures)
	DISABLE_SERIALIZATION(UFont, IsRemapped)

	//Offline fonts need special treatment during serialization
	if (Asset->FontCacheType == EFontCacheType::Offline) {
		Data->SetBoolField(TEXT("IsOfflineFont"), true);

		//Serialize Font glyph data into separate file, because it's quite big
		const FString GlyphDataFilename = Context->GetDumpFilePath(TEXT("GlyphData"), TEXT("bin"));
		
		const TUniquePtr<FArchive> Writer = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*GlyphDataFilename));
		*Writer << Asset->Characters;
		*Writer << Asset->IsRemapped;
		*Writer << Asset->CharRemap;
		Writer->Close();

		//Serialize offline textures containing glyph data
		TArray<TSharedPtr<FJsonValue>> TexturesArray;
		
		for (UTexture2D* FontTexture : Asset->Textures) {
			const TSharedPtr<FJsonObject> TextureObject = MakeShareable(new FJsonObject());
			TextureObject->SetStringField(TEXT("TextureName"), *FontTexture->GetName());
			
			UTextureAssetSerializer::SerializeTexture2D(FontTexture, TextureObject, Context, FontTexture->GetName());
			TexturesArray.Add(MakeShareable(new FJsonValueObject(TextureObject)));
		}
		Data->SetArrayField(TEXT("Textures"), TexturesArray);
		
		//Disable CompositeFont serialization because it's unused for offline fonts
		DISABLE_SERIALIZATION(UFont, CompositeFont)
	}

	//Runtime fonts are basically collections of referenced FontFace assets and their settings,
	//it would be convenient to list assets this font depends on for asset generation to avoid retrieving it by walking object hierarchy manually
	if (Asset->FontCacheType == EFontCacheType::Runtime) {
		const FCompositeFont& CompositeFont = Asset->CompositeFont;
		Data->SetBoolField(TEXT("IsRuntimeFont"), true);

		TArray<const FTypeface*> AllTypefaces;
		AllTypefaces.Add(&CompositeFont.DefaultTypeface);
		AllTypefaces.Add(&CompositeFont.FallbackTypeface.Typeface);
		for (const FCompositeSubFont& SubFont : CompositeFont.SubTypefaces) {
			AllTypefaces.Add(&SubFont.Typeface);
		}

		TArray<TSharedPtr<FJsonValue>> DependencyPackageNames;
		
		for (const FTypeface* Typeface : AllTypefaces) {
			for (const FTypefaceEntry& TypefaceEntry : Typeface->Fonts) {
				const UObject* AssetObject = TypefaceEntry.Font.GetFontFaceAsset();
				
				if (AssetObject != NULL) {
					const FString PackageName = AssetObject->GetOutermost()->GetName();
					DependencyPackageNames.Add(MakeShareable(new FJsonValueString(PackageName)));
				}
			}
		}
		
		Data->SetArrayField(TEXT("ReferencedFontFacePackages"), DependencyPackageNames);
	}
    
    //Serialize normal asset data now
    SERIALIZE_ASSET_OBJECT
    END_ASSET_SERIALIZATION
}

FName UFontAssetSerializer::GetAssetClass() const {
    return UFont::StaticClass()->GetFName();
}
