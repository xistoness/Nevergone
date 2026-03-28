// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Nevergone : ModuleRules
{
	public Nevergone(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "AITestSuite" });

		PublicIncludePaths.AddRange(new string[] {
			"Nevergone",
			"Nevergone/Public",
			"Nevergone/Variant_Platforming",
			"Nevergone/Variant_Platforming/Animation",
			"Nevergone/Variant_Combat",
			"Nevergone/Variant_Combat/AI",
			"Nevergone/Variant_Combat/Animation",
			"Nevergone/Variant_Combat/Gameplay",
			"Nevergone/Variant_Combat/Interfaces",
			"Nevergone/Variant_Combat/UI",
			"Nevergone/Variant_SideScrolling",
			"Nevergone/Variant_SideScrolling/AI",
			"Nevergone/Variant_SideScrolling/Gameplay",
			"Nevergone/Variant_SideScrolling/Interfaces",
			"Nevergone/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
