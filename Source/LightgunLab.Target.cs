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
	}
}
