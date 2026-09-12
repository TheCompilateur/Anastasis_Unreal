// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Anastasis_UnrealV2 : ModuleRules
{
	public Anastasis_UnrealV2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"AnastasisSim"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });

		PublicIncludePaths.AddRange(new string[] {
			"Anastasis_UnrealV2",
			"Anastasis_UnrealV2/Variant_Horror",
			"Anastasis_UnrealV2/Variant_Horror/UI",
			"Anastasis_UnrealV2/Variant_Shooter",
			"Anastasis_UnrealV2/Variant_Shooter/AI",
			"Anastasis_UnrealV2/Variant_Shooter/UI",
			"Anastasis_UnrealV2/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

