#pragma once
#include "AssetGenerationProcessor.h"
#include "Slate.h"

class SAssetGeneratorWidget : public SCompoundWidget {
public:
	SAssetGeneratorWidget();
	
	SLATE_BEGIN_ARGS(SAssetGeneratorWidget) {}
	SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
protected:
	class UAssetGeneratorLocalSettings* LocalSettings;
	TSharedPtr<class SAssetDumpViewWidget> AssetDumpViewWidget;
	TSharedPtr<class SEditableTextBox> InputDumpPathText;

	TSharedRef<SWidget> CreateAssetTypeFilterCategory();
	TSharedRef<SWidget> CreateSettingsCategory();

	static void ExpandWhitelistedAssetCategories(TSet<FName>& WhitelistedAssetCategories);
	FString GetAssetDumpFolderPath() const;
	void UpdateDumpViewRootDirectory();
	void SetAssetDumpFolderPath(const FString& InDumpFolderPath);
	FString GetDefaultAssetDumpPath() const;
	
	FReply OnBrowseOutputPathPressed();
	FReply OnGenerateAssetsButtonPressed();
};
