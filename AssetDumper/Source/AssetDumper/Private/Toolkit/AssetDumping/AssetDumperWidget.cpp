#include "Toolkit/AssetDumping/AssetDumperWidget.h"
#include "Toolkit/AssetDumping/AssetDumpConsoleWidget.h"
#include "Toolkit/AssetDumping/AssetRegistryViewWidget.h"
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Util/FileDialogHelper.h"

#define LOCTEXT_NAMESPACE "AssetDumper"

void SAssetDumperWidget::Construct(const FArguments& InArgs) {
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
					SAssignNew(AssetRegistryViewWidget, SAssetRegistryViewWidget)
				]
			]
		]
		+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(5.0f, 5.0f)[
			SAssignNew(AssetDumpButton, SButton)
			.Text(LOCTEXT("AssetDumper_DumpButton", "Begin Asset Dumping"))
			.IsEnabled_Lambda([]() { return FAssetDumpProcessor::GetActiveDumpProcessor().IsValid() == false; })
			.OnClicked_Raw(this, &SAssetDumperWidget::OnAssetDumpButtonPressed)
		]
	];
}

TSharedRef<SWidget> SAssetDumperWidget::CreateAssetTypeFilterCategory() {
	TSharedRef<SHorizontalBox> AssetTypesHolder = SNew(SHorizontalBox);
	TSharedPtr<SVerticalBox> CurrentVerticalBox;
	int32 AssetTypesStored = 0;
	const int32 MaxAssetTypesPerRow = 8;
	
	for (UAssetTypeSerializer* Serializer : UAssetTypeSerializer::GetAvailableAssetSerializers()) {
		const FName AssetClass = Serializer->GetAssetClass();
		this->AssetClassSerializationRules.Add(AssetClass, Serializer->ShouldSerializeByDefault());

		TArray<FName> AdditionalAssetClasses;
		Serializer->GetAdditionallyHandledAssetClasses(AdditionalAssetClasses);
		this->AdditionalAssetTypes.Add(AssetClass, AdditionalAssetClasses);

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
	                const bool bShouldSerialize = AssetClassSerializationRules.FindChecked(AssetClass);
	                return bShouldSerialize ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	            })
	            .OnCheckStateChanged_Lambda([this, AssetClass](const ECheckBoxState NewState) {
	                const bool bShouldSerialize = NewState == ECheckBoxState::Checked;
	                this->AssetClassSerializationRules.Add(AssetClass, bShouldSerialize);
	            })
	        ]
        ];
	}
	return AssetTypesHolder;
}

TSharedRef<SWidget> SAssetDumperWidget::CreateSettingsCategory() {
	return SNew(SVerticalBox)
	+SVerticalBox::Slot().Padding(FMargin(5.0f, 2.0f)).AutoHeight()[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f)).AutoWidth()[
			SNew(STextBlock)
            .Text(LOCTEXT("AssetDumper_Settings_OutputPath", "Output Folder Path"))
		]
		+SHorizontalBox::Slot().FillWidth(1.0f)[
			SAssignNew(OutputPathText, SEditableTextBox)
			.Text(FText::FromString(AssetDumpSettings.RootDumpDirectory))
			.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitReason){
				AssetDumpSettings.RootDumpDirectory = NewText.ToString();
			})
			.HintText(LOCTEXT("AssetDumper_Settings_OutputPath_Hint", "Type Output Folder Path..."))
		]
		+SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))[
			SNew(SButton)
			.Text(INVTEXT("..."))
			.OnClicked_Raw(this, &SAssetDumperWidget::OnBrowseOutputPathPressed)
		]
    ]
	+SVerticalBox::Slot().Padding(FMargin(5.0f, 2.0f)).AutoHeight()[
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f)).AutoWidth()[
            SNew(STextBlock)
            .Text(LOCTEXT("AssetDumper_Settings_RegenerateAssets", "Overwrite Existing Assets"))
        ]
        +SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center)[
        	SNew(SCheckBox)
        	.ToolTipText(LOCTEXT("AssetDumper_Settings_RegenerateAssets_Tooltip", "When checked, assets that have been dumped before will be overwritten. Otherwise, they are ignored."))
        	.IsChecked_Lambda([this]() {
        		return AssetDumpSettings.bOverwriteExistingAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        	})
        	.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState){
        		AssetDumpSettings.bOverwriteExistingAssets = NewState == ECheckBoxState::Checked;
        	})
        ]
    ]
	+SVerticalBox::Slot().Padding(FMargin(5.0f, 2.0f)).AutoHeight()[
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f)).AutoWidth()[
            SNew(STextBlock)
            .Text(LOCTEXT("AssetDumper_Settings_ForceSingleThread", "Force Single-Threaded"))
        ]
        +SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center)[
            SNew(SCheckBox)
            .ToolTipText(LOCTEXT("AssetDumper_Settings_ForceSingleThread_Tooltip", "Check to disable multithreaded dumping, use if you encounter concurrency-related issues during dumping."))
            .IsChecked_Lambda([this]() {
                return AssetDumpSettings.bForceSingleThread ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState){
                AssetDumpSettings.bForceSingleThread = NewState == ECheckBoxState::Checked;
            })
        ]
    ]
	+SVerticalBox::Slot().Padding(FMargin(5.0f, 2.0f)).AutoHeight()[
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoWidth()[
            SNew(STextBlock)
            .Text(LOCTEXT("AssetDumper_Settings_MaxPackagesTick", "Max Packages to Process Per Tick: "))
        ]
        +SHorizontalBox::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f)).AutoWidth()[
            SNew(STextBlock)
            .Text_Lambda([this]() { return FText::FromString(FString::FromInt(AssetDumpSettings.MaxPackagesToProcessInOneTick)); })
        ]
        +SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Fill).VAlign(VAlign_Center)[
        	SNew(SSlider)
        	.StepSize(1)
        	.MaxValue(64)
        	.MinValue(1)
        	.Value(AssetDumpSettings.MaxPackagesToProcessInOneTick)
        	.ToolTipText(LOCTEXT("AssetDumper_Settings_MaxPackagesTick_Tooltip", "Specifies maximum number of packages to be processed in one tick."))
        	.OnValueChanged_Lambda([this](float NewValue) {
        		AssetDumpSettings.MaxPackagesToProcessInOneTick = (int32) NewValue;
        	})
        ]
    ];
}

FReply SAssetDumperWidget::OnBrowseOutputPathPressed() {
	const TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this));
	const void* ParentWindowHandle = OwnerWindow.IsValid() ? OwnerWindow->GetNativeWindow()->GetOSWindowHandle() : NULL;
	const FString DialogTitle = LOCTEXT("AssetDumper_SelectOutputPath", "Select Asset Dump Output Path").ToString();

	//Make sure currently selected path exists so we can show it in the explorer
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*AssetDumpSettings.RootDumpDirectory);

	FString PickedOutputFolderPath;
    if (FFileDialogHelper::OpenDirectoryDialog(ParentWindowHandle, DialogTitle, AssetDumpSettings.RootDumpDirectory, PickedOutputFolderPath)) {
    	OutputPathText->SetText(FText::FromString(PickedOutputFolderPath));
    	AssetDumpSettings.RootDumpDirectory = PickedOutputFolderPath;
    }
	return FReply::Handled();
}

FReply SAssetDumperWidget::OnAssetDumpButtonPressed() {
	const TSharedRef<FSelectedAssetsStruct> SelectedAssetsStruct(new FSelectedAssetsStruct);
	AssetRegistryViewWidget->PopulateSelectedAssets(SelectedAssetsStruct);
	
	int32 AssetClassesSelected = 0;
	for (const TPair<FName, bool>& AssetCategoryFilterPair : this->AssetClassSerializationRules) {
		if (AssetCategoryFilterPair.Value) {
			SelectedAssetsStruct->AddAssetClassWhitelist(AssetCategoryFilterPair.Key);
			for (const FName& AdditionalClass : this->AdditionalAssetTypes.FindChecked(AssetCategoryFilterPair.Key)) {
				SelectedAssetsStruct->AddAssetClassWhitelist(AdditionalClass);
			}
			AssetClassesSelected++;
		}
	}

	//No asset classes have been selected, abort
	if (AssetClassesSelected == 0) {
		return FReply::Handled();
	}

	//Gather asset data from asset registry, might take a fair bit of time if selection is big enough
	SelectedAssetsStruct->LogSettings();
	SelectedAssetsStruct->GatherAssetsData();

	//Skip dumping if we have nothing to dump that matches all filters
	if (SelectedAssetsStruct->GetGatheredAssets().Num() == 0) {
		return FReply::Handled();
	}
	
	//Start asset dumper with gathered assets and provided settings, and then open console window
	const TSharedRef<FAssetDumpProcessor> Processor = FAssetDumpProcessor::StartAssetDump(AssetDumpSettings, SelectedAssetsStruct->GetGatheredAssets());
	SAssetDumpConsoleWidget::CreateForAssetDumper(Processor);
	
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE