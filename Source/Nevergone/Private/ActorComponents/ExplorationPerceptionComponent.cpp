// Copyright Xyzto Works


#include "ActorComponents/ExplorationPerceptionComponent.h"

#include "Nevergone.h"
#include "Characters/ExcursionSurvivorNPC.h"
#include "Data/ExplorationEnemyDefinition.h"
#include "EngineTools/CollisionChannels.h"
#include "World/ExcursionNoiseSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"

namespace ExplorationHearing
{
	// Occluders beyond this add nothing: four walls already leave 6% of the
	// range at the default factor, and every extra one costs a trace.
	static constexpr int32 MaxCountedOccluders = 4;

	// An occluder this close to the noise is treated as its source (the door
	// that slammed, the shelf a bottle fell from) and does not muffle it.
	static constexpr float SourceClearance = 25.0f;
}

UExplorationPerceptionComponent::UExplorationPerceptionComponent()
{
	// Only needed for the smell poll; disabled again when the type cannot smell.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Smell does not need frame accuracy, and the radius is small relative to
	// how fast a survivor can move. Ten times a second is plenty.
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UExplorationPerceptionComponent::BeginPlay()
{
	Super::BeginPlay();

	OnTargetPerceptionUpdated.AddDynamic(this, &UExplorationPerceptionComponent::HandleTargetPerceptionUpdated);
}

void UExplorationPerceptionComponent::InitializeFromDefinition(const UExplorationEnemyDefinition* InDefinition)
{
	if (!InDefinition)
	{
		UE_LOG(LogNevergone, Warning,
			TEXT("[ExplorationPerceptionComponent] %s initialized without a definition; the enemy will sense nothing"),
			*GetNameSafe(GetOwner()));

		return;
	}

	Definition = InDefinition;

	// --- Sight ---

	UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(this, TEXT("ExplorationSightConfig"));

	SightConfig->SightRadius = Definition->SightRadius;
	SightConfig->LoseSightRadius = Definition->LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = Definition->SightHalfAngleDegrees;
	SightConfig->SetMaxAge(Definition->MemorySeconds);

	// No team agent interface exists in the project, so every actor reads as
	// neutral to the perception system. Without this the enemy senses nothing
	// at all and the failure is completely silent.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	ConfigureSense(*SightConfig);

	// --- Hearing ---

	UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(this, TEXT("ExplorationHearingConfig"));

	HearingConfig->HearingRange = Definition->HearingRadius;
	HearingConfig->SetMaxAge(Definition->MemorySeconds);

	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	ConfigureSense(*HearingConfig);

	SetDominantSense(SightConfig->GetSenseImplementation());

	RequestStimuliListenerUpdate();

	SetComponentTickEnabled(Definition->bCanSmell);

	UE_LOG(LogNevergone, Log,
		TEXT("[ExplorationPerceptionComponent] %s configured: sight %.0f/%.0f at %.0f deg, hearing %.0f, smell %s"),
		*GetNameSafe(GetOwner()),
		Definition->SightRadius, Definition->LoseSightRadius, Definition->SightHalfAngleDegrees,
		Definition->HearingRadius,
		Definition->bCanSmell ? *FString::Printf(TEXT("%.0f"), Definition->SmellRadius) : TEXT("off"));
}

void UExplorationPerceptionComponent::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!IsValid(Actor))
	{
		return;
	}

	const bool bSensed = Stimulus.WasSuccessfullySensed();

	const bool bIsBody = AExcursionSurvivorNPC::IsSurvivorBody(Actor);

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		// Unrecruited survivors are recorded but never reported. See the
		// header: a stable sight stimulus is not fired again on recruitment.
		if (!bIsBody && !Actor->IsA<AExcursionSurvivorNPC>())
		{
			return;
		}

		FExplorationSensedSurvivor& Entry = SensedSurvivors.FindOrAdd(Actor);

		Entry.bSight = bSensed;

		if (bSensed)
		{
			RecordStimulus(Actor, Actor->GetActorLocation());

			if (bIsBody)
			{
				OnSurvivorSensed.Broadcast(Actor, Actor->GetActorLocation(), EExplorationSenseChannel::Sight);
			}
		}
		else
		{
			// Remember where it was, not where it is. The whole point of
			// breaking line of sight is that the enemy loses the live position.
			RecordStimulus(Actor, Stimulus.StimulusLocation);

			if (bIsBody)
			{
				OnSurvivorLost.Broadcast(Actor);
			}
		}

		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		// A noise is an instant, not a state: only the positive edge matters.
		if (bIsBody && bSensed && IsNoiseAudible(Actor, Stimulus))
		{
			RecordStimulus(Actor, Stimulus.StimulusLocation);

			OnSurvivorSensed.Broadcast(Actor, Stimulus.StimulusLocation, EExplorationSenseChannel::Hearing);
		}
	}
}

void UExplorationPerceptionComponent::UpdateSmell()
{
	if (!Definition || !Definition->bCanSmell)
	{
		return;
	}

	const APawn* OwnerPawn = GetListenerPawn();

	if (!OwnerPawn)
	{
		return;
	}

	TArray<AActor*> Bodies;

	AExcursionSurvivorNPC::GatherSurvivorBodies(GetWorld(), Bodies);

	const float RadiusSquared = FMath::Square(Definition->SmellRadius);

	TSet<AActor*> InRange;

	for (AActor* Body : Bodies)
	{
		if (FVector::DistSquared(Body->GetActorLocation(), OwnerPawn->GetActorLocation()) > RadiusSquared)
		{
			continue;
		}

		InRange.Add(Body);

		FExplorationSensedSurvivor& Entry = SensedSurvivors.FindOrAdd(Body);

		if (!Entry.bSmell)
		{
			UE_LOG(LogNevergone, Verbose,
				TEXT("[ExplorationPerceptionComponent] %s smelled %s"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Body));
		}

		Entry.bSmell = true;

		RecordStimulus(Body, Body->GetActorLocation());

		OnSurvivorSensed.Broadcast(Body, Body->GetActorLocation(), EExplorationSenseChannel::Smell);
	}

	// Anyone still marked as smelled who is out of range now, or who stopped
	// being a body enemies hunt, is lost to smell.
	for (TPair<TWeakObjectPtr<AActor>, FExplorationSensedSurvivor>& Pair : SensedSurvivors)
	{
		AActor* Survivor = Pair.Key.Get();

		if (!Pair.Value.bSmell || InRange.Contains(Survivor))
		{
			continue;
		}

		Pair.Value.bSmell = false;

		if (Survivor)
		{
			OnSurvivorLost.Broadcast(Survivor);
		}
	}
}

void UExplorationPerceptionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSmell();
}

bool UExplorationPerceptionComponent::IsNoiseAudible(const AActor* Instigator, const FAIStimulus& Stimulus) const
{
	const APawn* ListenerPawn = GetListenerPawn();

	const UWorld* World = GetWorld();

	if (!Definition || !ListenerPawn || !World)
	{
		return true;
	}

	const FVector ListenerLocation = ListenerPawn->GetPawnViewLocation();

	const FVector NoiseLocation = Stimulus.StimulusLocation;

	// --- Occlusion: once per wall ---

	const int32 Occluders = CountNoiseOccluders(ListenerLocation, NoiseLocation, ListenerPawn, Instigator);

	const float Occlusion = FMath::Pow(Definition->HearingOcclusionPerWall, static_cast<float>(Occluders));

	// --- Masking: the noisier end of the path wins ---

	float Mask = 1.0f;

	if (const UExcursionNoiseSubsystem* Noise = UExcursionNoiseSubsystem::Get(this))
	{
		Mask = FMath::Min(Noise->GetHearingMaskAt(ListenerLocation), Noise->GetHearingMaskAt(NoiseLocation));
	}

	// Stimulus strength is the reported loudness, surface already applied.
	// Without walls or masks this is exactly the engine's own range check.
	const float EffectiveRange = Definition->HearingRadius * Stimulus.Strength * Occlusion * Mask;

	const float Distance = FVector::Dist(ListenerLocation, NoiseLocation);

	const bool bAudible = Distance <= EffectiveRange;

	UE_LOG(LogNevergone, Verbose,
		TEXT("[ExplorationPerceptionComponent] %s %s noise %s: distance %.0f, range %.0f (loudness %.2f, %d walls, mask %.2f)"),
		*GetNameSafe(ListenerPawn), bAudible ? TEXT("heard") : TEXT("filtered"),
		*Stimulus.Tag.ToString(), Distance, EffectiveRange, Stimulus.Strength, Occluders, Mask);

#if ENABLE_DRAW_DEBUG
	if (UExcursionNoiseSubsystem::IsDebugEnabled())
	{
		const FColor Color = bAudible ? FColor::Green : FColor::Red;

		DrawDebugLine(World, ListenerLocation, NoiseLocation, Color, false, 2.0f, 0, 1.5f);

		DrawDebugString(World, ListenerLocation + FVector(0.0f, 0.0f, 30.0f),
			FString::Printf(TEXT("%.0f / %.0f  walls %d  mask %.2f"), Distance, EffectiveRange, Occluders, Mask),
			nullptr, Color, 2.0f);
	}
#endif

	return bAudible;
}

int32 UExplorationPerceptionComponent::CountNoiseOccluders(const FVector& From, const FVector& To,
	const AActor* Listener, const AActor* Instigator) const
{
	const UWorld* World = GetWorld();

	if (!World)
	{
		return 0;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExplorationNoiseOcclusion), false);
	Params.AddIgnoredActor(Listener);

	if (Instigator)
	{
		Params.AddIgnoredActor(Instigator);
	}

	const float TotalDistance = FVector::Dist(From, To);

	int32 Count = 0;

	FHitResult Hit;

	// A single trace stops at the first wall. To count walls, each hit
	// component is ignored and the same trace is repeated until it passes.
	// A mesh containing several walls counts once.
	while (Count < ExplorationHearing::MaxCountedOccluders &&
		World->LineTraceSingleByChannel(Hit, From, To, NevergoneCollision::NoiseOcclusion, Params))
	{
		if (Hit.Distance >= TotalDistance - ExplorationHearing::SourceClearance)
		{
			break;
		}

		UPrimitiveComponent* HitComponent = Hit.GetComponent();

		if (!HitComponent)
		{
			break;
		}

		++Count;

		Params.AddIgnoredComponent(HitComponent);
	}

	return Count;
}

APawn* UExplorationPerceptionComponent::GetListenerPawn() const
{
	AActor* Owner = GetOwner();

	if (APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		return OwnerPawn;
	}

	const AController* OwnerController = Cast<AController>(Owner);

	return OwnerController ? OwnerController->GetPawn() : nullptr;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

const FExplorationSensedSurvivor* UExplorationPerceptionComponent::FindEntry(const AActor* Survivor) const
{
	// The map is keyed by a weak pointer to a mutable actor so entries can
	// hand the actor back to the controller. Looking up only reads.
	return SensedSurvivors.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Survivor)));
}

bool UExplorationPerceptionComponent::HasSightOf(const AActor* Survivor) const
{
	const FExplorationSensedSurvivor* Entry = FindEntry(Survivor);

	return Entry && Entry->bSight && AExcursionSurvivorNPC::IsSurvivorBody(Survivor);
}

bool UExplorationPerceptionComponent::HasContactWith(const AActor* Survivor) const
{
	const FExplorationSensedSurvivor* Entry = FindEntry(Survivor);

	return Entry && (Entry->bSight || Entry->bSmell) && AExcursionSurvivorNPC::IsSurvivorBody(Survivor);
}

AActor* UExplorationPerceptionComponent::GetClosestSightedSurvivor() const
{
	const APawn* ListenerPawn = GetListenerPawn();

	if (!ListenerPawn)
	{
		return nullptr;
	}

	AActor* Closest = nullptr;

	float ClosestDistanceSquared = TNumericLimits<float>::Max();

	for (const TPair<TWeakObjectPtr<AActor>, FExplorationSensedSurvivor>& Pair : SensedSurvivors)
	{
		AActor* Survivor = Pair.Key.Get();

		if (!Pair.Value.bSight || !AExcursionSurvivorNPC::IsSurvivorBody(Survivor))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Survivor->GetActorLocation(), ListenerPawn->GetActorLocation());

		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			Closest = Survivor;
		}
	}

	return Closest;
}

bool UExplorationPerceptionComponent::GetLastKnownLocationOf(const AActor* Survivor, FVector& OutLocation) const
{
	const FExplorationSensedSurvivor* Entry = FindEntry(Survivor);

	if (!Entry || !Entry->bHasLastKnownLocation)
	{
		return false;
	}

	OutLocation = Entry->LastKnownLocation;

	return true;
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

void UExplorationPerceptionComponent::RecordStimulus(AActor* Survivor, const FVector& Location)
{
	FExplorationSensedSurvivor& Entry = SensedSurvivors.FindOrAdd(Survivor);

	Entry.LastKnownLocation = Location;
	Entry.bHasLastKnownLocation = true;

	// An unrecruited survivor seen from afar must not pull the search to
	// them: only bodies enemies hunt move the most recent stimulus.
	if (AExcursionSurvivorNPC::IsSurvivorBody(Survivor))
	{
		LastKnownLocation = Location;
		bHasLastKnownLocation = true;
	}
}

void UExplorationPerceptionComponent::SetLastKnownLocation(AActor* Survivor, const FVector& Location)
{
	RecordStimulus(Survivor, Location);
}

void UExplorationPerceptionComponent::ForgetLastKnownLocation()
{
	bHasLastKnownLocation = false;

	// Per-survivor memory stays. Only entries whose actor is gone are
	// dropped, since nothing can ask about them anymore.
	for (auto It = SensedSurvivors.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
