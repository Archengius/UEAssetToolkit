#include "Toolkit/AssetGeneration/AssetGeneratorWidget.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "AssetGeneration/AssetGeneratorLocalSettings.h"
#include "Toolkit/AssetGeneration/AssetDumpViewWidget.h"

#define LOCTEXT_NAMESPACE "AssetGenerator"

SAssetGeneratorWidget::SAssetGeneratorWidget() {
	LocalSettings = UAssetGeneratorLocalSettings::Get();
}

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
	if (LocalSettings->WhitelistedAssetCategories.Num() == 0) {
		return FReply::Handled();
	}

	TSet<FName> WhitelistedAssetCategories = LocalSettings->WhitelistedAssetCategories;
	ExpandWhitelistedAssetCategories(WhitelistedAssetCategories);
	
	TArray<FName> SelectedAssetPackages;
	AssetDumpViewWidget->PopulateSelectedPackages(SelectedAssetPackages, &WhitelistedAssetCategories);

	if (SelectedAssetPackages.Num() == 0) {
		return FReply::Handled();
	}

	FAssetGeneratorConfiguration Configuration;
	Configuration.DumpRootDirectory = GetAssetDumpFolderPath();
	Configuration.bRefreshExistingAssets = LocalSettings->bRefreshExistingAssets;
	Configuration.MaxAssetsToAdvancePerTick = LocalSettings->MaxAssetsToAdvancePerTick;
	Configuration.bGeneratePublicProject = LocalSettings->bGeneratePublicProject;
	
	FAssetGenerationProcessor::CreateAssetGenerator(Configuration, SelectedAssetPackages);
	return FReply::Handled();
}

TSharedRef<SWidget> SAssetGeneratorWidget::CreateAssetTypeFilterCategory()
{
	TSharedRef<SHorizontalBox> AssetTypesHolder = SNew(SHorizontalBox);
	TSharedPtr<SVerticalBox> CurrentVerticalBox;
	int32 AssetTypesStored = 0;
	const int32 MaxAssetTypesPerRow = 8;
	
	TSet<FName> AllAssetClasses;

	for (TSubclassOf<UAssetTypeGenerator> AssetGeneratorClass : UAssetTypeGenerator::GetAllGenerators()) {
		const FName AssetClass = AssetGeneratorClass.GetDefaultObject()->GetAssetClass();
		AllAssetClasses.Add(AssetClass);

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
					return LocalSettings->WhitelistedAssetCategories.Contains(AssetClass) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, AssetClass](const ECheckBoxState NewState) {
					if (NewState == ECheckBoxState::Checked) {
						LocalSettings->WhitelistedAssetCategories.Add(AssetClass);
						LocalSettings->SaveConfig();
						
					} else if (NewState == ECheckBoxState::Unchecked) {
						LocalSettings->WhitelistedAssetCategories.Remove(AssetClass);
						LocalSettings->SaveConfig();
					}
				})
			]
		];
	}

	return SNew(SVerticalBox)
	+SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoHeight()[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoWidth().Padding(10.0f, 5.0f, 5.0f, 10.0f)[
			SNew(SButton)
			.Text(LOCTEXT("AssetTypeFilter_UnselectAll", "Clear All"))
			.OnClicked_Lambda([this](){
				LocalSettings->WhitelistedAssetCategories.Empty();
				LocalSettings->SaveConfig();
				
				return FReply::Handled();
			})
		]
		+SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoWidth().Padding(0.0f, 5.0f, 0.0f, 10.0f)[
			SNew(SButton)
			.Text(LOCTEXT("AssetTypeFilter_SelectAll", "Select All"))
			.OnClicked_Lambda([this, AllAssetClasses](){
				LocalSettings->WhitelistedAssetCategories.Append(AllAssetClasses);
				LocalSettings->SaveConfig();
				
				return FReply::Handled();
			})
		]
		+SHorizontalBox::Slot().VAlign(VAlign_Fill).FillWidth(1.0f)[
			SNew(SSpacer)
		]
	]
	+SVerticalBox::Slot().HAlign(HAlign_Center).VAlign(VAlign_Fill).FillHeight(1.0f).Padding(10.0f)[
		AssetTypesHolder
	];
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
					Arguments.Add(TEXT("Assets"), LocalSettings->MaxAssetsToAdvancePerTick);
					
					return FText::Format(SourceText, Arguments);
				})
			]
			+SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[
				SNew(SSlider)
				.Orientation(Orient_Horizontal)
				.MinValue(1.0f)
				.MaxValue(32.0f)
				.StepSize(1.0f)
				.Value(LocalSettings->MaxAssetsToAdvancePerTick)
				.OnValueChanged_Lambda([this](float NewValue) {
					LocalSettings->MaxAssetsToAdvancePerTick = (int32) NewValue;
					LocalSettings->SaveConfig();
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
					return LocalSettings->bRefreshExistingAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) {
					LocalSettings->bRefreshExistingAssets = NewState == ECheckBoxState::Checked;
					LocalSettings->SaveConfig();
				})
           ]
		]
		+SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.0f, 2.0f))[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center)[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetGenerator_GeneratePublicProject", "Generate Public Project: "))
			]
		   +SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center)[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() {
					return LocalSettings->bGeneratePublicProject ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState) {
					LocalSettings->bGeneratePublicProject = NewState == ECheckBoxState::Checked;
					LocalSettings->SaveConfig();
				})
		   ]
		];
}

void SAssetGeneratorWidget::ExpandWhitelistedAssetCategories(TSet<FName>& WhitelistedAssetCategories) {
	for (TSubclassOf<UAssetTypeGenerator> AssetGeneratorClass : UAssetTypeGenerator::GetAllGenerators()) {
		UAssetTypeGenerator* AssetGenerator = AssetGeneratorClass.GetDefaultObject();
		const FName AssetClass = AssetGenerator->GetAssetClass();

		if (WhitelistedAssetCategories.Contains(AssetClass)) {
			TArray<FName> OutAdditionalClasses;
			AssetGenerator->GetAdditionallyHandledAssetClasses(OutAdditionalClasses);
			WhitelistedAssetCategories.Append(OutAdditionalClasses);
		}
	}
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
	const FString AssetDumpFolderPath = GetAssetDumpFolderPath();
	
 	this->AssetDumpViewWidget->SetAssetDumpRootDirectory(AssetDumpFolderPath);
	this->LocalSettings->CurrentAssetDumpPath = AssetDumpFolderPath;
	this->LocalSettings->SaveConfig();
}

void SAssetGeneratorWidget::SetAssetDumpFolderPath(const FString& InDumpFolderPath) {
	FString NewDumpFolderPath = InDumpFolderPath;
	FPaths::NormalizeDirectoryName(NewDumpFolderPath);
	this->InputDumpPathText->SetText(FText::FromString(NewDumpFolderPath));
	
	UpdateDumpViewRootDirectory();
}

FString SAssetGeneratorWidget::GetDefaultAssetDumpPath() const {
	FString ExistingDumpFolderPath = LocalSettings->CurrentAssetDumpPath;
	
	if (ExistingDumpFolderPath.IsEmpty()) {
		return FPaths::ProjectDir() + TEXT("/AssetDump");
	}
	FPaths::NormalizeDirectoryName(ExistingDumpFolderPath);
	return ExistingDumpFolderPath;
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
