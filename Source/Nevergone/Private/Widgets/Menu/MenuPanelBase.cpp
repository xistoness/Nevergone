// Copyright Xyzto Works

#include "Widgets/Menu/MenuPanelBase.h"

#include "Audio/AudioSubsystem.h"

class UAudioSubsystem;

void UMenuPanelBase::ConfirmPanel()
{
	OnPanelConfirmed.Broadcast();
}

void UMenuPanelBase::RequestNavigation(FName PanelId)
{
	OnPanelNavigationRequested.Broadcast(PanelId);
}