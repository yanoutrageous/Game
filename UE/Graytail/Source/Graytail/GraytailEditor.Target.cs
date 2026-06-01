using UnrealBuildTool;
using System.Collections.Generic;

public class GraytailEditorTarget : TargetRules
{
	public GraytailEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("Graytail");
	}
}
