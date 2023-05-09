// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UENeuralNetwork : ModuleRules
{
	public UENeuralNetwork(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput" });
	}
}
