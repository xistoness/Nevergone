// Copyright Xyzto Works

#pragma once

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Engine/World.h"
#include "GameInstance/MyGameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

// The smoke tests never write to a player slot. The slot name is injected into the
// game instance by the tests only, and the file is deleted when the test ends.
namespace NevergoneSmokeSave
{
	// Outside the numbered campaign slots ("Slot_N"), so the slot list and the
	// campaign limit never see it.
	static const TCHAR* const SlotName = TEXT("Automation_Smoke");

	inline UMyGameInstance* FindGameInstance()
	{
		const UWorld* World = AutomationCommon::GetAnyGameWorld();
		return World ? Cast<UMyGameInstance>(World->GetGameInstance()) : nullptr;
	}

	// First command of a smoke test: routes New Game to the test slot and removes
	// a leftover file from an earlier run that was killed.
	class FUseTestSlotCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			UMyGameInstance::DeleteAutomationSlot(SlotName);

			UMyGameInstance* GI = FindGameInstance();
			if (!GI)
			{
				// Without the injected name, New Game would write to a player slot.
				if (FAutomationTestBase* Test = FAutomationTestFramework::Get().GetCurrentTest())
				{
					Test->AddError(TEXT("No game instance to route the test save slot; New Game would use a player slot."));
				}
				return true;
			}

			GI->SetAutomationSlotName(SlotName);
			return true;
		}
	};

	// Last command of a smoke test.
	class FCleanTestSlotCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			if (UMyGameInstance* GI = FindGameInstance())
			{
				GI->EndAutomationSession();
			}
			UMyGameInstance::DeleteAutomationSlot(SlotName);
			return true;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
