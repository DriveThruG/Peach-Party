using UnrealBuildTool;
using System.Collections.Generic;

public class PeachPartyEditorTarget : TargetRules
{
	public PeachPartyEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PeachParty");
	}
}
