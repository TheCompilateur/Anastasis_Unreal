// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Anastasis_UnrealV2Target : TargetRules
{
	public Anastasis_UnrealV2Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("AnastasisSim");
		ExtraModuleNames.Add("Anastasis_UnrealV2");
	}
}
