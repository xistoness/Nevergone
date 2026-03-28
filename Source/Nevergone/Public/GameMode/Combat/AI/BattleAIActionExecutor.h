// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/AIBattleTypes.h"
#include "BattleAIActionExecutor.generated.h"

class ACharacterBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAIActionExecutionFinished, bool);

UCLASS()
class NEVERGONE_API UBattleAIActionExecutor : public UObject
{
	GENERATED_BODY()
	
public:
	void Initialize();
	bool ExecuteAction(const FTeamCandidateAction& Action);

	FOnAIActionExecutionFinished OnActionExecutionFinished;

private:
	void CleanupPendingBindings();
	UFUNCTION()
	void HandleActionFinished(ACharacterBase* ActingUnit);

private:
	UPROPERTY()
	ACharacterBase* ActiveUnit = nullptr;
};
