// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "NevergoneCharacter.h"
#include "Interfaces/SaveParticipant.h"
#include "Components/DecalComponent.h"
#include "CharacterBase.generated.h"

class UInputMappingContext;
class UInputAction;
class USaveableComponent;
class UInteractableComponent;
class UCharacterModeComponent;
class UExplorationModeComponent;
class UBattleModeComponent;
class UUnitStatsComponent;
class UMyAbilitySystemComponent;
class UFloatingCombatTextWidget;
enum class EFloatingTextType : uint8;

UCLASS()
class NEVERGONE_API ACharacterBase : public ANevergoneCharacter, public ISaveParticipant
{
	GENERATED_BODY()

public:
	
	DECLARE_MULTICAST_DELEGATE(FOnPathMoveFinished);
	FOnPathMoveFinished OnPathMoveFinished;
	
	ACharacterBase();
	
	void EnableExplorationMode();
	void EnableBattleMode();
	
	void SetSelected(bool bSelected);
	
	// Camera
	void ResetCamera();
	FVector GetGroundAlignedLocation(const FVector& GroundLocation) const;

	// Save system
	virtual void WriteSaveData_Implementation(FActorSaveData& OutData) const override;
	virtual void ReadSaveData_Implementation(const FActorSaveData& InData) override;
	virtual void OnPostRestore_Implementation() override;
	
	// Getters
	UUnitStatsComponent* GetUnitStats() const;
	
	UBattleModeComponent* GetBattleModeComponent() const;
	
	UMyAbilitySystemComponent* GetAbilitySystemComponent();
	
	bool IsInExplorationMode();
	bool IsInBattleMode();
	
	// Setters
	
	void MoveToLocation(const FVector& InLocation, const TArray<FVector>& WorldPoints);

	/**
	 * Spawns a floating text in world space above this character.
	 *
	 * @param Text    Text to display (e.g., "42", "STUN", "MISS")
	 * @param Type    Semantic type — controls color and icon in the Blueprint
	 * @param Icon    Optional icon; nullptr = no icon
	 */
	void SpawnFloatingText(const FString& Text, EFloatingTextType Type, UTexture2D* Icon = nullptr);

	/**
	 * Called when this unit's HP reaches zero during combat.
	 * Hides the mesh and disables capsule collision so the actor stays in memory
	 * without blocking movement or cluttering the viewport.
	 * Declared as BlueprintNativeEvent so Blueprint subclasses can layer a death
	 * animation on top without replacing the C++ behaviour.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	void PlayDeathVisual();
	virtual void PlayDeathVisual_Implementation();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void BeginDestroy() override;
	virtual void Destroyed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;
	
	void SetMode(UCharacterModeComponent* NewMode);
	
	// Path movement
	UFUNCTION(BlueprintCallable, Category="Movement")
	void MoveAlongPath_Lerped(const TArray<FVector>& WorldPoints);
	
	// Camera persistence
	float ArmLength;
	FRotator ControllerRot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Modes")
	UExplorationModeComponent* ExplorationMode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Modes")
	UBattleModeComponent* BattleMode;
	
	UPROPERTY()
	UCharacterModeComponent* ActiveMode;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	USaveableComponent* SaveableComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	UUnitStatsComponent* UnitStatsComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UMyAbilitySystemComponent* AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Selection")
	UDecalComponent* SelectionIndicator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UFloatingCombatTextWidget> FloatingTextClass;

private:	
	// --- Lerp movement state ---
	UPROPERTY()
	TArray<FVector> PendingPathPoints;

	int32 CurrentPathIndex = INDEX_NONE;

	UPROPERTY()
	bool bIsMovingAlongPath = false;

	UPROPERTY(EditAnywhere, Category="Movement|Lerp")
	float MoveSpeedUnitsPerSec = 600.0f;

	UPROPERTY(EditAnywhere, Category="Movement|Lerp")
	float AcceptRadius = 5.0f;
	
	/**
	 * Tracks how many floating text widgets are currently alive above this character.
	 * Used to stagger each new widget upward so they do not overlap.
	 */
	int32 ActiveFloatingTextCount = 0;

	// Advances movement toward current point
	void TickPathMove(float DeltaSeconds);

	// Stops and clears state
	void StopPathMove();
	void FinishPathMove();
};