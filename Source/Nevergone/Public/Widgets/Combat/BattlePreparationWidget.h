// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/BattleTypes.h"
#include "BattlePreparationWidget.generated.h"

class UBattlePreparationContext;
class UTextBlock;
class UButton;
class UVerticalBox;

/**
 * Pre-battle info screen shown when the player walks into an encounter.
 *
 * Displays:
 *   - Enemy roster: class name + level for each enemy unit
 *   - Ally roster:  class name + level for each party member
 *   - A "Start Battle" button that fires OnStartBattleClicked
 *
 * All visual styling is handled in the Blueprint subclass.
 * The controller calls PopulateFromContext() after creating this widget.
 */
UCLASS(Abstract)
class NEVERGONE_API UBattlePreparationWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Populates all text blocks and lists from the preparation context.
	 * Must be called before the widget is visible.
	 *
	 * @param Context  The preparation context built by TowerFloorGameMode / EncounterGeneratorSubsystem.
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle Preparation")
	void PopulateFromContext(UBattlePreparationContext* Context);

	/**
	 * Fired when the player clicks "Start Battle".
	 * The owning BattlePreparationController binds this to RequestBattleStart().
	 */
	DECLARE_MULTICAST_DELEGATE(FOnStartBattleClicked);
	FOnStartBattleClicked OnStartBattleClicked;
	
	DECLARE_MULTICAST_DELEGATE(FOnAbortBattleClicked);
	FOnAbortBattleClicked OnAbortBattleClicked;

protected:

	virtual void NativeConstruct() override;

	// -----------------------------------------------------------------------
	// Named UMG widgets — bind in the Blueprint designer
	// -----------------------------------------------------------------------

	/** Header label, e.g. "Battle Preparation" or the floor name. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeaderText;

	/** Enemy count summary, e.g. "Enemies: 3". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyCountText;

	/**
	 * Vertical list of enemy entries.
	 * Each entry is a text row created in C++ via AddUnitRow().
	 * The Blueprint subclass is responsible for styling the container.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> EnemyListBox;

	/** Ally count summary, e.g. "Your Party: 2". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AllyCountText;

	/** Vertical list of ally entries — same pattern as EnemyListBox. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> AllyListBox;

	/** Confirms preparation and starts the battle. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartBattleButton;
	
	/** Aborts the battle. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AbortBattleButton;

	// -----------------------------------------------------------------------
	// Blueprint event
	// -----------------------------------------------------------------------

	/** Called after PopulateFromContext() fills all data. Animate or style here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle Preparation")
	void OnPreparationReady();

private:

	/**
	 * Appends a single text row ("<DisplayName>  Lv.<Level>") to the target box.
	 * Extracted so enemy and ally loops share the same formatting logic.
	 *
	 * @param TargetBox   The UVerticalBox to append to (EnemyListBox or AllyListBox).
	 * @param DisplayName Class name taken from UUnitDefinition::DisplayName.
	 * @param Level       Unit level from UnitStatsComponent or FGeneratedEnemyData.
	 */
	void AddUnitRow(UVerticalBox* TargetBox, const FText& DisplayName, int32 Level);

	UFUNCTION()
	void HandleStartBattleClicked();
	UFUNCTION()
	void HandleAbortBattleClicked();
};