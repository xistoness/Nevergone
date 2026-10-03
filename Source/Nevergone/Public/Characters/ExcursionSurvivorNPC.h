// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Characters/DialogueNPC.h"
#include "ExcursionSurvivorNPC.generated.h"

class AExcursionSurvivorNPC;

// Where the survivor stands in the rescue. The pawn owns it so any system
// (enemy perception, the excursion exit, the AnimInstance) can read it
// without knowing the controller exists.
UENUM(BlueprintType)
enum class EExcursionSurvivorState : uint8
{
	// Found, not talked into leaving yet. Enemies ignore the survivor.
	Unrecruited,

	// Recruited and following the player-controlled survivor.
	Following,

	// Recruited and told to hold position.
	Waiting,

	// Caught by an enemy, or died with the player. No longer a survivor
	// body: enemies ignore it and the excursion does not count it.
	Dead,
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExcursionSurvivorStateChanged,
	AExcursionSurvivorNPC* /*Survivor*/, EExcursionSurvivorState /*NewState*/);

// ---------------------------------------------------------------------------
// AExcursionSurvivorNPC
//
// A survivor found inside the building. Talks through the regular dialogue
// flow, and a choice carrying the FollowPlayer effect recruits them. Once
// recruited, interacting again toggles between following and waiting
// without opening a dialogue.
//
// Inherits ACharacterBase through ADialogueNPC on purpose: the survivor gets
// the same UExplorationModeComponent as the player, so speeds, crouched
// height, stamina and panting all come from one place.
// AExcursionSurvivorController drives that component through
// IExplorationInputReceiver, the same door the player controller uses.
//
// Nothing here is saved: an excursion is a single session.
// ---------------------------------------------------------------------------
UCLASS()
class NEVERGONE_API AExcursionSurvivorNPC : public ADialogueNPC
{
	GENERATED_BODY()

public:

	AExcursionSurvivorNPC();

	// --- IInteractable ---
	virtual void Interact_Implementation(AActor* Interactor) override;

	// Starts following. Called by the dialogue FollowPlayer effect. Does
	// nothing once the survivor is already recruited.
	void Recruit();

	// Following <-> Waiting. Ignored before recruitment.
	void ToggleFollow();

	// Called by UExcursionEncounterSubsystem when an enemy catches the
	// survivor or the player dies. Leaves exploration mode, so the body
	// stops breathing hard and making noise.
	void MarkDead();

	EExcursionSurvivorState GetSurvivorState() const { return SurvivorState; }

	const FText& GetDisplayName() const { return DisplayName; }

	// Recruited and alive: following or waiting.
	bool IsRecruited() const
	{
		return SurvivorState == EExcursionSurvivorState::Following ||
			SurvivorState == EExcursionSurvivorState::Waiting;
	}

	FOnExcursionSurvivorStateChanged OnSurvivorStateChanged;

	// --- Survivor bodies ---
	//
	// The survivors enemies hunt: the one the player controls and every
	// recruited survivor. Checking player control instead of a class keeps
	// this right when the party leader is swapped mid-excursion. Enemy
	// perception and knockable props both ask here, so the two never
	// disagree on who counts.

	static bool IsSurvivorBody(const AActor* Actor);

	// Every survivor body in the world, for checks that poll instead of
	// receiving events (smell).
	static void GatherSurvivorBodies(const UWorld* World, TArray<AActor*>& OutBodies);

protected:

	virtual void BeginPlay() override;

	// Name shown on the excursion result when this survivor is rescued.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survivor")
	FText DisplayName;

private:

	void SetSurvivorState(EExcursionSurvivorState NewState);

	UPROPERTY(VisibleInstanceOnly, Category = "Survivor")
	EExcursionSurvivorState SurvivorState = EExcursionSurvivorState::Unrecruited;
};
