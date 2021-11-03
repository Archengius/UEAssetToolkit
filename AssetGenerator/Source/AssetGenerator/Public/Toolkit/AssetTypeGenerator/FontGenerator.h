#pragma once
#include "CoreMinimal.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"
#include "FontGenerator.generated.h"

/** Describes glyph data for the offline font */
struct FFontGlyphData {
	TArray<struct FFontCharacter> Characters;
	bool bIsRemapped;
	TMap<uint16, uint16> CharRemap;
};

UCLASS(MinimalAPI)
class UFontGenerator : public UAssetTypeGenerator {
	GENERATED_BODY()
protected:
	void ReadGlyphDataFromFile(FFontGlyphData& GlyphData) const;
	virtual void CreateAssetPackage() override;
	virtual void OnExistingPackageLoaded() override;
	void PopulateFontAssetWithData(class UFont* Font, const FFontGlyphData& GlyphData);
	bool IsFontUpToDate(class UFont* Font, const FFontGlyphData& GlyphData) const;
public:
	virtual FName GetAssetClass() override;
	virtual void PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const override;
};