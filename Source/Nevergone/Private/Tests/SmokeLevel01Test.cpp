// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Tests/SmokeSaveSlot.h"

#include "Components/PrimitiveComponent.h"
#include "EngineTools/CollisionChannels.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NevergoneSmokeLevel01
{
	// The navmesh of the project's first SupportedAgents entry must be
	// registered, built for that agent and hold tiles: without it no enemy
	// moves. The agent is read from the config, never repeated here.
	class FCheckNavMeshCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			FAutomationTestBase* Test = FAutomationTestFramework::Get().GetCurrentTest();

			if (!Test)
			{
				return true;
			}

			UWorld* World = AutomationCommon::GetAnyGameWorld();

			if (!World)
			{
				Test->AddError(TEXT("No game world to check the navmesh in"));

				return true;
			}

			const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

			if (!NavSys)
			{
				Test->AddError(TEXT("No navigation system in the level"));

				return true;
			}

			const TArray<FNavDataConfig>& Agents = NavSys->GetSupportedAgents();

			if (Agents.IsEmpty())
			{
				Test->AddError(TEXT("No SupportedAgents in the navigation system config"));

				return true;
			}

			const FNavDataConfig& Agent = Agents[0];

			const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSys->GetNavDataForAgentName(Agent.Name));

			if (!NavMesh || !NavMesh->IsRegistered())
			{
				Test->AddError(FString::Printf(TEXT("No RecastNavMesh registered for agent %s"), *Agent.Name.ToString()));

				return true;
			}

			Test->TestTrue(FString::Printf(TEXT("%s is built for agent %s"), *GetNameSafe(NavMesh), *Agent.Name.ToString()),
				NavMesh->GetConfig().IsEquivalent(Agent));

			Test->TestTrue(FString::Printf(TEXT("%s is the default navmesh"), *GetNameSafe(NavMesh)),
				NavSys->GetDefaultNavDataInstance() == NavMesh);

			Test->TestTrue(FString::Printf(TEXT("%s has tiles loaded"), *GetNameSafe(NavMesh)),
				NavMesh->GetNavMeshTilesCount() > 0);

			return true;
		}
	};

	// Characters, alive or dead, never muffle noise: no component of theirs
	// with query collision may block the noise occlusion channel. Catches a
	// Blueprint whose capsule or mesh uses custom collision.
	class FCheckCharactersIgnoreNoiseOcclusionCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			FAutomationTestBase* Test = FAutomationTestFramework::Get().GetCurrentTest();

			UWorld* World = AutomationCommon::GetAnyGameWorld();

			if (!Test)
			{
				return true;
			}

			if (!World)
			{
				Test->AddError(TEXT("No game world to check the characters in"));

				return true;
			}

			int32 Checked = 0;

			for (TActorIterator<ACharacter> It(World); It; ++It)
			{
				++Checked;

				TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);

				for (const UPrimitiveComponent* Primitive : Primitives)
				{
					if (Primitive->IsQueryCollisionEnabled() &&
						Primitive->GetCollisionResponseToChannel(NevergoneCollision::NoiseOcclusion) == ECR_Block)
					{
						Test->AddError(FString::Printf(TEXT("%s.%s (profile %s) blocks NoiseOcclusion: characters must not muffle noise"),
							*GetNameSafe(*It), *GetNameSafe(Primitive), *Primitive->GetCollisionProfileName().ToString()));
					}
				}
			}

			// An empty level would pass without checking anything.
			Test->TestTrue(TEXT("Level has characters to check"), Checked > 0);

			return true;
		}
	};
}

// Opens the excursion test level and lets it run. Any Error or ensure logged
// meanwhile fails the test (the automation framework's default). After the
// wait it also requires the default agent's navmesh and characters that do
// not muffle noise.
// Meant to run with -game: in the editor the map would load without playing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneSmokeLevel01Test, "Nevergone.Smoke.Level01",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneSmokeLevel01Test::RunTest(const FString& Parameters)
{
	static const TCHAR* MapName = TEXT("/Game/Blueprints/Level/LevelPrototype/Level_01/Level_01");

	if (!AutomationOpenMap(MapName))
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MapName));
		return false;
	}

	// Like the boot smoke, this one never touches a player slot.
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeSave::FUseTestSlotCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeLevel01::FCheckNavMeshCommand());
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeLevel01::FCheckCharactersIgnoreNoiseOcclusionCommand());
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeSave::FCleanTestSlotCommand());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
