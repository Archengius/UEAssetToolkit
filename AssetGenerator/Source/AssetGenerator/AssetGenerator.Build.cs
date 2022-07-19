using UnrealBuildTool;
using System.IO;
using System;

public class AssetGenerator : ModuleRules
{
    public AssetGenerator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
#if UE_4_24_OR_LATER
        bLegacyPublicIncludePaths = false;
        ShadowVariableWarningLevel = WarningLevel.Off;
#endif     
        
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
            "MediaAssets",
            "AudioEditor",
            "GraphEditor"
        });

#if UE_4_26_OR_LATER
	    PrivateDependencyModuleNames.Add("DeveloperSettings");
#endif
        
    }
}