// Copyright Xyzto Works

#include "Widgets/Menu/MenuPanelBase.h"

void UMenuPanelBase::ConfirmPanel()
{
	OnPanelConfirmed.Broadcast();
}

void UMenuPanelBase::RequestNavigation(FName PanelId)
{
	OnPanelNavigationRequested.Broadcast(PanelId);
}