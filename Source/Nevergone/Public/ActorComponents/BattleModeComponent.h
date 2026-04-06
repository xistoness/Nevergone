// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UnitStatsComponent.h"
#include "ActorComponents/CharacterModeComponent.h"
#include "Interfaces/BattleInputReceiver.h"
#include "Types/BattleTypes.h"
#include "Types/CharacterTypes.h"
#include "BattleModeComponent.generated.h"

class UAbilityPreviewRenderer;
class UActionResolver;
class UBattleGameplayAbility;
class UBattleMovementAbility;
class UCombatEventBus;
class UGameplayAbility;
class ACharacterBase;
class UTurnManager;
class UBattleState;
class UAbilityDefinition;
class UStatusEffectManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilitySelectionChanged, int32, NewIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionUseStarted, ACharacterBase*, ActingUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionUseFinished, ACharacterBase*, ActingUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionPointsDepleted, ACharacterBase*, Character);

/**
 * Fired at the end of every PreviewCurrentAbility() call.
 *
 * @param APCost           AP this ability would spend if confirmed. 0 when preview is invalid.
 * @param RemainingAP      AP the unit still has available this turn.
 * @param bPreviewIsValid  Whether the current target/tile is a legal action.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnPreviewUpdated,
    int32, APCost,
    int32, RemainingAP,
    bool,  bPreviewIsValid);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NEVERGONE_API UBattleModeComponent : public UCharacterModeComponent, public IBattleInputReceiver
{
    GENERATED_BODY()

public:
    UBattleModeComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;
    UUnitStatsComponent* GetUnitStats() const;

    void HandleUnitDeath(ACharacterBase* OwnerCharacter);
    virtual void EnterMode() override;
    virtual void ExitMode() override;

    virtual void Input_Confirm() override;
    virtual void Input_Cancel() override;
    virtual void Input_Hover(const FVector& WorldLocation) override;
    virtual void Input_SelectNextAction() override;
    virtual void Input_SelectPreviousAction() override;

    void HandleAbilitySelected();

    bool PreviewCurrentAbilityExecution(FActionResult& ExecutionResult);
    void RenderVisualPreview();
    void PreviewCurrentAbility(FActionResult& PreviewResult);

    bool ExecuteCurrentAction(const FActionContext& Context, const FActionResult& Result);
    bool ExecuteActionFromAI(const FActionContext& InContext);

    void HandleActionStarted();
    void HandleActionFinished();

    void UpdateOccupancy(const FIntPoint& NewGridCoord) const;

    const FActionContext& GetCurrentContext() const { return CurrentContext; }

    // --- Injectors — set by CombatManager at battle start ---
    void SetCombatEventBus(UCombatEventBus* InBus);
    UCombatEventBus* GetCombatEventBus() const { return CombatEventBus; }
    void SetTurnManager(UTurnManager* InTurnManager);
    UTurnManager* GetTurnManager() const { return TurnManager; }
    void SetBattleState(UBattleState* InBattleState);
    UBattleState* GetBattleState() const { return BattleState; }
    void SetStatusEffectManager(UStatusEffectManager* InManager);
    UStatusEffectManager* GetStatusEffectManager() const { return StatusEffectManager; }

    TSubclassOf<UBattleMovementAbility> GetEquippedMovementAbilityClass() const;
    const TArray<FUnitAbilityEntry>& GetGrantedBattleAbilities() const { return GrantedBattleAbilities; }

    // -----------------------------------------------------------------------
    // Per-definition cooldown map
    // Cooldown is stored here (not on the GAS instance) so Thunder and
    // HealingRain have independent counters even though both use GA_AoE.
    // -----------------------------------------------------------------------

    bool  IsDefinitionOnCooldown(const UAbilityDefinition* Def) const;
    int32 GetDefinitionCooldownTurns(const UAbilityDefinition* Def) const;
    void  StartDefinitionCooldown(const UAbilityDefinition* Def, int32 Turns);
    void  TickDefinitionCooldowns();

    bool SelectAbilityByIndex(int32 AbilityIndex);
    bool SelectAbilityByTag(const FGameplayTag& AbilityTag);
    bool SelectDefaultAbility();

    // -----------------------------------------------------------------------
    // Delegates
    // -----------------------------------------------------------------------

    UPROPERTY(BlueprintAssignable)
    FOnActionUseStarted OnActionUseStarted;

    UPROPERTY(BlueprintAssignable)
    FOnActionUseFinished OnActionUseFinished;

    FOnActionPointsDepleted OnActionPointsDepleted;

    UPROPERTY(BlueprintAssignable)
    FOnAbilitySelectionChanged OnAbilitySelectionChanged;

    /**
     * Fired every time PreviewCurrentAbility() resolves, including on hover.
     * Use this to update AP cost and remaining AP displays in the HUD.
     */
    UPROPERTY(BlueprintAssignable)
    FOnPreviewUpdated OnPreviewUpdated;

protected:
    bool HasLineOfSight() const;

    void InitializeAbilitiesFromDefinition();
    void EnsureAbilityGranted(TSubclassOf<UGameplayAbility> AbilityClass);
    void InjectAbilityDefinition(const FUnitAbilityEntry& Entry);
    void SetSelectedAbilityFromEntry(const FUnitAbilityEntry& Entry);
    EBattleAbilitySelectionMode GetCurrentAbilitySelectionMode() const;

    bool TryLockCurrentTarget();
    void ClearLockedTarget();
    bool IsCurrentAbilityInApproachSelection() const;

protected:
    UPROPERTY()
    FActionContext CurrentContext;

    UPROPERTY()
    UAbilityPreviewRenderer* CurrentPreview = nullptr;

    UPROPERTY()
    UActionResolver* ActionResolver = nullptr;

    UPROPERTY()
    TArray<FUnitAbilityEntry> GrantedBattleAbilities;

    UPROPERTY()
    TMap<TObjectPtr<const UAbilityDefinition>, int32> DefinitionCooldowns;

    UPROPERTY()
    int32 SelectedAbilityIndex = INDEX_NONE;

    UPROPERTY()
    bool bActionInProgress = false;

    UPROPERTY()
    TObjectPtr<UCombatEventBus> CombatEventBus;

    UPROPERTY()
    TObjectPtr<UTurnManager> TurnManager;

    UPROPERTY()
    TObjectPtr<UBattleState> BattleState;

    UPROPERTY()
    TObjectPtr<UStatusEffectManager> StatusEffectManager;
};