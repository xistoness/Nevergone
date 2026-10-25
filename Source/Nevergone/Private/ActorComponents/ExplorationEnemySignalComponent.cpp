// Copyright Xyzto Works


#include "ActorComponents/ExplorationEnemySignalComponent.h"

#include "Nevergone.h"
#include "Actors/Door.h"
#include "Audio/AudioSubsystem.h"
#include "Characters/ExplorationEnemyCharacter.h"
#include "Data/ExplorationEnemyDefinition.h"
#include "Data/NoiseDefinition.h"
#include "World/ExcursionNoiseSubsystem.h"

#include "Animation/AnimMontage.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

namespace ExplorationEnemySignals
{
	// Wait before trying again when the investigation loop did not start.
	// Short enough that walking up to the enemy brings the sound in quickly.
	static constexpr float LoopRetryIntervalSeconds = 0.5f;
}

UExplorationEnemySignalComponent::UExplorationEnemySignalComponent()
{
	// Only the investigation loop needs a tick, and only while it is wanted.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

EExplorationEnemySignalMoment UExplorationEnemySignalComponent::GetSignalMoment(EExplorationEnemyState OldState,
	EExplorationEnemyState NewState)
{
	if (OldState == NewState)
	{
		return EExplorationEnemySignalMoment::None;
	}

	switch (NewState)
	{
	case EExplorationEnemyState::Suspicious:
		return EExplorationEnemySignalMoment::Suspicion;

	case EExplorationEnemyState::Investigate:
		return EExplorationEnemySignalMoment::Investigation;

	case EExplorationEnemyState::Chase:
		return EExplorationEnemySignalMoment::Chase;

	case EExplorationEnemyState::Patrol:
		// Giving up only after noticing something. The first patrol after
		// spawn, getting up and finishing a meal are not a give-up. Suspicion
		// never returns to patrol: it ends in a chase or an investigation.
		return OldState == EExplorationEnemyState::Investigate ||
			OldState == EExplorationEnemyState::Chase
			? EExplorationEnemySignalMoment::GiveUp
			: EExplorationEnemySignalMoment::None;

	default:
		return EExplorationEnemySignalMoment::None;
	}
}

void UExplorationEnemySignalComponent::BeginPlay()
{
	Super::BeginPlay();

	AExplorationEnemyCharacter* Enemy = GetEnemy();

	if (!Enemy)
	{
		UE_LOG(LogNevergone, Error,
			TEXT("[ExplorationEnemySignalComponent] %s is not on an AExplorationEnemyCharacter; no signals will play"),
			*GetNameSafe(GetOwner()));

		return;
	}

	StateChangedHandle = Enemy->OnStateChanged.AddUObject(this, &UExplorationEnemySignalComponent::HandleStateChanged);

	DoorSignalHandle = Enemy->OnDoorSignal.AddUObject(this, &UExplorationEnemySignalComponent::HandleDoorSignal);
}

void UExplorationEnemySignalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopInvestigationLoop();

	if (AExplorationEnemyCharacter* Enemy = GetEnemy())
	{
		Enemy->OnStateChanged.Remove(StateChangedHandle);
		Enemy->OnDoorSignal.Remove(DoorSignalHandle);
	}

	StateChangedHandle.Reset();
	DoorSignalHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UExplorationEnemySignalComponent::HandleStateChanged(EExplorationEnemyState OldState,
	EExplorationEnemyState NewState)
{
	if (OldState == EExplorationEnemyState::Investigate)
	{
		StopInvestigationLoop();
	}

	const UExplorationEnemyDefinition* Definition = GetDefinition();

	if (!Definition)
	{
		return;
	}

	switch (GetSignalMoment(OldState, NewState))
	{
	case EExplorationEnemySignalMoment::Suspicion:
		PlaySignal(Definition->SuspicionSignal, TEXT("suspicion"));
		break;

	case EExplorationEnemySignalMoment::Investigation:
		// The sound is the loop; only the montage plays once.
		PlayMontage(Definition->InvestigationSignal.Montage, TEXT("investigation"));
		StartInvestigationLoop();
		break;

	case EExplorationEnemySignalMoment::Chase:
		PlayChaseSignal(Definition->ChaseSignal);
		break;

	case EExplorationEnemySignalMoment::GiveUp:
		PlaySignal(Definition->GiveUpSignal, TEXT("give-up"));
		break;

	default:
		break;
	}
}

void UExplorationEnemySignalComponent::HandleDoorSignal(EExplorationEnemySignalMoment Moment, ADoor* Door)
{
	const UExplorationEnemyDefinition* Definition = GetDefinition();

	if (!Definition || !Door)
	{
		return;
	}

	switch (Moment)
	{
	case EExplorationEnemySignalMoment::DoorKnock:
		PlayDoorSignal(Definition->KnockSignal, Door, TEXT("knock"));
		break;

	case EExplorationEnemySignalMoment::DoorBreak:
		PlayDoorSignal(Definition->BreakSignal, Door, TEXT("break"));
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// One-shot signals
// ---------------------------------------------------------------------------

void UExplorationEnemySignalComponent::PlayDoorSignal(const FExplorationEnemyScreamSignal& Signal, ADoor* Door,
	const TCHAR* MomentName)
{
	PlayMontage(Signal.Montage, MomentName);

	AExplorationEnemyCharacter* Enemy = GetEnemy();

	if (!Signal.Noise || !Enemy)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s has no %s noise"),
			*GetNameSafe(Enemy), MomentName);

		return;
	}

	// Sounds at the door, which is also its source: the door never muffles
	// it for a listener on the other side.
	if (UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
	{
		Noise->EmitNoise(Signal.Noise, Door->GetNoiseLocation(), Enemy, nullptr, Door);

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s emitted %s noise %s at %s"),
			*GetNameSafe(Enemy), MomentName, *GetNameSafe(Signal.Noise), *GetNameSafe(Door));
	}
}

void UExplorationEnemySignalComponent::PlaySignal(const FExplorationEnemySignal& Signal, const TCHAR* MomentName)
{
	PlayMontage(Signal.Montage, MomentName);

	AExplorationEnemyCharacter* Enemy = GetEnemy();

	if (!Signal.Sound || !Enemy)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s has no %s sound"),
			*GetNameSafe(Enemy), MomentName);

		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	if (UAudioSubsystem* Audio = GameInstance ? GameInstance->GetSubsystem<UAudioSubsystem>() : nullptr)
	{
		Audio->PlaySFXAttached(Signal.Sound, Enemy->GetRootComponent());

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s played %s sound %s"),
			*GetNameSafe(Enemy), MomentName, *GetNameSafe(Signal.Sound));
	}
}

void UExplorationEnemySignalComponent::PlayChaseSignal(const FExplorationEnemyScreamSignal& Signal)
{
	PlayMontage(Signal.Montage, TEXT("chase"));

	AExplorationEnemyCharacter* Enemy = GetEnemy();

	if (!Signal.Noise || !Enemy)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s has no chase scream noise"),
			*GetNameSafe(Enemy));

		return;
	}

	// The noise plays its own sound for the player and reports the scream to
	// every other enemy's hearing.
	if (UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
	{
		Noise->EmitNoise(Signal.Noise, Enemy->GetActorLocation(), Enemy);

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s screamed %s"),
			*GetNameSafe(Enemy), *GetNameSafe(Signal.Noise));
	}
}

void UExplorationEnemySignalComponent::PlayMontage(UAnimMontage* Montage, const TCHAR* MomentName)
{
	AExplorationEnemyCharacter* Enemy = GetEnemy();

	if (!Montage || !Enemy)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s has no %s montage"),
			*GetNameSafe(Enemy), MomentName);

		return;
	}

	const float Length = Enemy->PlayAnimMontage(Montage);

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemySignalComponent] %s played %s montage %s (%.2fs)"),
		*GetNameSafe(Enemy), MomentName, *GetNameSafe(Montage), Length);
}

// ---------------------------------------------------------------------------
// Investigation loop
// ---------------------------------------------------------------------------

void UExplorationEnemySignalComponent::StartInvestigationLoop()
{
	const UExplorationEnemyDefinition* Definition = GetDefinition();

	if (!Definition || !Definition->InvestigationSignal.Sound)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s has no investigation sound"),
			*GetNameSafe(GetOwner()));

		return;
	}

	bInvestigationLoopWanted = true;

	LoopRetrySeconds = 0.0f;

	SpawnInvestigationLoopSound();

	SetComponentTickEnabled(true);
}

void UExplorationEnemySignalComponent::StopInvestigationLoop()
{
	const bool bWasWanted = bInvestigationLoopWanted;

	bInvestigationLoopWanted = false;

	SetComponentTickEnabled(false);

	if (IsValid(InvestigationLoop))
	{
		InvestigationLoop->Stop();
	}

	InvestigationLoop = nullptr;

	if (bWasWanted)
	{
		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s stopped the investigation loop"),
			*GetNameSafe(GetOwner()));
	}
}

void UExplorationEnemySignalComponent::SpawnInvestigationLoopSound()
{
	const UExplorationEnemyDefinition* Definition = GetDefinition();

	AExplorationEnemyCharacter* Enemy = GetEnemy();

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	UAudioSubsystem* Audio = GameInstance ? GameInstance->GetSubsystem<UAudioSubsystem>() : nullptr;

	InvestigationLoop = (Definition && Enemy && Audio)
		? Audio->PlaySFXAttached(Definition->InvestigationSignal.Sound, Enemy->GetRootComponent())
		: nullptr;

	if (!IsValid(InvestigationLoop) || !InvestigationLoop->IsPlaying())
	{
		// The engine skips a short sound the listener is too far to hear,
		// and a concurrency limit can refuse one. Neither is permanent, so
		// the loop stays wanted and tries again later instead of every tick.
		LoopRetrySeconds = ExplorationEnemySignals::LoopRetryIntervalSeconds;

		UE_LOG(LogNevergone, Verbose,
			TEXT("[ExplorationEnemySignalComponent] %s could not start the investigation loop; retrying in %.1fs"),
			*GetNameSafe(Enemy), LoopRetrySeconds);

		return;
	}

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationEnemySignalComponent] %s playing investigation loop %s"),
		*GetNameSafe(Enemy), *GetNameSafe(Definition->InvestigationSignal.Sound));
}

void UExplorationEnemySignalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInvestigationLoopWanted)
	{
		SetComponentTickEnabled(false);

		return;
	}

	const AActor* Owner = GetOwner();

	// Frozen by a player capture. The world is not paused, so the audio
	// engine would keep playing the loop on its own.
	const bool bFrozen = Owner && Owner->CustomTimeDilation <= 0.0f;

	if (IsValid(InvestigationLoop) && InvestigationLoop->IsPlaying())
	{
		const bool bIsPaused = InvestigationLoop->bIsPaused;

		if (bIsPaused != bFrozen)
		{
			InvestigationLoop->SetPaused(bFrozen);
		}

		return;
	}

	// Frozen: the loop resumes with the world.
	if (bFrozen)
	{
		return;
	}

	if (LoopRetrySeconds > 0.0f)
	{
		LoopRetrySeconds -= DeltaTime;

		return;
	}

	// The sound ended on its own (the asset does not loop), or the last
	// attempt did not start. Start it again.
	SpawnInvestigationLoopSound();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

AExplorationEnemyCharacter* UExplorationEnemySignalComponent::GetEnemy() const
{
	return Cast<AExplorationEnemyCharacter>(GetOwner());
}

const UExplorationEnemyDefinition* UExplorationEnemySignalComponent::GetDefinition() const
{
	const AExplorationEnemyCharacter* Enemy = GetEnemy();

	return Enemy ? Enemy->GetDefinition() : nullptr;
}
