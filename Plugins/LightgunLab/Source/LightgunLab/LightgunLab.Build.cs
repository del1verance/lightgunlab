// Copyright (c) 2026 del1verance. MIT License.

using UnrealBuildTool;

public class LightgunLab : ModuleRules
{
	public LightgunLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Sockets",
			"Networking",
			"ApplicationCore",
			"Projects"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[]
			{
				"SetupAPI.lib",
				"Advapi32.lib",
				"User32.lib"
			});
		}
	}
}
