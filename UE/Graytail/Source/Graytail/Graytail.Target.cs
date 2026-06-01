using UnrealBuildTool;
using System.Collections.Generic;

public class GraytailTarget : TargetRules
{
	public GraytailTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		CppStandard = CppStandardVersion.Cpp20;
		ExtraModuleNames.Add("Graytail");
	}
}
