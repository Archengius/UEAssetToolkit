#include "Toolkit/AssetGeneration/AssetGeneratorWidget.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Toolkit/AssetGeneration/AssetDumpViewWidget.h"

#define LOCTEXT_NAMESPACE "AssetGenerator"

void SAssetGeneratorWidget::Construct(const FArguments& InArgs) {
	ChildSlot[
		SNew(SOverlay)
		+SOverlay::Slot()[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()[
				CreateSettingsCategory()
			]
			+SVerticalBox::Slot().AutoHeight()[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("AssetDumper_CatTypes", "Asset Type Filter"))
				.InitiallyCollapsed(true)
				.BodyContent()[
					CreateAssetTypeFilterCategory()
				]
			]
			+SVerticalBox::Slot().FillHeight(1.0f)[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				.ScrollBarAlwaysVisible(true)
				+SScrollBox::Slot()[
					SAssignNew(AssetDumpViewWidget, SAssetDumpViewWidget)
				]
			]
		]
		+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(5.0f, 5.0f)[
			SNew(SButton)
			.Text(LOCTEXT("AssetGenerator_GenerateAssets", "Generate Assets!"))
			.OnClicked_Raw(this, &SAssetGeneratorWidget::OnGenerateAssetsButtonPressed)
			.IsEnabled_Lambda([]() { return !FAssetGenerationProcessor::GetActiveAssetGenerator().IsValid(); })
		]
	];
	UpdateDumpViewRootDirectory();
}

FReply SAssetGeneratorWidget::OnGenerateAssetsButtonPressed() {
	if (WhitelistedAssetClasses.Num() == 0) {
		return FReply::Handled();
	}

	TArray<FName> SelectedAssetPackages;
	AssetDumpViewWidget->PopulateSelectedPackages(SelectedAssetPackages, &WhitelistedAssetClasses);

	if (SelectedAssetPackages.Num() == 0) {
		return FReply::Handled();
	}

	AssetGeneratorSettings.DumpRootDirectory = GetAssetDumpFolderPath();
	FAssetGenerationProcessor::CreateAssetGenerator(AssetGeneratorSettings, SelectedAssetPackages);
	return FReply::Handled();
}

TSharedRef<SWidget> SAssetGeneratorWidget::CreateAssetTypeFilterCategory() {
	TSharedRef<SHorizontalBox> AssetTypesHolder = SNew(SHorizontalBox);
	TSharedPtr<SVerticalBox> CurrentVerticalBox;
	int32 AssetTypesStored = 0;
	const int32 MaxAssetTypesPerRow = 8;

	for (TSubclassOf<UAssetTypeGenerator> AssetGeneratorClass : UAssetTypeGenerator::GetAllGenerators()) {
		const FName AssetClass = AssetGeneratorClass.GetDefaultObject()->GetAssetClass();
		this->WhitelistedAssetClasses.Add(AssetClass);

		if (!CurrentVerticalBox.IsValid() || AssetTypesStored >= MaxAssetTypesPerRow) {
			CurrentVerticalBox = SNew(SVerticalBox);
			AssetTypesHolder->AddSlot()
				.Padding(10.0f, 0.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				[CurrentVerticalBox.ToSharedRef()];
			AssetTypesStored = 0;
		}
		AssetTypesStored++;
		
		CurrentVerticalBox->AddSlot().VAlign(VAlign_Top)[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().VAlign(VAlign_Center).AutoWidth()[
				SNew(SBox)
				.WidthOverride(170)
				.HAlign(HAlign_Left)
				.Content()[
					SNew(STextBlock)
					.Text(FText::FromString(AssetClass.ToString()))
				]
			]
			+SHorizontalBox::Slot().VAlign(VAlign_Center).AutoWidth()[
				SNew(SCheckBox)
				.IsChecked_Lambda([this, AssetClass]() {
					return this->WhitelistedAssetClasses.Contains(AssetClass) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, AssetClass](const ECheckBoxState NewState) {
					if (NewState == ECheckBoxState::Checked) {
						this->WhitelistedAssetClasses.Add(AssetClass);
					} else if (NewState == ECheckBoxState::Unchecked) {
						this->WhitelistedAssetClasses.Remove(AssetClass);
					}
				})
			]
		];
	}
	return AssetTypesHolder;
}

TSharedRef<SWidget> SAssetGeneratorWidget::CreateSettingsCategory() {
	return SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.0f, 2.0f))[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center)[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetGenerator_DumpPath", "Dump Root Folder Path:"))
			]
			+SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(5.0f, 0.0f))[
				SAssignNew(InputDumpPathText, SEditableTextBox)
				.HintText(LOCTEXT("AssetGenerator_DumpPathHint", "Enter path to the dump root folder here..."))
				.Text(FText::FromString(GetDefaultAssetDumpPath()))
				.OnTextCommitted_Lambda([this](const FText&, const ETextCommit::Type) { UpdateDumpViewRootDirectory(); })
			]
			+SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center)[
				SNew(SButton)
				.Text(INVTEXT("..."))
				.OnClicked_Raw(this, &SAssetGeneratorWidget::OnBrowseOutputPathPressed)
			]
		]
		+SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.0f, 5.0f))[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center)[
				SNew(STextBlock)
				.Text_Lambda([this]() {
					const FText SourceText = LOCTEXT("AssetGenerator_AssetsPerTick", "Assets To Generate Per Tick ({Assets}): ");
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Assets"), AssetGeneratorSettings.MaxAssetsToAdvancePerTick);
					
					return FText::Format(SourceText, Arguments);
				})
			]
			+SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[
				SNew(SSlider)
				.Orientation(Orient_Horizontal)
				.MinValue(1.0f)
				.MaxValue(32.0f)
				.StepSize(1.0f)
				.Value(AssetGeneratorSettings.MaxAssetsToAdvancePerTick)
				.OnValueChanged_Lambda([this](float NewValue) {
					AssetGeneratorSettings.MaxAssetsToAdvancePerTick = (int32) NewValue;
				})
			]
		]
		+SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.0f, 2.0f))[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center)[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetGenerator_RefreshAssets", "Refresh Existing Assets: "))
            ]
           +SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center)[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() {
					return AssetGeneratorSettings.bRefreshExistingAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) {
					AssetGeneratorSettings.bRefreshExistingAssets = NewState == ECheckBoxState::Checked;
				})
           ]
		];
}

FString SAssetGeneratorWidget::GetAssetDumpFolderPath() const {
	FString FolderPath = InputDumpPathText->GetText().ToString();
	FPaths::NormalizeDirectoryName(FolderPath);
	
	//Return default path to asset dump if we failed to validate the typed path
	if (FolderPath.IsEmpty() || !FPaths::ValidatePath(*FolderPath)) {
		return GetDefaultAssetDumpPath();
	}

	//Make sure folder path actually ends with a forward slash
	if (FolderPath[FolderPath.Len() - 1] != '/') {
		FolderPath.AppendChar('/');
	}
	
	return FolderPath;
}

void SAssetGeneratorWidget::UpdateDumpViewRootDirectory() {
 	this->AssetDumpViewWidget->SetAssetDumpRootDirectory(GetAssetDumpFolderPath());
}

void SAssetGeneratorWidget::SetAssetDumpFolderPath(const FString& InDumpFolderPath) {
	FString NewDumpFolderPath = InDumpFolderPath;
	FPaths::NormalizeDirectoryName(NewDumpFolderPath);
	this->InputDumpPathText->SetText(FText::FromString(NewDumpFolderPath));
	UpdateDumpViewRootDirectory();
}

FString SAssetGeneratorWidget::GetDefaultAssetDumpPath() {
	return FPaths::ProjectDir() + TEXT("AssetDump/");
}

FReply SAssetGeneratorWidget::OnBrowseOutputPathPressed() {
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(SharedThis(this));
	const FText DialogTitle = LOCTEXT("AssetGenerator_SelectDumpPath", "Select Asset Dump Root Folder");
	
	//Make sure selected directory exists, or directory dialog will fallback to user's root directory
	//Might be a better idea to navigate to parent directory instead, but let's leave it as-is
	const FString CurrentAssetDumpPath = GetAssetDumpFolderPath();
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*CurrentAssetDumpPath);
	
	FString OutDirectoryPath;
	if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, DialogTitle.ToString(), CurrentAssetDumpPath, OutDirectoryPath)) {
		SetAssetDumpFolderPath(OutDirectoryPath);
	}
	return FReply::Handled();
}
