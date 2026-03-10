// Copyright Xyzto Works

#include "GameMode/Combat/BattleInputManager.h"

#include "ActorComponents/BattleModeComponent.h"
#include "Characters/CharacterBase.h"
#include "GameMode/CombatManager.h"
#include "GameMode/TurnManager.h"
#include "GameMode/Combat/BattleCameraPawn.h"
#include "Interfaces/BattleInputReceiver.h"

void UBattleInputManager::Initialize(UTurnManager* InTurnManager, ABattleCameraPawn* InCameraPawn, UCombatManager* InCombatManager)
{
	TurnManager = InTurnManager;
	BattleCameraPawn = InCameraPawn;
	CombatManager = InCombatManager;
	
	if (BattleCameraPawn &&
		BattleCameraPawn->GetClass()->ImplementsInterface(UBattleInputReceiver::StaticClass()))
	{
		DefaultInputReceiver.SetObject(BattleCameraPawn);
		DefaultInputReceiver.SetInterface(
			Cast<IBattleInputReceiver>(BattleCameraPawn)
		);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BattleInputManager] Initialize | DefaultReceiver=%s"),
		DefaultInputReceiver ? TEXT("Camera") : TEXT("None")
	);
}

void UBattleInputManager::SetInputContext(const FBattleInputContext& NewContext)
{
	InputContext = NewContext;
}

void UBattleInputManager::SetSelectedCharacter(ACharacterBase* NewSelection)
{
	if (!InputContext.CanAcceptOrders())
	{
		return;
	}

	SelectedCharacter = NewSelection;
	CacheUnitInput(NewSelection);
}

void UBattleInputManager::OnConfirmPressed()
{
	if (!InputContext.CanAcceptOrders())
	{
		return;
	}

	RouteConfirm();
}

void UBattleInputManager::OnCancelPressed()
{
	if (!InputContext.CanAcceptOrders())
	{
		return;
	}

	RouteCancel();
}

void UBattleInputManager::OnEndTurn()
{
	if (!InputContext.CanAcceptOrders() || !TurnManager)
	{
		return;
	}

	TurnManager->EndCurrentTurn();
}

void UBattleInputManager::OnHover(const FVector& WorldLocation)
{
	if (!InputContext.CanAcceptOrders())
	{
		return;
	}
	
	RouteHover(WorldLocation);
}

void UBattleInputManager::OnSelectNextUnit()
{
	if (!CanAcceptInput() || !CombatManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager] Can't select next unit"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager] Calls CM to select next..."));
	CombatManager->SelectNextControllableUnit();
}

void UBattleInputManager::OnSelectPreviousUnit()
{
	if (!CanAcceptInput() || !CombatManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager] Can't select previous unit"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager] Calls CM to select previous..."));
	CombatManager->SelectPreviousControllableUnit();
}

void UBattleInputManager::OnSelectNextAction()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager]: Routing select next action..."));
	IBattleInputReceiver* Receiver = ResolveReceiver();
	if (!CanAcceptInput() || !CombatManager || !Receiver)
	{
		return;
	}
	
	Receiver->Input_SelectNextAction();
}

void UBattleInputManager::OnSelectPreviousAction()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager]: Routing select previous action..."));
	IBattleInputReceiver* Receiver = ResolveReceiver();
	if (!CanAcceptInput() || !CombatManager || !Receiver)
	{
		return;
	}
	
	Receiver->Input_SelectPreviousAction();
}

void UBattleInputManager::OnCameraMove(const FVector2D& Input)
{
	if (!InputContext.bCameraInputEnabled) {return;}
		
	RouteCameraMove(Input);

}

void UBattleInputManager::OnCameraZoom(float Value)
{
	if (!InputContext.bCameraInputEnabled) {return;}
	RouteCameraZoom(Value);
}

void UBattleInputManager::OnCameraRotate(float Value)
{
	if (!InputContext.bCameraInputEnabled) {return;}
	RouteCameraRotate(Value);

}

void UBattleInputManager::OnActiveUnitChanged(ACharacterBase* NewActiveUnit)
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager] Selects new active unit!"));

	if (SelectedCharacter)
	{
		SelectedCharacter->SetSelected(false);
	}

	if (!NewActiveUnit)
	{
		SelectedCharacter = nullptr;
		SelectedCharacterInput = nullptr;
		return;
	}

	SelectedCharacter = NewActiveUnit;
	SelectedCharacter->SetSelected(true);
	CacheUnitInput(NewActiveUnit);

	if (UBattleModeComponent* BattleComp = NewActiveUnit->FindComponentByClass<UBattleModeComponent>())
	{
		BattleComp->SelectAbilityByIndex(0);
	}

	if (BattleCameraPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager] Battle camera pawn is valid"));
		BattleCameraPawn->FocusOnLocationSmooth(NewActiveUnit->GetActorLocation());
	}
}

void UBattleInputManager::RouteConfirm()
{
	UE_LOG(LogTemp, Warning, TEXT("[BattleInputManager]: Routing confirm..."));
	IBattleInputReceiver* Receiver = ResolveReceiver();
	if (!Receiver)
	{
		return;
	}
	
	Receiver->Input_Confirm();
}

void UBattleInputManager::RouteCancel()
{
	IBattleInputReceiver* Receiver = ResolveReceiver();
	if (!Receiver)
	{
		return;
	}

	Receiver->Input_Cancel();
}

void UBattleInputManager::RouteHover(const FVector& WorldLocation)
{
	IBattleInputReceiver* Receiver = ResolveReceiver();
	if (!Receiver)
	{
		return;
	}

	Receiver->Input_Hover(WorldLocation);
}

bool UBattleInputManager::CanAcceptInput() const
{
	return InputContext.CanAcceptOrders();
}

void UBattleInputManager::RouteCameraMove(const FVector2D& Input)
{
	if (BattleCameraPawn)
	{
		BattleCameraPawn->Input_CameraMove(Input);
	}
}

void UBattleInputManager::RouteCameraZoom(float Value)
{
	if (BattleCameraPawn)
	{
		BattleCameraPawn->Input_CameraZoom(Value);
	}
}

void UBattleInputManager::RouteCameraRotate(float Value)
{
	if (BattleCameraPawn)
	{
		BattleCameraPawn->Input_CameraRotate(Value);
	}
}

void UBattleInputManager::CacheUnitInput(ACharacterBase* Unit)
{
	SelectedCharacterInput = nullptr;

	if (!Unit)
	{
		return;
	}

	const TArray<UActorComponent*> Components =
		Unit->GetComponentsByInterface(UBattleInputReceiver::StaticClass());

	if (Components.Num() > 0)
	{
		SelectedCharacterInput.SetObject(Components[0]);
		SelectedCharacterInput.SetInterface(Cast<IBattleInputReceiver>(Components[0]));
	}
}

IBattleInputReceiver* UBattleInputManager::ResolveReceiver() const
{
	if (InputContext.InputFocus == EBattleInputFocus::Unit && SelectedCharacterInput)
	{
		return SelectedCharacterInput.GetInterface();
	}

	return nullptr;
}