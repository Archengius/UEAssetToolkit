#pragma once
#include "CoreMinimal.h"

class ASSETGENERATOR_API FStubFileInfo {
	FString FullFilePath;
	FString FileHash;
public:
	FStubFileInfo(const FString& FileName);

	FORCEINLINE FString GetFullFilePath() const { return FullFilePath; }
	FORCEINLINE FString GetFileHash() const { return FileHash; }
};

template<typename T>
class TStubAssetInfo {
	T* Object;
public:
	FORCEINLINE TStubAssetInfo(const TCHAR* PackageName) {
		this->Object = LoadObject<T>(NULL, PackageName);
		checkf(Object, TEXT("Failed to load stub %s asset from %s"), *T::StaticClass()->GetName(), PackageName);
		this->Object->AddToRoot();
	}
	
	FORCEINLINE ~TStubAssetInfo() {
		this->Object->RemoveFromRoot();
		this->Object = NULL;
	}

	FORCEINLINE T* GetObject() const { return Object; }
};

class ASSETGENERATOR_API FPublicProjectStubHelper {
public:
	static FString ResolveStubFilePath(const FString& Filename);

	static FStubFileInfo EditorCube;
	static FStubFileInfo DefaultTexture;
	static FStubFileInfo DefaultSkeletalMesh;
	static TStubAssetInfo<USkeleton> DefaultSkeletalMeshSkeleton;
};