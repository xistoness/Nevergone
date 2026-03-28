// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/BattleTypes.h"
#include "BattleResultsContext.generated.h"

/**
 * 
 */
UCLASS()
class NEVERGONE_API UBattleResultsContext : public UObject
{
	GENERATED_BODY()
	
public:

	// Snapshot of the session data
	FBattleSessionData SessionSnapshot;

	void Initialize(const FBattleSessionData& InSession);

	void DumpToLog() const;
};
