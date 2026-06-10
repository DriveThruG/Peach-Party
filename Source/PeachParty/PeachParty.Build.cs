using UnrealBuildTool;

public class PeachParty : ModuleRules
{
	public PeachParty(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat module layout (Core/ Minigame/ Interaction/ directly under the module root).
		// V6 build settings disable legacy include paths, so add the module root explicitly
		// to make folder-relative includes like "Core/PPTypes.h" resolve.
		PublicIncludePaths.Add(ModuleDirectory);

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
