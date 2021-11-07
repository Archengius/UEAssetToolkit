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
        
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", 
            "CoreUObject",
            "Engine",
            "InputCore",
            "BlueprintGraph",
            "KismetCompiler",
            "AssetRegistry",
            "SlateCore", "Slate", "UMG",
            "Projects",
            "Landscape",
            "Json",
			"UnrealEd",
            "UMGEditor",
			"BlueprintGraph",
            "MovieSceneTracks",
            "AssetDumper"
        });
        
		PrivateDependencyModuleNames.AddRange(new[] {
            "DesktopPlatform",
            "MaterialEditor",
            "RHI", 
            "AnimGraph",
            "Kismet",
            "WorkspaceMenuStructure",
            "PhysicsCore",
            "DeveloperSettings", 
            "MediaAssets"
        });
    }
}