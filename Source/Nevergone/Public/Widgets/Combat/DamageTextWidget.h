// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageTextWidget.generated.h"

class UTextBlock;

UCLASS()
class NEVERGONE_API UDamageTextWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetDamage(float Damage);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DamageText;
};
