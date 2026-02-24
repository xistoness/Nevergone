// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Types/BattleInputContext.h"
#include "UObject/Object.h"
#include "BattleInputContextBuilder.generated.h"

class UTurnManager;

UCLASS()
class NEVERGONE_API UBattleInputContextBuilder : public UObject
{
	GENERATED_BODY()
	
public:
	FBattleInputContext BuildContext() const;

	void SetTurnManager(UTurnManager* InTurnManager);
	void SetUIState(bool bIsUIOpen);
	void SetInteractionMode(EBattleInteractionMode Mode);
	void SetCameraInputEnabled(bool bEnabled);
	void SetHardLock(bool bLocked);
	
private:
	UPROPERTY()
	UTurnManager* TurnManager = nullptr;

	bool bUIOpen = false;
	bool bCameraEnabled = true;
	bool bHardLock = false;

	EBattleInteractionMode InteractionMode = EBattleInteractionMode::None;
};
