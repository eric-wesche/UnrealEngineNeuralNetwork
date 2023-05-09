// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UENeuralNetwork : ModuleRules
{
	public UENeuralNetwork(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Include path for rendering dependencies
        PrivateIncludePaths.AddRange(new string[] { Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private") });

        PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput",
			// Rendering dependencies
            "Renderer",
            "RenderCore",
            "RHI",
            "RHICore",
            "D3D12RHI",
            // OpenCV dependencies
            "OpenCV",
            "OpenCVHelper",
        });
	}
}
