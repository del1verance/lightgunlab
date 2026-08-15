// Copyright (c) 2026 del1verance. MIT License.

using UnrealBuildTool;

public class LightgunLabGame : ModuleRules
{
	public LightgunLabGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});
	}
}
