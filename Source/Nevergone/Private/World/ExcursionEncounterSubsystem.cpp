// Copyright Xyzto Works

#include "World/ExcursionEncounterSubsystem.h"

#include "Nevergone.h"
#include "ActorComponents/UnitStatsComponent.h"
#include "Characters/CharacterBase.h"
#include "Characters/ExcursionSurvivorNPC.h"
#include "Characters/ExplorationEnemyCharacter.h"
#include "Characters/PlayerControllers/ExplorationEnemyController.h"
#include "Characters/PlayerControllers/ExplorationPlayerController.h"
#include "Data/ExplorationEnemyDefinition.h"
#include "Data/ItemDefinition.h"
#include "World/ExcursionEncounterSettings.h"
#include "World/ExcursionNoiseSubsystem.h"
#include "World/InventorySubsystem.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

UExcursionEncounterSubsystem* UExcursionEncounterSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	return World ? World->GetSubsystem<UExcursionEncounterSubsystem>() : nullptr;
}

bool UExcursionEncounterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UExcursionEncounterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExcursionEncounterSubsystem, STATGROUP_Tickables);
}

// ---------------------------------------------------------------------------
// Catch
// ---------------------------------------------------------------------------

bool UExcursionEncounterSubsystem::ReportCatch(AExplorationEnemyCharacter* Enemy, ACharacter* Victim)
{
	if (!IsValid(Enemy) || !IsValid(Victim))
	{
		return false;
	}

	// The player's death ends the excursion; nothing is resolved after it.
	if (bPlayerDied)
	{
		return false;
	}

	if (IsInCapture(Enemy) || IsInCapture(Victim))
	{
		return false;
	}

	const UExplorationEnemyDefinition* Definition = Enemy->GetDefinition();

	if (!Definition)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionEncounterSubsystem] %s has no definition; capture of %s refused"),
			*GetNameSafe(Enemy), *GetNameSafe(Victim));

		return false;
	}

	if (AExcursionSurvivorNPC* Follower = Cast<AExcursionSurvivorNPC>(Victim))
	{
		if (!Follower->IsRecruited())
		{
			return false;
		}

		StartFollowerCapture(Enemy, Follower, *Definition);

		return true;
	}

	if (Victim->IsPlayerControlled())
	{
		StartPlayerCapture(Enemy, Victim, *Definition);

		return true;
	}

	UE_LOG(LogNevergone, Warning,
		TEXT("[ExcursionEncounterSubsystem] %s caught %s, which is neither the player nor a follower; refused"),
		*GetNameSafe(Enemy), *GetNameSafe(Victim));

	return false;
}

void UExcursionEncounterSubsystem::StartFollowerCapture(AExplorationEnemyCharacter* Enemy,
	AExcursionSurvivorNPC* Follower, const UExplorationEnemyDefinition& Definition)
{
	// Dead from the first frame: other enemies drop him as a target and the
	// excursion stops counting him while the animation still plays.
	Follower->MarkDead();

	if (UCharacterMovementComponent* Movement = Follower->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	AlignForCapture(Enemy, Follower, Definition.CaptureStandOffDistance);

	PlayPairedAnimation(Enemy, Follower, Definition.DevourFollowerAnimation);

	KeepDeathMontageEnd(Follower, GetDeathMontage(Definition, false, EExcursionCaptureOutcome::Killed));

	FExcursionCaptureSequence Capture;
	Capture.Enemy = Enemy;
	Capture.Victim = Follower;
	Capture.Outcome = EExcursionCaptureOutcome::Killed;
	Capture.TimeLeft = Definition.DevourFollowerSeconds;
	Capture.bVictimIsPlayer = false;

	Captures.Add(Capture);

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionEncounterSubsystem] %s caught follower %s; devouring for %.1fs"),
		*GetNameSafe(Enemy), *GetNameSafe(Follower), Definition.DevourFollowerSeconds);
}

void UExcursionEncounterSubsystem::StartPlayerCapture(AExplorationEnemyCharacter* Enemy, ACharacter* Player,
	const UExplorationEnemyDefinition& Definition)
{
	FreezeWorldExcept(Enemy, Player);

	if (AExplorationPlayerController* PlayerController = GetPlayerController(Player))
	{
		PlayerController->EnterCaptureInputMode();
	}

	if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	AlignForCapture(Enemy, Player, Definition.CaptureStandOffDistance);

	if (Definition.CaptureNoise)
	{
		if (UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
		{
			Noise->EmitNoise(Definition.CaptureNoise, Player->GetActorLocation(), Player);
		}
	}

	FExcursionCaptureSequence Capture;
	Capture.Enemy = Enemy;
	Capture.Victim = Player;
	Capture.bVictimIsPlayer = true;

	const FExcursionCaptureAnimation* Animation = nullptr;

	if (TryConsumeSafeguard())
	{
		Capture.Outcome = EExcursionCaptureOutcome::Unharmed;

		Animation = &Definition.SafeguardAnimation;
	}
	else
	{
		const float DeathChance = ComputeDeathChance(Definition, Player);

		const float Roll = FMath::FRandRange(0.0f, 100.0f);

		Capture.Outcome = Roll < DeathChance ? EExcursionCaptureOutcome::Killed : EExcursionCaptureOutcome::Infected;

		Animation = Capture.Outcome == EExcursionCaptureOutcome::Killed
			? &Definition.KillAnimation
			: &Definition.BiteAnimation;

		UE_LOG(LogNevergone, Log,
			TEXT("[ExcursionEncounterSubsystem] %s rolled %.1f against a death chance of %.1f"),
			*GetNameSafe(Player), Roll, DeathChance);
	}

	Capture.TimeLeft = PlayPairedAnimation(Enemy, Player, *Animation);

	// Under a safeguard the enemy dies; otherwise only the player can.
	ACharacter* DyingBody = Capture.Outcome == EExcursionCaptureOutcome::Unharmed ? Enemy : Player;

	KeepDeathMontageEnd(DyingBody, GetDeathMontage(Definition, true, Capture.Outcome));

	Captures.Add(Capture);

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionEncounterSubsystem] %s caught %s; outcome %s, sequence %.1fs"),
		*GetNameSafe(Enemy), *GetNameSafe(Player),
		*UEnum::GetValueAsString(Capture.Outcome), Capture.TimeLeft);
}

// ---------------------------------------------------------------------------
// Sequence
// ---------------------------------------------------------------------------

void UExcursionEncounterSubsystem::Tick(float DeltaTime)
{
	WatchRagdolls();

	if (Captures.IsEmpty())
	{
		return;
	}

	TArray<FExcursionCaptureSequence> Finished;

	for (int32 Index = Captures.Num() - 1; Index >= 0; --Index)
	{
		FExcursionCaptureSequence& Capture = Captures[Index];

		const AExplorationEnemyCharacter* Enemy = Capture.Enemy.Get();

		const float Dilation = Enemy ? Enemy->CustomTimeDilation : 1.0f;

		Capture.TimeLeft -= DeltaTime * Dilation;

		if (Capture.TimeLeft <= 0.0f || !Enemy || !Capture.Victim.IsValid())
		{
			Finished.Add(Capture);

			Captures.RemoveAt(Index);
		}
	}

	// Finished outside the loop: finishing unfreezes the world and calls back
	// into controllers, and none of that may touch the array being walked.
	for (const FExcursionCaptureSequence& Capture : Finished)
	{
		FinishCapture(Capture);
	}
}

void UExcursionEncounterSubsystem::FinishCapture(const FExcursionCaptureSequence& Capture)
{
	AExplorationEnemyCharacter* Enemy = Capture.Enemy.Get();

	ACharacter* Victim = Capture.Victim.Get();

	AExplorationEnemyController* EnemyController =
		Enemy ? Cast<AExplorationEnemyController>(Enemy->GetController()) : nullptr;

	const UExplorationEnemyDefinition* Definition = Enemy ? Enemy->GetDefinition() : nullptr;

	UAnimMontage* DeathMontage = Definition
		? GetDeathMontage(*Definition, Capture.bVictimIsPlayer, Capture.Outcome)
		: nullptr;

	if (!Capture.bVictimIsPlayer)
	{
		if (Enemy)
		{
			// The devour montage loops; the time decides when it ends.
			Enemy->StopAnimMontage();
		}

		BecomeCorpse(Victim, DeathMontage);

		if (EnemyController)
		{
			EnemyController->FinishFeeding();
		}
	}
	else
	{
		AExplorationPlayerController* PlayerController = GetPlayerController(Victim);

		switch (Capture.Outcome)
		{
		case EExcursionCaptureOutcome::Unharmed:
			if (EnemyController)
			{
				EnemyController->Kill();
			}

			BecomeCorpse(Enemy, DeathMontage);

			UnfreezeWorld();

			if (PlayerController)
			{
				PlayerController->ExitCaptureInputMode();
			}
			break;

		case EExcursionCaptureOutcome::Infected:
			if (Victim)
			{
				++BitesBySurvivor.FindOrAdd(Victim);
			}

			if (EnemyController && Definition)
			{
				EnemyController->BeginKnockdown(Definition->KnockdownSeconds, Victim);
			}

			UnfreezeWorld();

			if (PlayerController)
			{
				PlayerController->ExitCaptureInputMode();
			}
			break;

		case EExcursionCaptureOutcome::Killed:
			// The excursion is over. The world stays frozen and the input
			// stays blocked until the excursion result takes over.
			bPlayerDied = true;

			BecomeCorpse(Victim, DeathMontage);

			KillRecruitedSurvivors();
			break;

		default:
			break;
		}
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionEncounterSubsystem] capture of %s by %s resolved: %s"),
		*GetNameSafe(Victim), *GetNameSafe(Enemy), *UEnum::GetValueAsString(Capture.Outcome));

	OnCaptureResolved.Broadcast(Victim, Enemy, Capture.Outcome);
}

bool UExcursionEncounterSubsystem::IsInCapture(const AActor* Actor) const
{
	for (const FExcursionCaptureSequence& Capture : Captures)
	{
		if (Capture.Enemy.Get() == Actor || Capture.Victim.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Resolution
// ---------------------------------------------------------------------------

bool UExcursionEncounterSubsystem::TryConsumeSafeguard() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	UInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UInventorySubsystem>() : nullptr;

	if (!Inventory)
	{
		return false;
	}

	UItemDefinition* Safeguard = nullptr;

	for (const FInventoryEntry& Entry : Inventory->GetInventory())
	{
		UItemDefinition* Item = Entry.Definition.LoadSynchronous();

		if (Item && Item->ItemTags.HasTag(TAG_Item_Safeguard))
		{
			Safeguard = Item;

			break;
		}
	}

	if (!Safeguard)
	{
		return false;
	}

	// Removed outside the loop above: removing reshapes the array it walks.
	if (Inventory->RemoveItem(Safeguard, 1) != EInventoryResult::Success)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionEncounterSubsystem] found safeguard %s but could not remove it; rolling instead"),
			*GetNameSafe(Safeguard));

		return false;
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionEncounterSubsystem] safeguard %s used and broken"),
		*GetNameSafe(Safeguard));

	return true;
}

float UExcursionEncounterSubsystem::ComputeDeathChance(const UExplorationEnemyDefinition& Definition,
	ACharacter* Survivor) const
{
	const UExcursionEncounterSettings* Settings = GetDefault<UExcursionEncounterSettings>();

	const int32 Strength = GetStrength(Survivor);

	const int32 PriorBites = BitesBySurvivor.FindRef(Survivor);

	const float Chance = CalculateDeathChance(Definition.BaseDeathChance, Strength, PriorBites,
		Settings->DeathChanceReductionPerStrength, Settings->DeathChanceBonusPerPriorBite);

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExcursionEncounterSubsystem] %s death chance: base %.1f, Strength %d, earlier bites %d -> %.1f"),
		*GetNameSafe(Survivor), Definition.BaseDeathChance, Strength, PriorBites, Chance);

	return Chance;
}

float UExcursionEncounterSubsystem::CalculateDeathChance(float BaseDeathChance, int32 Strength, int32 PriorBites,
	float ReductionPerStrength, float BonusPerPriorBite)
{
	const float Chance = BaseDeathChance
		- Strength * ReductionPerStrength
		+ PriorBites * BonusPerPriorBite;

	return FMath::Clamp(Chance, 0.0f, 100.0f);
}

UAnimMontage* UExcursionEncounterSubsystem::GetDeathMontage(const UExplorationEnemyDefinition& Definition,
	bool bVictimIsPlayer, EExcursionCaptureOutcome Outcome)
{
	if (!bVictimIsPlayer)
	{
		return Definition.DevourFollowerAnimation.VictimMontage;
	}

	switch (Outcome)
	{
	case EExcursionCaptureOutcome::Unharmed:
		// The safeguard kills the enemy.
		return Definition.SafeguardAnimation.EnemyMontage;

	case EExcursionCaptureOutcome::Killed:
		return Definition.KillAnimation.VictimMontage;

	default:
		return nullptr;
	}
}

int32 UExcursionEncounterSubsystem::GetStrength(const ACharacter* Survivor)
{
	// Outside combat the live pawn's stats are the authority, bonuses
	// included; FPartyMemberData carries no attributes.
	const ACharacterBase* Character = Cast<ACharacterBase>(Survivor);

	const UUnitStatsComponent* Stats = Character ? Character->GetUnitStats() : nullptr;

	if (!Stats)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionEncounterSubsystem] %s has no UUnitStatsComponent; rolling with Strength 0"),
			*GetNameSafe(Survivor));

		return 0;
	}

	return Stats->Attributes.Strength + Stats->TemporaryAttributeBonuses.Strength;
}

// ---------------------------------------------------------------------------
// Presentation
// ---------------------------------------------------------------------------

void UExcursionEncounterSubsystem::AlignForCapture(AExplorationEnemyCharacter* Enemy, ACharacter* Victim,
	float StandOffDistance)
{
	if (!Enemy || !Victim)
	{
		return;
	}

	FVector ToEnemy = Enemy->GetActorLocation() - Victim->GetActorLocation();
	ToEnemy.Z = 0.0f;

	if (!ToEnemy.Normalize())
	{
		ToEnemy = Victim->GetActorForwardVector();
	}

	FVector EnemyLocation = Victim->GetActorLocation() + ToEnemy * StandOffDistance;
	EnemyLocation.Z = Enemy->GetActorLocation().Z;

	// Teleport rather than sweep: the pair is authored at this distance, and
	// a sweep would stop the enemy on the victim's capsule.
	Enemy->SetActorLocationAndRotation(EnemyLocation, (-ToEnemy).Rotation(), false, nullptr,
		ETeleportType::TeleportPhysics);

	Victim->SetActorRotation(ToEnemy.Rotation());
}

float UExcursionEncounterSubsystem::PlayPairedAnimation(AExplorationEnemyCharacter* Enemy, ACharacter* Victim,
	const FExcursionCaptureAnimation& Animation)
{
	float Length = 0.0f;

	if (Enemy && Animation.EnemyMontage)
	{
		Length = FMath::Max(Length, Enemy->PlayAnimMontage(Animation.EnemyMontage));
	}

	if (Victim && Animation.VictimMontage)
	{
		Length = FMath::Max(Length, Victim->PlayAnimMontage(Animation.VictimMontage));
	}

	if (Length <= 0.0f)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExcursionEncounterSubsystem] %s has no capture animation for this outcome; resolving at once"),
			*GetNameSafe(Enemy));
	}

	return Length;
}

void UExcursionEncounterSubsystem::KeepDeathMontageEnd(ACharacter* Body, UAnimMontage* DeathMontage)
{
	USkeletalMeshComponent* Mesh = Body ? Body->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;

	if (!DeathMontage || !AnimInstance)
	{
		return;
	}

	// The instance copies the asset's flag on play and may override it, as
	// the engine's own sequencer montages do. BecomeCorpse stops it.
	if (FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(DeathMontage))
	{
		Instance->bEnableAutoBlendOut = false;
	}
}

void UExcursionEncounterSubsystem::BecomeCorpse(ACharacter* Character, UAnimMontage* DeathMontage)
{
	if (!Character)
	{
		return;
	}

	// Physics takes over from the pose the body has now; a montage still
	// playing (or blending out) would keep pulling the bones.
	Character->StopAnimMontage();

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}

	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();

	if (!Mesh)
	{
		return;
	}

	if (!Mesh->GetPhysicsAsset())
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExcursionEncounterSubsystem] %s has no physics asset; the body keeps its animated pose instead of falling"),
			*GetNameSafe(Character));

		return;
	}

	Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();
	Mesh->bBlendPhysics = true;

	FExcursionRagdoll Ragdoll;
	Ragdoll.Mesh = Mesh;
	Ragdoll.DeathMontage = DeathMontage;

	ActiveRagdolls.Add(Ragdoll);
}

void UExcursionEncounterSubsystem::WatchRagdolls()
{
	for (int32 Index = ActiveRagdolls.Num() - 1; Index >= 0; --Index)
	{
		USkeletalMeshComponent* Mesh = ActiveRagdolls[Index].Mesh.Get();

		if (!Mesh)
		{
			ActiveRagdolls.RemoveAt(Index);

			continue;
		}

		if (Mesh->Bounds.ContainsNaN())
		{
			UE_LOG(LogNevergone, Error,
				TEXT("[ExcursionEncounterSubsystem] ragdoll of %s blew up (NaN); removed from physics. Check physics asset %s: tiny bodies (fingers, toes) or overlapping bodies with collision between them are the usual cause"),
				*GetNameSafe(Mesh->GetOwner()), *GetNameSafe(Mesh->GetPhysicsAsset()));

			Mesh->SetSimulatePhysics(false);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			UAnimMontage* DeathMontage = ActiveRagdolls[Index].DeathMontage.Get();

			if (HoldDeathPose(Mesh, DeathMontage))
			{
				UE_LOG(LogNevergone, Log,
					TEXT("[ExcursionEncounterSubsystem] %s stays on the ground in the last frame of %s"),
					*GetNameSafe(Mesh->GetOwner()), *GetNameSafe(DeathMontage));
			}
			else
			{
				// Animation alone would stand the body back up.
				Mesh->SetVisibility(false);

				UE_LOG(LogNevergone, Log,
					TEXT("[ExcursionEncounterSubsystem] %s has no death pose to hold (no montage, or the pose stayed NaN); body hidden"),
					*GetNameSafe(Mesh->GetOwner()));
			}

			ActiveRagdolls.RemoveAt(Index);

			continue;
		}

		// Settled bodies do not go NaN anymore.
		if (!Mesh->IsAnyRigidBodyAwake())
		{
			ActiveRagdolls.RemoveAt(Index);
		}
	}
}

bool UExcursionEncounterSubsystem::HoldDeathPose(USkeletalMeshComponent* Mesh, UAnimMontage* DeathMontage)
{
	ACharacter* Character = Mesh ? Cast<ACharacter>(Mesh->GetOwner()) : nullptr;

	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;

	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;

	if (!DeathMontage || !Capsule || !AnimInstance)
	{
		return false;
	}

	// Started at its end and paused, the montage never blends out. A
	// montage that does not play (wrong skeleton, no slot) would leave the
	// body standing in the anim graph's pose.
	const float Length = AnimInstance->Montage_PlayWithBlendIn(DeathMontage, FAlphaBlendArgs(0.0f), 1.0f,
		EMontagePlayReturnType::MontageLength, DeathMontage->GetPlayLength());

	if (Length <= 0.0f)
	{
		return false;
	}

	AnimInstance->Montage_Pause(DeathMontage);

	// The ragdoll dragged the mesh away from the capsule, which stayed where
	// the body died: the mesh goes back to its usual offset there. The
	// attach only matters if something detached it. The NaN ragdoll left
	// NaN in the mesh's location: every setter that moves by a delta from
	// the current transform keeps it NaN, so the offset is written
	// directly and the transform rebuilt from it. Scale is never touched
	// by physics. The bodies follow once the pose below is valid; pushed
	// now, they would get the still-NaN bones.
	Mesh->AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Mesh->SetRelativeLocation_Direct(Character->GetBaseTranslationOffset());
	Mesh->SetRelativeRotation_Direct(Character->GetBaseRotationOffset().Rotator());
	Mesh->UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate, ETeleportType::TeleportPhysics);

	// The bones are still NaN, and so are the bounds: the mesh is never
	// rendered, and a character's mesh only refreshes its bones when
	// rendered. The pose is evaluated here, before the frame ends (NaN
	// bounds reaching the renderer trip an ensure), and the bones keep
	// refreshing from now on.
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->TickAnimation(0.0f, false);
	Mesh->RefreshBoneTransforms();

	// An evaluation skipped (update rate optimizations) leaves the bones
	// NaN: better hidden than a body that never shows.
	return !Mesh->Bounds.ContainsNaN();
}

void UExcursionEncounterSubsystem::KillRecruitedSurvivors()
{
	for (TActorIterator<AExcursionSurvivorNPC> It(GetWorld()); It; ++It)
	{
		AExcursionSurvivorNPC* Survivor = *It;

		if (!Survivor->IsRecruited())
		{
			continue;
		}

		Survivor->MarkDead();

		// No animation of their own: they die where they stand.
		BecomeCorpse(Survivor, nullptr);

		UE_LOG(LogNevergone, Log,
			TEXT("[ExcursionEncounterSubsystem] %s dies with the player"),
			*GetNameSafe(Survivor));
	}
}

// ---------------------------------------------------------------------------
// Freeze
// ---------------------------------------------------------------------------

void UExcursionEncounterSubsystem::FreezeWorldExcept(const AActor* KeepA, const AActor* KeepB)
{
	auto Freeze = [this](AActor* Actor)
	{
		if (Actor && Actor->CustomTimeDilation > 0.0f)
		{
			Actor->CustomTimeDilation = 0.0f;

			FrozenActors.Add(Actor);
		}
	};

	// Every character and its controller: enemies, followers, idle NPCs.
	// The controller matters as much as the body, or its brain would keep
	// issuing orders.
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Character = *It;

		if (Character == KeepA || Character == KeepB)
		{
			continue;
		}

		Freeze(Character);
		Freeze(Character->GetController());
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionEncounterSubsystem] world frozen around %s and %s (%d actors)"),
		*GetNameSafe(KeepA), *GetNameSafe(KeepB), FrozenActors.Num());
}

void UExcursionEncounterSubsystem::UnfreezeWorld()
{
	// Everything frozen here ran at normal speed before: nothing else in the
	// excursion changes time dilation.
	for (const TWeakObjectPtr<AActor>& Actor : FrozenActors)
	{
		if (Actor.IsValid())
		{
			Actor->CustomTimeDilation = 1.0f;
		}
	}

	UE_LOG(LogNevergone, Log,
		TEXT("[ExcursionEncounterSubsystem] world resumed (%d actors)"),
		FrozenActors.Num());

	FrozenActors.Reset();
}

AExplorationPlayerController* UExcursionEncounterSubsystem::GetPlayerController(const ACharacter* Player)
{
	return Player ? Cast<AExplorationPlayerController>(Player->GetController()) : nullptr;
}
