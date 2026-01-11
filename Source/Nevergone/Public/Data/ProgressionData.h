// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "ProgressionData.generated.h"

USTRUCT(BlueprintType)
struct FProgressionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MainQuestStage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSet<FName> CompletedSideQuests;
};
