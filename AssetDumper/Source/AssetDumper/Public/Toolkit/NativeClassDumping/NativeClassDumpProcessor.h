#pragma once
#include "CoreMinimal.h"
#include "Tickable.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNativeClassDumper, All, All);

/** Holds asset dumping related settings */
struct ASSETDUMPER_API FNativeClassDumpSettings {
	FString RootDumpDirectory;
	int32 MaxClassesToProcessInOneTick;
	int32 MaxStructsToProcessInOneTick;
	bool bForceSingleThread;
	bool bOverwriteExistingFiles;
	bool bExitOnFinish;

	/** Default settings for native class dumping */
	FNativeClassDumpSettings();
};

/**
 * This class is responsible for processing native class dumping request
 * Only one instance of this class can be active at a time
 * Global active instance of the native class dump processor can be retrieved through GetActiveInstance() method
 */
class ASSETDUMPER_API FNativeClassDumpProcessor : public FTickableGameObject {
private:
	static TSharedPtr<FNativeClassDumpProcessor> ActiveDumpProcessor;
	
	TArray<UClass*> Classes;
	TArray<UClass*> RemainingClasses;
	TArray<UStruct*> Structs;
	TArray<UStruct*> RemainingStructs;

	int32 ClassesTotal;
	FThreadSafeCounter ClassesSkipped;
	FThreadSafeCounter ClassesProcessed;
	FNativeClassDumpSettings Settings;
	bool bHasFinishedDumping;
	
	explicit FNativeClassDumpProcessor(const FNativeClassDumpSettings& Settings, const TArray<UClass*>& InClasses, const TArray<UStruct*>& InStructs);
public:
	~FNativeClassDumpProcessor();
	
	/** Returns currently active instance of the dump processor. Try not to hold any long-living references to it, as it will prevent it's garbage collection */
	FORCEINLINE static TSharedPtr<FNativeClassDumpProcessor> GetActiveDumpProcessor() { return ActiveDumpProcessor; }

	/** Begins native class dumping with specified settings for provided native class. Crashes when dumping is already in progress */
	static TSharedRef<FNativeClassDumpProcessor> StartNativeClassDump(const FNativeClassDumpSettings& Settings, const TArray<UClass*>& InClasses, const TArray<UStruct*>& InStructs);
	
	/** Returns current progress of the native class dumping */
	FORCEINLINE float GetProgressPct() const { return (ClassesSkipped.GetValue() + ClassesProcessed.GetValue()) / (ClassesTotal * 1.0f); }

	FORCEINLINE int32 GetTotalClasses() const { return ClassesTotal; }
	FORCEINLINE int32 GetClassesSkipped() const { return ClassesSkipped.GetValue(); }
	FORCEINLINE int32 GetClassesProcessed() const { return ClassesProcessed.GetValue(); }
	FORCEINLINE bool IsFinishedDumping() const { return bHasFinishedDumping; }
	
	//Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override;
	//End FTickableGameObject
protected:
	void InitializeNativeClassDump();
	void PerformNativeClassDumpForClass(UClass* Class);
	void PerformNativeClassDumpForStruct(UStruct* Struct);
};