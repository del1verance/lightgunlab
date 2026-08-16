// Copyright (c) 2026 del1verance. MIT License.

using UnrealBuildTool;

public class LightgunLabTarget : TargetRules
{
	public LightgunLabTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LightgunLabGame");

		// The public demo ships as Shipping for size, but its whole purpose is
		// community hardware test reports - keep UE_LOG alive so Saved/Logs
		// carries the LogLightgunLab detection/routing story.
		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			BuildEnvironment = TargetBuildEnvironment.Unique;
			bUseLoggingInShipping = true;
		}
	}
}
