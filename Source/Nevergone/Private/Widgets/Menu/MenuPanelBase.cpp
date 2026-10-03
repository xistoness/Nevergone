// Copyright Xyzto Works

#include "Widgets/Menu/MenuPanelBase.h"

#include "Audio/AudioSubsystem.h"

class UAudioSubsystem;

void UMenuPanelBase::Activate()
{
	// C++ first, BP after — same order as engine convention (NativeConstruct / Construct).
	NativeOnPanelActivated();
	OnPanelActivated();
}

void UMenuPanelBase::Deactivate()
{
	NativeOnPanelDeactivated();
	OnPanelDeactivated();
}

void UMenuPanelBase::ConfirmPanel()
{
	OnPanelConfirmed.Broadcast();
}

void UMenuPanelBase::RequestNavigation(FName PanelId)
{
	OnPanelNavigationRequested.Broadcast(PanelId);
}
