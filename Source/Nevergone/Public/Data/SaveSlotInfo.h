// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "SaveSlotInfo.generated.h"

/**
 * Lightweight descriptor for a save slot — used by the Load Game panel
 * to display slot info without loading the full save into memory.
 */
USTRUCT(BlueprintType)
struct NEVERGONE_API FSaveSlotInfo
{
	GENERATED_BODY()
 
	// Slot name used with UGameplayStatics (e.g. "Slot_0", "Slot_1", "Slot_2")
	UPROPERTY(BlueprintReadOnly)
	FString SlotName;
 
	// Slot index (0, 1, 2 ...)
	UPROPERTY(BlueprintReadOnly)
	int32 SlotIndex = 0;
 
	// Whether this slot has any save data
	UPROPERTY(BlueprintReadOnly)
	bool bIsOccupied = false;
 
	// Human-readable label from SaveDisplayName (empty if slot is empty)
	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;
 
	// Playtime formatted as "Xh Ym" (empty if slot is empty)
	UPROPERTY(BlueprintReadOnly)
	FString PlaytimeFormatted;
 
	// Last saved timestamp formatted as relative string (empty if slot is empty)
	UPROPERTY(BlueprintReadOnly)
	FString LastSavedFormatted;
 
	// Level name where the save was made (empty if slot is empty)
	UPROPERTY(BlueprintReadOnly)
	FName SavedLevelName = NAME_None;
};