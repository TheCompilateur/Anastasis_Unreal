// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Anastasis_UnrealV2EditorTarget : TargetRules
{
	public Anastasis_UnrealV2EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("AnastasisSim");
		ExtraModuleNames.Add("Anastasis_UnrealV2");
	}
}
