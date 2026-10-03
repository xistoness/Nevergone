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
			// PhysicsCore: UPhysicalMaterial and EPhysicalSurface, used by the noise system
			"PhysicsCore",
			"GameplayAbilities",
			"GameplayTags",
			"DeveloperSettings",
			"GameplayTasks",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
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
		

		PublicIncludePaths.AddRange(new string[] {
			"Nevergone",
			"Nevergone/Public"
		});
	}
}