using UnrealBuildTool;
using System.Collections.Generic;

public class GraytailTarget : TargetRules
{
	public GraytailTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("Graytail");
	}
}
