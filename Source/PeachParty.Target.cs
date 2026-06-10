using UnrealBuildTool;
using System.Collections.Generic;

public class PeachPartyTarget : TargetRules
{
	public PeachPartyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PeachParty");
	}
}
