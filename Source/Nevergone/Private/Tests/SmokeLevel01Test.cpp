// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Tests/SmokeSaveSlot.h"

#include "Actors/Door.h"
#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
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

	// Only what the player sees muffles noise (and blocks a capture, which
	// shares the channel). Characters, alive or dead, never do. On any other
	// actor only a mesh may block the noise occlusion channel; the door's
	// box is the one declared exception, standing for the closed leaf.
	// Catches a Blueprint or an instance with custom collision on a trigger,
	// a volume or a widget.
	class FCheckNoiseOcclusionCommand : public IAutomationLatentCommand
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
				Test->AddError(TEXT("No game world to check noise occlusion in"));

				return true;
			}

			int32 CharactersChecked = 0;

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const AActor* Actor = *It;

				const bool bIsCharacter = Actor->IsA<ACharacter>();

				if (bIsCharacter)
				{
					++CharactersChecked;
				}

				const ADoor* Door = Cast<ADoor>(Actor);

				TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);

				for (UPrimitiveComponent* Primitive : Primitives)
				{
					if (!Primitive->IsQueryCollisionEnabled() ||
						Primitive->GetCollisionResponseToChannel(NevergoneCollision::NoiseOcclusion) != ECR_Block)
					{
						continue;
					}

					// Without collision geometry there is nothing for a trace
					// to hit: a screen-space widget, a debug draw component.
					if (!Primitive->GetBodySetup())
					{
						continue;
					}

					if (bIsCharacter)
					{
						Test->AddError(FString::Printf(TEXT("%s.%s (profile %s) blocks NoiseOcclusion: characters must not muffle noise"),
							*GetNameSafe(Actor), *GetNameSafe(Primitive), *Primitive->GetCollisionProfileName().ToString()));

						continue;
					}

					// A widget component is a mesh component to the engine,
					// but nothing the player sees as solid.
					const bool bIsMesh = Primitive->IsA<UMeshComponent>() && !Primitive->IsA<UWidgetComponent>();

					const bool bIsDoorBox = Door && Primitive == Door->GetCollisionBox();

					if (!bIsMesh && !bIsDoorBox)
					{
						Test->AddError(FString::Printf(TEXT("%s.%s (%s, profile %s) blocks NoiseOcclusion: only meshes may muffle noise"),
							*GetNameSafe(Actor), *GetNameSafe(Primitive), *GetNameSafe(Primitive->GetClass()),
							*Primitive->GetCollisionProfileName().ToString()));
					}
				}
			}

			// A level without characters would skip the character rule.
			Test->TestTrue(TEXT("Level has characters to check"), CharactersChecked > 0);

			return true;
		}
	};
}

// Opens the excursion test level and lets it run. Any Error or ensure logged
// meanwhile fails the test (the automation framework's default). After the
// wait it also requires the default agent's navmesh and that nothing but
// scenery meshes muffles noise.
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
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeLevel01::FCheckNoiseOcclusionCommand());
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeSave::FCleanTestSlotCommand());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
