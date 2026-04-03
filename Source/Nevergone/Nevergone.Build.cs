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
			"Slate",
			// --- Audio ---
			// AudioMixer: required for UAudioComponent, USoundBase, USoundClass, USoundMix
			// MetasoundEngine: required for MetaSound assets (MS_BattleMusic, etc.)
			"AudioMixer",
			"MetasoundEngine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "AITestSuite" });

		PublicIncludePaths.AddRange(new string[] {
			"Nevergone",
			"Nevergone/Public"
		});
	}
}