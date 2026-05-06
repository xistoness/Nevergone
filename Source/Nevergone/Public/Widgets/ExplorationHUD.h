// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ExplorationHUD.generated.h"

class UInventoryWidget;
/**
 * 
 */
UCLASS()
class NEVERGONE_API AExplorationHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	UInventoryWidget* GetInventoryWidget() const { return InventoryWidget; };
	
private:
	
	UPROPERTY()
	UInventoryWidget* InventoryWidget;
};
