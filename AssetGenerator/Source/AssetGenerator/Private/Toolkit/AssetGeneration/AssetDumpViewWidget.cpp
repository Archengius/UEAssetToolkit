#include "Toolkit/AssetGeneration/AssetDumpViewWidget.h"
#include "PackageTools.h"
#include "Toolkit/AssetGeneration/AssetTypeGenerator.h"

#define LOCTEXT_NAMESPACE "AssetGenerator"

FString CollectQuotedString(const FString& SourceString, const int32 StartOffset) {
	FString ResultString;
	bool bLastSymbolBackslash = false;
	
	for (int32 i = StartOffset; i < SourceString.Len(); i++) {
		const TCHAR CharAtIndex = SourceString[i];
		if (bLastSymbolBackslash) {
			ResultString.AppendChar(CharAtIndex);
			bLastSymbolBackslash = false;
		} else if (CharAtIndex == '\\') {
			bLastSymbolBackslash = true;
		} else if (CharAtIndex != '"') {
			ResultString.AppendChar(CharAtIndex);
		} else {
			break;
		}
	}
	return ResultString;
}

FAssetDumpTreeNode::FAssetDumpTreeNode() {
	this->bIsLeafNode = false;
	this->bIsChecked = false;
	this->bIsOverridingParentState = false;
	this->bChildrenNodesInitialized = false;
	this->bAssetClassComputed = false;
}

TSharedPtr<FAssetDumpTreeNode> FAssetDumpTreeNode::MakeChildNode() {
	TSharedRef<FAssetDumpTreeNode> NewNode = MakeShareable(new FAssetDumpTreeNode());
	NewNode->ParentNode = SharedThis(this);
	NewNode->RootDirectory = RootDirectory;
	NewNode->bIsChecked = bIsChecked;
	
	Children.Add(NewNode);
	return NewNode;
}

FString FAssetDumpTreeNode::ComputeAssetClass() {
	if (!bIsLeafNode) {
		return TEXT("");
	}
	
	FString FileContentsString;
	if (!FFileHelper::LoadFileToString(FileContentsString, *DiskPackagePath)) {
		UE_LOG(LogAssetGenerator, Error, TEXT("Failed to load asset dump json file %s"), *DiskPackagePath);
		return TEXT("Unknown");
	}

	//Try quick and dirty method that saves us time from parsing JSON fully
	const FString AssetClassPrefix = TEXT("\"AssetClass\": \"");
	const int32 AssetClassIndex = FileContentsString.Find(*AssetClassPrefix);
	if (AssetClassIndex != INDEX_NONE) {
		return CollectQuotedString(FileContentsString, AssetClassIndex + AssetClassPrefix.Len());
	}
	
	UE_LOG(LogAssetGenerator, Warning, TEXT("Quick AssetClass computation method failed for asset file %s"), *DiskPackagePath);

	//Fallback to heavy json file scanning
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FileContentsString), JsonObject)) {
		return JsonObject->GetStringField(TEXT("AssetClass"));
	}

	UE_LOG(LogAssetGenerator, Error, TEXT("Failed to parse asset dump file %s as valid json"), *DiskPackagePath);
	return TEXT("Unknown");
}

void FAssetDumpTreeNode::SetupPackageNameFromDiskPath() {
	//Remove extension from the file path (all asset dump files are json files)
	FString PackageNameNew = FPaths::ChangeExtension(DiskPackagePath, TEXT(""));
	
	//Make path relative to root directory (e.g D:\ProjectRoot\DumpRoot\Game\FactoryGame\Asset -> Game\FactoryGame\Asset)
	const FString RootDirectorySlash = RootDirectory / TEXT("");
	FPaths::MakePathRelativeTo(PackageNameNew, *RootDirectorySlash);
	
	//Normalize path separators to use / instead of backslashes (Game/FactoryGame/Asset)
	PackageNameNew.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	//Make sure package path starts with a forward slash (/Game/FactoryGame/Asset)
	PackageNameNew.InsertAt(0, TEXT('/'));

	//Make sure package name is sanitised before using it
	PackageNameNew = UPackageTools::SanitizePackageName(PackageNameNew);

	this->PackageName = PackageNameNew;
	this->NodeName = PackageNameNew.Len() > 1 ? FPackageName::GetShortName(PackageNameNew) : PackageNameNew;
}

FString FAssetDumpTreeNode::GetOrComputeAssetClass() {
	if (!bAssetClassComputed) {
		this->AssetClass = ComputeAssetClass();
		this->bAssetClassComputed = true;
	}
	return AssetClass;
}

void FAssetDumpTreeNode::RegenerateChildren() {
	if (bIsLeafNode) {
		return;
	}
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> ChildDirectoryNames;
	TArray<FString> ChildFilenames;
	
	PlatformFile.IterateDirectory(*DiskPackagePath, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) {
		if (bIsDirectory) {
			ChildDirectoryNames.Add(FilenameOrDirectory);
		//TODO this should really use a better filtering mechanism than checking file extension, or maybe we could use some custom extension like .uassetdump
		} else if (FPaths::GetExtension(FilenameOrDirectory) == TEXT("json")) {
			ChildFilenames.Add(FilenameOrDirectory);
		}
		return true;
	});

	//Append child directory nodes first, even if they are empty
	for (const FString& ChildDirectoryName : ChildDirectoryNames) {
		const TSharedPtr<FAssetDumpTreeNode> ChildNode = MakeChildNode();
		ChildNode->bIsLeafNode = false;
		ChildNode->DiskPackagePath = ChildDirectoryName;
		ChildNode->SetupPackageNameFromDiskPath();
	}

	//Append filenames then, these represent individual packages
	for (const FString& AssetFilename : ChildFilenames) {
		const TSharedPtr<FAssetDumpTreeNode> ChildNode = MakeChildNode();
		ChildNode->bIsLeafNode = true;
		ChildNode->DiskPackagePath = AssetFilename;
		ChildNode->SetupPackageNameFromDiskPath();
	}
}

void FAssetDumpTreeNode::GetChildrenNodes(TArray<TSharedPtr<FAssetDumpTreeNode>>& OutChildrenNodes) {
	if (!bChildrenNodesInitialized) {
		this->bChildrenNodesInitialized = true;
		RegenerateChildren();
	}
	OutChildrenNodes.Append(Children);
}

void FAssetDumpTreeNode::UpdateSelectedState(bool bIsCheckedNew, bool bIsSetByParent) {
	this->bIsChecked = bIsCheckedNew;
	
	//We reset override state when selected state is updated by parent
	if (bIsSetByParent) {
		this->bIsOverridingParentState = false;
	} else {
		//Otherwise we reset it if it matches parent state or set it to true otherwise
		const TSharedPtr<FAssetDumpTreeNode> ParentNodePinned = ParentNode.Pin();
		const bool bIsParentChecked = ParentNodePinned.IsValid() ? ParentNodePinned->IsChecked() : false;
		//If updated state matches parent state, we should remove override
		if (bIsParentChecked == bIsCheckedNew) {
			this->bIsOverridingParentState = false;
		} else {
			//Otherwise our state differs from the parents one, so we are overriding it
			this->bIsOverridingParentState = true;
		}
	}
	
	//Propagate state update to children widgets
	for (const TSharedPtr<FAssetDumpTreeNode>& ChildNode : Children) {
		ChildNode->UpdateSelectedState(bIsCheckedNew, true);
	}
}

void FAssetDumpTreeNode::PopulateGeneratedPackages(TArray<FName>& OutPackageNames, const TSet<FName>* WhitelistedAssetClasses) {
	if (bIsLeafNode) {
		//If we represent leaf node, append our package name if we are checked
		if (bIsChecked) {
			if (WhitelistedAssetClasses == NULL || WhitelistedAssetClasses->Contains(*GetOrComputeAssetClass())) {
				OutPackageNames.Add(*PackageName);
			}
		}
	} else {
		//Otherwise we represent a directory node. Directory nodes ignore checked flag because children nodes
		//can override it, and just delegate function call to their children
		
		//User cannot really explicitly include children nodes if they are not initialized,
		//so for uninitialized directory nodes, we skip them entirely, unless they're explicitly checked
		if (!bChildrenNodesInitialized) {
			if (!IsChecked()) {
				return;
			}
			RegenerateChildren();
		}

		//And iterate them for them to add generated packages to the list
		for (const TSharedPtr<FAssetDumpTreeNode>& ChildNode : Children) {
			ChildNode->PopulateGeneratedPackages(OutPackageNames, WhitelistedAssetClasses);
		}
	}
}

TSharedPtr<FAssetDumpTreeNode> FAssetDumpTreeNode::CreateRootTreeNode(const FString& DumpDirectory) {
	const TSharedPtr<FAssetDumpTreeNode> RootNode = MakeShareable(new FAssetDumpTreeNode);
	
	RootNode->bIsLeafNode = false;
	RootNode->RootDirectory = DumpDirectory;
	RootNode->DiskPackagePath = RootNode->RootDirectory;
	RootNode->SetupPackageNameFromDiskPath();
	RootNode->UpdateSelectedState(true, false);

	return RootNode;
}

void SAssetDumpViewWidget::Construct(const FArguments& InArgs) {
	ChildSlot[
        SAssignNew(TreeView, STreeView<TSharedPtr<FAssetDumpTreeNode>>)
        .HeaderRow(SNew(SHeaderRow)
                    +SHeaderRow::Column(TEXT("ShouldGenerate"))
                        .DefaultLabel(LOCTEXT("AssetGenerator_ColumnShouldGenerate", "Generate"))
                        .FixedWidth(70)
                        .HAlignCell(HAlign_Center)
                        .HAlignHeader(HAlign_Center)
                    +SHeaderRow::Column(TEXT("Path"))
                        .DefaultLabel(LOCTEXT("AssetGenerator_ColumnPath", "Asset Path"))
                    +SHeaderRow::Column(TEXT("AssetClass"))
                        .DefaultLabel(LOCTEXT("AssetGenerator_ColumnAssetClass", "Asset Class"))
                        .FixedWidth(100)
                        .HAlignCell(HAlign_Left)
                        .HAlignHeader(HAlign_Center)
        )
        .SelectionMode(ESelectionMode::None)
        .OnGenerateRow_Raw(this, &SAssetDumpViewWidget::OnCreateRow)
        .OnGetChildren_Raw(this, &SAssetDumpViewWidget::GetNodeChildren)
        .TreeItemsSource(&this->RootAssetPaths)
    ];
}

void SAssetDumpViewWidget::SetAssetDumpRootDirectory(const FString& RootDirectory) {
	this->RootNode = FAssetDumpTreeNode::CreateRootTreeNode(RootDirectory);
	this->RootAssetPaths.Empty();
	this->RootNode->GetChildrenNodes(RootAssetPaths);
	this->TreeView->RequestTreeRefresh();
}

void SAssetDumpViewWidget::PopulateSelectedPackages(TArray<FName>& OutPackageNames, const TSet<FName>* WhitelistedAssetClasses) const {
	this->RootNode->PopulateGeneratedPackages(OutPackageNames, WhitelistedAssetClasses);
}

TSharedRef<ITableRow> SAssetDumpViewWidget::OnCreateRow(const TSharedPtr<FAssetDumpTreeNode> TreeNode,
	const TSharedRef<STableViewBase>& Owner) const {
	return SNew(SAssetDumpTreeNodeRow, Owner, TreeNode);
}

void SAssetDumpViewWidget::GetNodeChildren(const TSharedPtr<FAssetDumpTreeNode> TreeNode, TArray<TSharedPtr<FAssetDumpTreeNode>>& OutChildren) const {
	TreeNode->GetChildrenNodes(OutChildren);
}

void SAssetDumpTreeNodeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<FAssetDumpTreeNode>& TreeNodeArg) {
	this->TreeNode = TreeNodeArg;
	FSuperRowType::Construct(FTableRowArgs(), OwnerTable);
}

TSharedRef<SWidget> SAssetDumpTreeNodeRow::GenerateWidgetForColumn(const FName& InColumnName) {
	if (InColumnName == TEXT("Path")) {
		return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[
            SNew(SExpanderArrow, SharedThis(this))
        ]
        +SHorizontalBox::Slot().FillWidth(1.0f)[
            SNew(STextBlock)
            .Text(FText::FromString(TreeNode->NodeName))
        ];
	}
	if (InColumnName == TEXT("AssetClass")) {
		if (TreeNode->bIsLeafNode) {
			return SNew(STextBlock)
                .Text(FText::FromString(TreeNode->GetOrComputeAssetClass()));
		}
		return SNullWidget::NullWidget;
	}
	return SNew(SCheckBox)
        .IsChecked_Lambda([this]() {
            return TreeNode->IsChecked() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) {
            const bool bIsChecked = NewState == ECheckBoxState::Checked;
            TreeNode->UpdateSelectedState(bIsChecked, false);
        });
}

#undef LOCTEXT_NAMESPACE