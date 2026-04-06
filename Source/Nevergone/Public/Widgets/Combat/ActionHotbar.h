// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/BattleInputContext.h"
#include "ActionHotbar.generated.h"

class ACharacterBase;
class UBattleModeComponent;
class UCombatManager;
class UActionSlot;
class UHorizontalBox;
class UTextBlock;
class UTurnManager;

UCLASS()
class NEVERGONE_API UActionHotbar : public UUserWidget
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "ActionHotbar")
    void InitializeWithCombatManager(UCombatManager* InCombatManager);

    UFUNCTION(BlueprintCallable, Category = "ActionHotbar")
    void RefreshSelection(int32 AbilityIndex);

    UFUNCTION(BlueprintCallable, Category = "ActionHotbar")
    void ClearHotbar();

protected:

    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActionHotbar|Config")
    TSubclassOf<UActionSlot> ActionSlotClass;

    // ---- Widget bindings ----

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UHorizontalBox> SlotsBox;

    /** Shows the AP this ability costs on the current hover target. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> APCost;

    /** Shows how many AP the active unit has left this turn. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> RemainingAP;

private:

    void ShowForUnit(ACharacterBase* Unit);
    void HideHotbar();
    void ClearSlots();
    void UnbindFromCurrentUnit();
    void UnbindFromCombatManager();

    /** Updates APCost and RemainingAP text blocks. Clears both when bClear is true. */
    void RefreshAPDisplay(int32 Cost, int32 Remaining, bool bPreviewIsValid);
    void ClearAPDisplay();

    // ---- CombatManager handlers ----

    UFUNCTION()
    void HandleActiveUnitChanged(ACharacterBase* NewActiveUnit);

    UFUNCTION()
    void HandleEnemyTurnBegan();

    // ---- BattleModeComponent handlers ----

    UFUNCTION()
    void HandleActionStarted(ACharacterBase* ActingUnit);

    UFUNCTION()
    void HandleActionFinished(ACharacterBase* ActingUnit);

    UFUNCTION()
    void HandleAbilitySelectionChanged(int32 NewIndex);

    /** Fired by BattleModeComponent::OnPreviewUpdated on every hover update. */
    UFUNCTION()
    void HandlePreviewUpdated(int32 Cost, int32 Remaining, bool bPreviewIsValid);

    // ---- Cooldown display ----

    /**
     * Queries every ability instance on the tracked unit and calls
     * SetCooldown() on the corresponding ActionSlot.
     * Called after ShowForUnit and on every turn state change.
     */
    void RefreshCooldownStates();
    void HandleTurnStateChangedForCooldown(EBattleTurnOwner NewOwner, EBattleTurnPhase NewPhase);

    FDelegateHandle TurnStateHandle;

    // ---- State ----

    UPROPERTY()
    TArray<TObjectPtr<UActionSlot>> Slots;

    UPROPERTY()
    TWeakObjectPtr<ACharacterBase> TrackedUnit;

    UPROPERTY()
    TWeakObjectPtr<UBattleModeComponent> TrackedBattleMode;

    UPROPERTY()
    TWeakObjectPtr<UCombatManager> TrackedCombatManager;

    int32 CurrentSelectedIndex = INDEX_NONE;
    bool bIsLocked = false;
};