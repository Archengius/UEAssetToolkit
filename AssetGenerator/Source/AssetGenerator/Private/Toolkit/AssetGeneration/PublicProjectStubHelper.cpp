#include "Toolkit/AssetGeneration/PublicProjectStubHelper.h"
#include "Interfaces/IPluginManager.h"

FStubFileInfo FPublicProjectStubHelper::EditorCube(TEXT("EditorCube.fbx"));
FStubFileInfo FPublicProjectStubHelper::DefaultTexture(TEXT("DefaultTexture.png"));
FStubFileInfo FPublicProjectStubHelper::DefaultSkeletalMesh(TEXT("DefaultSkeletalMesh.fbx"));
TStubAssetInfo<USkeleton> FPublicProjectStubHelper::DefaultSkeletalMeshSkeleton(TEXT("/Engine/EditorMeshes/SkeletalMesh/DefaultSkeletalMesh_Skeleton"));

FStubFileInfo::FStubFileInfo(const FString& FileName) {
	this->FullFilePath = FPublicProjectStubHelper::ResolveStubFilePath(FileName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	checkf(PlatformFile.FileExists(*FullFilePath), TEXT("Cannot resolve stub file at path %s"), *FullFilePath);

	const FMD5Hash FileHash = FMD5Hash::HashFile(*FullFilePath);
	this->FileHash = LexToString(FileHash);
}

FString FPublicProjectStubHelper::ResolveStubFilePath(const FString& Filename) {
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AssetGenerator"));
	const FString StubsFolderPath = Plugin->GetBaseDir() / TEXT("Resources") / TEXT("PublicProjectStubs");
	return StubsFolderPath / Filename;
}
