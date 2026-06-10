using UnrealBuildTool;

public class PeachParty : ModuleRules
{
	public PeachParty(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
			// "Paper2D"  // add when the 2D minigame sprite layer is implemented
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
