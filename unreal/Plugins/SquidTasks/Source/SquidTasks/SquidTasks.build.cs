using UnrealBuildTool;

public class SquidTasks : ModuleRules
{
	public SquidTasks(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
		});

		SharedPCHHeaderFile = "Public/SquidTasksSharedPCH.h";
	}
}
