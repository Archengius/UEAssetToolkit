#include "Toolkit/NativeClassDumping/NativeClassDumpProcessor.h"
#include "Async/ParallelFor.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"

DEFINE_LOG_CATEGORY(LogNativeClassDumper)

FNativeClassDumpSettings::FNativeClassDumpSettings() :
		RootDumpDirectory(FPaths::ProjectDir() + TEXT("NativeClassDump/")),
        MaxClassesToProcessInOneTick(FPlatformMisc::NumberOfCores() / 2),
        MaxStructsToProcessInOneTick(FPlatformMisc::NumberOfCores() / 2),
        bForceSingleThread(false),
        bOverwriteExistingFiles(true),
		bExitOnFinish(false) {
}

TSharedPtr<FNativeClassDumpProcessor> FNativeClassDumpProcessor::ActiveDumpProcessor = NULL;

FNativeClassDumpProcessor::FNativeClassDumpProcessor(const FNativeClassDumpSettings& Settings, const TArray<UClass*>& InClasses, const TArray<UStruct*>& InStructs) {
	this->Settings = Settings;
	this->Classes = InClasses;
	this->Structs = InStructs;
	InitializeNativeClassDump();
}

FNativeClassDumpProcessor::~FNativeClassDumpProcessor() {		
	this->RemainingClasses.Empty();
}

TSharedRef<FNativeClassDumpProcessor> FNativeClassDumpProcessor::StartNativeClassDump(const FNativeClassDumpSettings& Settings, const TArray<UClass*>& InClasses, const TArray<UStruct*>& InStructs) {
	checkf(!ActiveDumpProcessor.IsValid(), TEXT("StartNativeClassDump is called while another native class dump is in progress"));
	
	TSharedRef<FNativeClassDumpProcessor> NewProcessor = MakeShareable(new FNativeClassDumpProcessor(Settings, InClasses, InStructs));
	ActiveDumpProcessor = NewProcessor;
	return NewProcessor;
}

void FNativeClassDumpProcessor::Tick(float DeltaTime) {	
	//Process pending dump requests in parallel for loop
	TArray<UClass*, TInlineAllocator<16>> ClassesToProcessThisTick;
	ClassesToProcessThisTick.Reserve(Settings.MaxClassesToProcessInOneTick);
	
	const int32 ClassElementsToCopy = FMath::Min(RemainingClasses.Num(), Settings.MaxClassesToProcessInOneTick);
	ClassesToProcessThisTick.Append(RemainingClasses.GetData(), ClassElementsToCopy);
	RemainingClasses.RemoveAt(0, ClassElementsToCopy, false);
	
	TArray<UStruct*, TInlineAllocator<16>> StructsToProcessThisTick;
	StructsToProcessThisTick.Reserve(Settings.MaxStructsToProcessInOneTick);
	
	const int32 StructElementsToCopy = FMath::Min(RemainingStructs.Num(), Settings.MaxStructsToProcessInOneTick);
	StructsToProcessThisTick.Append(RemainingStructs.GetData(), StructElementsToCopy);
	RemainingStructs.RemoveAt(0, StructElementsToCopy, false);

	if (ClassesToProcessThisTick.Num()) {
		EParallelForFlags ParallelFlags = EParallelForFlags::Unbalanced;
		if (Settings.bForceSingleThread) {
			ParallelFlags |= EParallelForFlags::ForceSingleThread;
		}
		ParallelFor(ClassesToProcessThisTick.Num(), [this, &ClassesToProcessThisTick](const int32 PackageIndex) {
			PerformNativeClassDumpForClass(ClassesToProcessThisTick[PackageIndex]);
		}, ParallelFlags);
	}

	if (StructsToProcessThisTick.Num()) {
		EParallelForFlags ParallelFlags = EParallelForFlags::Unbalanced;
		if (Settings.bForceSingleThread) {
			ParallelFlags |= EParallelForFlags::ForceSingleThread;
		}
		ParallelFor(StructsToProcessThisTick.Num(), [this, &StructsToProcessThisTick](const int32 PackageIndex) {
			PerformNativeClassDumpForStruct(StructsToProcessThisTick[PackageIndex]);
		}, ParallelFlags);
	}

	if (RemainingClasses.Num() == 0 && RemainingStructs.Num() == 0) {
		UE_LOG(LogNativeClassDumper, Display, TEXT("Native class dumping finished successfully"));
		this->bHasFinishedDumping = true;

		//If we were requested to exit on finish, do it now
		if (Settings.bExitOnFinish) {
			UE_LOG(LogNativeClassDumper, Display, TEXT("Exiting because bExitOnFinish was set to true in native class dumper settings..."));
			FPlatformMisc::RequestExit(false);
		}

		//If we represent an active dump processor, make sure to reset ourselves once dumping is done
		if (ActiveDumpProcessor.Get() == this) {
			ActiveDumpProcessor.Reset();
		}
	}
}

void FNativeClassDumpProcessor::PerformNativeClassDumpForClass(UClass* Class) {
	const FString Filename = FPackageName::GetShortName(Class->GetPackage()->GetFullName()) / Class->GetName() + TEXT(".json");
	const FString OutputFilename = Settings.RootDumpDirectory / TEXT("Classes") / Filename;

	if (!Settings.bOverwriteExistingFiles) {		
		//Skip dumping when we have a dump file already and are not allowed to overwrite files
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutputFilename)) {
			UE_LOG(LogNativeClassDumper, Display, TEXT("Skipping dumping native class %s, dump file is already present and overwriting is not allowed"), *Class->GetFullName());
			return;
		}
	}
	
	UE_LOG(LogNativeClassDumper, Display, TEXT("Serializing class %s"), *Class->GetFullName());

	const TSharedPtr<FJsonObject> ClassSerializedData = MakeShareable(new FJsonObject());
	UPropertySerializer* PropertySerializer = NewObject<UPropertySerializer>();
	UObjectHierarchySerializer* ObjectHierarchySerializer = NewObject<UObjectHierarchySerializer>();
	ObjectHierarchySerializer->SetPropertySerializer(PropertySerializer);
	ObjectHierarchySerializer->InitializeForSerialization(Class->GetPackage());

	FAssetHelper::SerializeClass(ClassSerializedData, Class, ObjectHierarchySerializer);

	const TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject());	
	RootObject->SetObjectField(TEXT("ClassSerializedData"), ClassSerializedData);
	RootObject->SetArrayField(TEXT("ObjectHierarchy"), ObjectHierarchySerializer->FinalizeSerialization());

	FString ResultString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
	FJsonSerializer::Serialize(RootObject, Writer);

	check(FFileHelper::SaveStringToFile(ResultString, *OutputFilename));
	
	this->ClassesProcessed.Increment();
}

void FNativeClassDumpProcessor::PerformNativeClassDumpForStruct(UStruct* Struct) {
	const FString Filename = FPackageName::GetShortName(Struct->GetPackage()->GetFullName()) / Struct->GetName() + TEXT(".json");
	const FString OutputFilename = Settings.RootDumpDirectory / TEXT("Structs") / Filename;

	if (!Settings.bOverwriteExistingFiles) {		
		//Skip dumping when we have a dump file already and are not allowed to overwrite files
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutputFilename)) {
			UE_LOG(LogNativeClassDumper, Display, TEXT("Skipping dumping native class %s, dump file is already present and overwriting is not allowed"), *Struct->GetFullName());
			return;
		}
	}
	
	UE_LOG(LogNativeClassDumper, Display, TEXT("Serializing struct %s"), *Struct->GetFullName());

	const TSharedPtr<FJsonObject> ClassSerializedData = MakeShareable(new FJsonObject());
	UPropertySerializer* PropertySerializer = NewObject<UPropertySerializer>();
	UObjectHierarchySerializer* ObjectHierarchySerializer = NewObject<UObjectHierarchySerializer>();
	ObjectHierarchySerializer->SetPropertySerializer(PropertySerializer);
	ObjectHierarchySerializer->InitializeForSerialization(Struct->GetPackage());

	FAssetHelper::SerializeStruct(ClassSerializedData, Struct, ObjectHierarchySerializer);

	const TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject());	
	RootObject->SetObjectField(TEXT("ClassSerializedData"), ClassSerializedData);
	RootObject->SetArrayField(TEXT("ObjectHierarchy"), ObjectHierarchySerializer->FinalizeSerialization());

	FString ResultString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
	FJsonSerializer::Serialize(RootObject, Writer);

	check(FFileHelper::SaveStringToFile(ResultString, *OutputFilename));
	
	this->ClassesProcessed.Increment();
}

bool FNativeClassDumpProcessor::IsTickable() const {
	return bHasFinishedDumping == false;
}

TStatId FNativeClassDumpProcessor::GetStatId() const {
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNativeClassDumpProcessor, STATGROUP_Game);
}

bool FNativeClassDumpProcessor::IsTickableWhenPaused() const {
	return true;
}

void FNativeClassDumpProcessor::InitializeNativeClassDump() {
	this->Settings = Settings;
	this->bHasFinishedDumping = false;
	this->RemainingClasses = Classes;
	this->RemainingStructs = Structs;
	this->ClassesTotal = Classes.Num();
	check(ClassesTotal);
	UE_LOG(LogNativeClassDumper, Display, TEXT("Starting native class dump of %d classes..."), ClassesTotal);
}

bool StaticNativeClassDumperExec(UWorld* World, const TCHAR* Command, FOutputDevice& Ar) {

	if (FParse::Command(&Command, TEXT("DumpAllNativeClasses")))
	{
		Ar.Log(TEXT("Starting console-driven native class dumping, dumping all native classes"));

		TArray<UPackage*> Packages;
		FString NextToken;
		while(FParse::Token(Command, NextToken, true))
		{
			if(UPackage* Package = FindPackage(nullptr, *NextToken))
				Packages.Add(Package);
		}

		TArray<UClass*> Classes;
		
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
			UClass* Class = *ClassIt;

			// Only interested in native C++ classes
			if (!Class->IsNative()) {
				continue;
			}

			if (Packages.Contains(Class->GetOuterUPackage()) || Packages.Num() == 0) {
				Classes.Add(Class);
			}
		}

		TArray<UStruct*> Structs;
		
		for (TObjectIterator<UStruct> StructIt; StructIt; ++StructIt) {
			UStruct* Struct = *StructIt;

			// Only interested in native C++ *structs*
			if (!Struct->IsNative() || Struct->IsA<UClass>() || Struct->IsA<UFunction>()) {
				continue;
			}

			if (Packages.Contains(Struct->GetOuter()) || Packages.Num() == 0) {
				Structs.Add(Struct);
			}
		}
		
		Ar.Logf(TEXT("Asset data gathered successfully! Gathered %d native classes and %d native structs for dumping"), Classes.Num(), Structs.Num());

		FNativeClassDumpSettings DumpSettings{};
		DumpSettings.bExitOnFinish = true;
		FNativeClassDumpProcessor::StartNativeClassDump(DumpSettings, Classes, Structs);
		Ar.Log(TEXT("Native class dump started successfully, game will shutdown on finish"));
		return true;
	}
	return false;
}

static FStaticSelfRegisteringExec NativeClassDumperStaticExec(&StaticNativeClassDumperExec);