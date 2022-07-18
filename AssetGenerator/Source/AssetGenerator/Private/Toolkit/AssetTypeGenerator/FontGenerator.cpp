#include "Toolkit/AssetTypeGenerator/FontGenerator.h"
#include "Engine/Font.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/AssetTypeGenerator/Texture2DGenerator.h"

void UFontGenerator::ReadGlyphDataFromFile(FFontGlyphData& GlyphData) const {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	
	if (AssetData->HasField(TEXT("IsOfflineFont")) && AssetData->GetBoolField(TEXT("IsOfflineFont"))) {
		const FString GlyphDataFilename = GetAdditionalDumpFilePath(TEXT("GlyphData"), TEXT("bin"));
		
		const TUniquePtr<FArchive> Reader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*GlyphDataFilename));
		*Reader << GlyphData.Characters;
		*Reader << GlyphData.bIsRemapped;
		*Reader << GlyphData.CharRemap;
		Reader->Close();
	}
}

void UFontGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UFont* NewFont = NewObject<UFont>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, NewFont);

	FFontGlyphData FontGlyphData;
	ReadGlyphDataFromFile(FontGlyphData);
	
	PopulateFontAssetWithData(NewFont, FontGlyphData);
}

void UFontGenerator::OnExistingPackageLoaded() {
	UFont* ExistingFont = GetAsset<UFont>();

	FFontGlyphData FontGlyphData;
	ReadGlyphDataFromFile(FontGlyphData);

	if (!IsFontUpToDate(ExistingFont, FontGlyphData)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing asset data for Font %s"), *ExistingFont->GetPathName());
		PopulateFontAssetWithData(ExistingFont, FontGlyphData);
	}
}

void UFontGenerator::PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		const TSharedPtr<FJsonObject> AssetData = GetAssetData();

		if (AssetData->HasField(TEXT("IsRuntimeFont")) && AssetData->GetBoolField(TEXT("IsRuntimeFont"))) {
			
			const TArray<TSharedPtr<FJsonValue>> Dependencies = AssetData->GetArrayField(TEXT("ReferencedFontFacePackages"));	
			for (const TSharedPtr<FJsonValue>& Value : Dependencies) {
				OutDependencies.Add(FPackageDependency{*Value->AsString(), EAssetGenerationStage::CONSTRUCTION});
			}
		}
	}
}

void UFontGenerator::PopulateFontAssetWithData(UFont* Font, const FFontGlyphData& GlyphData) {
	UObjectHierarchySerializer* ObjectSerializer = GetObjectSerializer();
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectData = AssetData->GetObjectField(TEXT("AssetObjectData"));

	//Flush normal data values into the asset right now
	//This will also deserialize CompositeFont data, which is enough to completely refresh runtime cached font
	ObjectSerializer->DeserializeObjectProperties(AssetObjectData.ToSharedRef(), Font);

	//If we are dealing with the offline cached font, we need to flush some stuff manually
	//and then refresh source art for texture objects
	if (Font->FontCacheType == EFontCacheType::Offline) {
		
		//Clear composite font data because offline cached fonts should never have it
		Font->CompositeFont = FCompositeFont();

		//Deserialize glyph data into the font
		Font->Characters = GlyphData.Characters;
		Font->IsRemapped = GlyphData.bIsRemapped;
		Font->CharRemap = GlyphData.CharRemap;

		//Empty Textures array for us to re-populate it right now with new textures
		Font->Textures.Empty();
		
		const TArray<TSharedPtr<FJsonValue>> Textures = AssetData->GetArrayField(TEXT("Textures"));

		//Iterate textures array now and add new ones
		for (int32 i = 0; i < Textures.Num(); i++) {
			const TSharedPtr<FJsonObject> TextureData = Textures[i]->AsObject();
			const FString TextureName = TextureData->GetStringField(TEXT("TextureName"));

			//Here comes the interesting thing: even though we cleared Textures, UTexture2D objects might've not been
			//garbage collected yet, so in this case we first try to find the existing object,
			//and only attempt to make new one afterwards
			UTexture2D* Texture = FindObjectFast<UTexture2D>(Font, *TextureName);
			
			//Note that we also don't mark texture as RF_Standalone,
			//as it does not represent root asset and just a part of primary asset. RF_Public is needed because
			//texture can be referenced directly from the materials (according to Font Editor at least)
			if (Texture == NULL) {
				Texture = NewObject<UTexture2D>(Font, *TextureName, RF_Public);
			}

			//Rebuild texture data using UTexture2DGenerator methods
			const FString ImageFilename = GetAdditionalDumpFilePath(TextureName, TEXT("png"));
			UTexture2DGenerator::RebuildTextureData(Texture, ImageFilename, ObjectSerializer, TextureData);

			//Finally add it into the textures array
			Font->Textures.Add(Texture);
		}
	}

	//Call PostLoad on the asset so it will update textures for offline font and set the correct compression settings
	Font->PostLoad();
}

bool FontCharacterEqual(const FFontCharacter& A, const FFontCharacter& B) {
	return A.StartU == B.StartU &&
		A.StartV == B.StartV &&
		A.TextureIndex == B.TextureIndex &&
		A.USize == B.USize &&
		A.VSize == B.VSize &&
		A.VerticalOffset == B.VerticalOffset;
}

bool FontCharacterArrayEqual(const TArray<FFontCharacter>& A, const TArray<FFontCharacter>& B) {
	if (A.Num() != B.Num()) {
		return false;
	}
	for (int32 i = 0; i < A.Num(); i++) {
		if (!FontCharacterEqual(A[i], B[i])) {
			return false;
		}
	}
	return true;
}

bool UFontGenerator::IsFontUpToDate(UFont* Font, const FFontGlyphData& GlyphData) const {
	UObjectHierarchySerializer* ObjectSerializer = GetObjectSerializer();
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectData = AssetData->GetObjectField(TEXT("AssetObjectData"));

	//If present data does not match, asset is not up to date regardless
	//It will also compare CompositeFont data present in the asset, so it basically handles everything except
	//offline font data (textures, characters and char remap map)
	if (!ObjectSerializer->AreObjectPropertiesUpToDate(AssetObjectData.ToSharedRef(), Font)) {
		return false;
	}

	//If we are dealing with the offline font, we need to read glyph data from the file and compare it with asset glyph data
	if (Font->FontCacheType == EFontCacheType::Offline) {

		//Check textures first, make sure their amount is the same
		const TArray<TSharedPtr<FJsonValue>> Textures = AssetData->GetArrayField(TEXT("Textures"));
		if (Font->Textures.Num() != Textures.Num()) {
			return false;
		}

		//Then compare actual texture object properties and source art hash
		for (int32 i = 0; i < Textures.Num(); i++) {
			UTexture2D* Texture2D = Font->Textures[i];
			const TSharedPtr<FJsonObject> TextureData = Textures[i]->AsObject();

			//Make sure texture names match
			const FString TextureName = TextureData->GetStringField(TEXT("TextureName"));
			if (Texture2D->GetName() != TextureName) {
				return false;
			}

			//Perform standard texture comparison using Texture2DGenerator methods
			if (!UTexture2DGenerator::IsTextureUpToDate(Texture2D, ObjectSerializer, TextureData)) {
				return false;
			}
		}

		//Check glyph data and make sure characters, char remap map and is remapped boolean match
		if (FontCharacterArrayEqual(Font->Characters, GlyphData.Characters) ||
			!Font->CharRemap.OrderIndependentCompareEqual(GlyphData.CharRemap) ||
			((bool) Font->IsRemapped) != GlyphData.bIsRemapped) {
			return false;
		}
	}
	
	return true;
}

FName UFontGenerator::GetAssetClass() {
	return UFont::StaticClass()->GetFName();
}
