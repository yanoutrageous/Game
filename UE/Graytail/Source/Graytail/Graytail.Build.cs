using UnrealBuildTool;

public class Graytail : ModuleRules
{
	public Graytail(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Json",
			"JsonUtilities",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.Add(ModuleDirectory);
	}
}

