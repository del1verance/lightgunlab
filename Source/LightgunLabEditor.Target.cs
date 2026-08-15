// Copyright (c) 2026 del1verance. MIT License.

using UnrealBuildTool;

public class LightgunLabEditorTarget : TargetRules
{
	public LightgunLabEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LightgunLabGame");
	}
}
