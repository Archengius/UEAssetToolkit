using UnrealBuildTool;
using System.IO;
using System;

public class AssetGenerator : ModuleRules
{
    public AssetGenerator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;
        ShadowVariableWarningLevel = WarningLevel.Off;
        
        //Asset Generator dependencies
        PublicDependencyModuleNames.AddRange(new[] {
            "Json",
			"UnrealEd",
			"BlueprintGraph",
			"FactoryGame",
			"AssetDumper"
        });
		PrivateDependencyModuleNames.AddRange(new[] {
            "DesktopPlatform",
            "MaterialEditor",
            "RHI", 
            "AnimGraph"
        });
        
        //FactoryGame transitive dependencies
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject",
            "Engine",
            "InputCore",
            "OnlineSubsystem", "OnlineSubsystemNull", "OnlineSubsystemEOS", "OnlineSubsystemUtils",
            "SignificanceManager",
            "APEX", "ApexDestruction",
            "BlueprintGraph",
            "KismetCompiler",
            "AnimGraphRuntime",
            "AkAudio", 
            "PhysXVehicles",
            "AssetRegistry",
            "NavigationSystem",
            "ReplicationGraph",
            "AIModule",
            "GameplayTasks",
            "SlateCore", "Slate", "UMG",
            "InstancedSplines",
            "Projects",
            "WorkspaceMenuStructure",
            "Landscape"
        });
    }
}