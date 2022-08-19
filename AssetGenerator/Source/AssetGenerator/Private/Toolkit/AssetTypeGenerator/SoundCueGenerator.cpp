#include "Toolkit/AssetTypeGenerator/SoundCueGenerator.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraph/EdGraphNode.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "SoundCueGraph/SoundCueGraph.h"
#include "SoundCueGraph/SoundCueGraphNode.h"
#include "SoundCueGraph/SoundCueGraphNode_Base.h"
#include "SoundCueGraph/SoundCueGraphNode_Root.h"
#include "SoundCueGraph/SoundCueGraphSchema.h"
#include "Sound/SoundWave.h"
#include "Sound/DialogueWave.h"
#include "Sound/SoundCue.h"
#include "Components/AudioComponent.h"
#include "AudioEditorModule.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "ScopedTransaction.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphUtilities.h"
#include "SNodePanel.h"
#include "Editor.h"
#include "SoundCueGraphEditorCommands.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/GenericCommands.h"
#include "Sound/SoundNodeDialoguePlayer.h"
#include "HAL/PlatformApplicationMisc.h"
#include "AudioDeviceManager.h"
#include "Audio/AudioDebug.h"
#include <Editor/UnrealEd/Public/EdGraphUtilities.h>
#include <Editor/GraphEditor/Public/SNodePanel.h>

#include "Dom/JsonObject.h"
#include <Editor/AudioEditor/Private/SoundCueEditor.h>

UClass* USoundCueGenerator::GetAssetObjectClass()  const {
	return USoundCue::StaticClass();
}

FName USoundCueGenerator::GetAssetClass() {
	return USoundCue::StaticClass()->GetFName();
}

void USoundCueGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	USoundCue* SoundCue = NewObject<USoundCue>(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	SetPackageAndAsset(NewPackage, SoundCue);

	SoundCue->GetGraph()->Modify();
	SoundCue->Modify();
	
	const FString TextToImport = GetAssetData()->GetStringField(TEXT("SoundCueGraph")).ReplaceEscapedCharWithChar();
	
	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(SoundCue->GetGraph(), TextToImport, /*out*/ PastedNodes);
	
	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
	
		if (USoundCueGraphNode* SoundGraphNode = Cast<USoundCueGraphNode>(Node))
		{
			SoundCue->AllNodes.Add(SoundGraphNode->SoundNode);
		}
	
		if (Node->NodePosX == 0 && Node->NodePosY == 0) {
			for (UEdGraphNode* node : SoundCue->GetGraph()->Nodes)
			{
				if (USoundCueGraphNode_Root* root = Cast<USoundCueGraphNode_Root>(node)) {
					Node->FindPin(TEXT("Output"), EGPD_Output)->MakeLinkTo(root->Pins[0]);
				}
			}
			
		}
		Node->NodePosX = Node->NodePosX - 300;
		Node->SnapToGrid(SNodePanel::GetSnapGridSize());
	
		// Give new node a different Guid from the old one
		Node->CreateNewGuid();
	}
	
	// Force new pasted SoundNodes to have same connections as graph nodes
	SoundCue->CompileSoundNodesFromGraphNodes();

	// Update UI
	//SoundCueGraphEditor->NotifyGraphChanged();

	SoundCue->PostEditChange();
	SoundCue->MarkPackageDirty();
	
	PopulateSimpleAssetWithData(SoundCue);
}