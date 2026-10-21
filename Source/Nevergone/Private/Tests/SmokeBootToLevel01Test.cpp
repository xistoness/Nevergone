// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Menu/MainMenuWidget.h"
#include "Widgets/Menu/MenuPanelBase.h"

#include "Tests/SmokeSaveSlot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NevergoneSmokeBoot
{
	// The automation framework only fails a test for Errors logged while it runs,
	// and the boot (engine startup, main menu load) happens before any test starts.
	// This device is created when the module loads, so it also sees the boot.
	// The controller's own relay of the run's log lines is not a game error and is
	// skipped. The rest of the LogAutomation* family is not: an Error there counts.
	class FErrorCapture : public FOutputDevice
	{
	public:
		FErrorCapture()
		{
			if (GLog)
			{
				GLog->AddOutputDevice(this);
			}
		}

		virtual ~FErrorCapture() override
		{
			if (GLog)
			{
				GLog->RemoveOutputDevice(this);
			}
		}

		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Verbosity > ELogVerbosity::Error || Category == TEXT("LogAutomationController"))
			{
				return;
			}

			FScopeLock Lock(&Mutex);
			Lines.Add(FString::Printf(TEXT("%s: %s"), *Category.ToString(), Message));
		}

		TArray<FString> GetLines() const
		{
			FScopeLock Lock(&Mutex);
			return Lines;
		}

	private:
		mutable FCriticalSection Mutex;
		TArray<FString> Lines;
	};

	FErrorCapture GErrorCapture;

	void ReportError(const FString& Message)
	{
		if (FAutomationTestBase* Test = FAutomationTestFramework::Get().GetCurrentTest())
		{
			Test->AddError(Message);
		}
	}

	UMainMenuWidget* FindMainMenu()
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		for (TObjectIterator<UMainMenuWidget> It; It; ++It)
		{
			if (!It->IsTemplate() && It->GetWorld() == World && It->IsInViewport())
			{
				return *It;
			}
		}
		return nullptr;
	}

	// Waits until the main menu widget is on screen.
	class FWaitForMainMenuCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			if (FindMainMenu())
			{
				return true;
			}
			if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
			{
				ReportError(TEXT("Main menu widget never appeared in the viewport."));
				return true;
			}
			return false;
		}

	private:
		static constexpr double TimeoutSeconds = 30.0;
		double StartTime = FPlatformTime::Seconds();
	};

	// Confirms the New Game panel, the same call the menu button makes.
	class FConfirmNewGameCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			UMainMenuWidget* Menu = FindMainMenu();
			UMenuPanelBase* Panel = Menu && Menu->WidgetTree
				? Cast<UMenuPanelBase>(Menu->WidgetTree->FindWidget(TEXT("Panel_NewGame")))
				: nullptr;
			if (!Panel)
			{
				ReportError(TEXT("Panel_NewGame not found in the main menu widget."));
				return true;
			}

			Panel->ConfirmPanel();
			return true;
		}
	};

	// Waits until the given map is the running game world.
	class FWaitForMapCommand : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForMapCommand(const FString& InMapShortName) : MapShortName(InMapShortName) {}

		virtual bool Update() override
		{
			const UWorld* World = AutomationCommon::GetAnyGameWorld();
			if (World && World->GetName() == MapShortName)
			{
				return true;
			}
			if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
			{
				ReportError(FString::Printf(TEXT("Did not reach %s (current world: %s)."),
					*MapShortName, World ? *World->GetName() : TEXT("none")));
				return true;
			}
			return false;
		}

	private:
		static constexpr double TimeoutSeconds = 90.0;
		FString MapShortName;
		double StartTime = FPlatformTime::Seconds();
	};

	// Fails the test with every Error captured since the module loaded.
	class FAssertNoBootErrorsCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			for (const FString& Line : GErrorCapture.GetLines())
			{
				ReportError(FString::Printf(TEXT("Error logged since boot: %s"), *Line));
			}
			return true;
		}
	};
}

// Boot -> main menu -> New Game -> Level_01. New Game saves to the test slot
// (see SmokeSaveSlot.h), which is deleted at the end, pass or fail.
// Meant to run with -game: in the editor the maps would load without playing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneSmokeBootToLevel01Test, "Nevergone.Smoke.BootToLevel01",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneSmokeBootToLevel01Test::RunTest(const FString& Parameters)
{
	static const TCHAR* MainMenuMap = TEXT("/Game/Blueprints/Level/MainMenu/Level_MainMenu");

	if (!AutomationOpenMap(MainMenuMap))
	{
		AddError(FString::Printf(TEXT("Could not open %s"), MainMenuMap));
		return false;
	}

	using namespace NevergoneSmokeBoot;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMainMenuCommand());
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeSave::FUseTestSlotCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FConfirmNewGameCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapCommand(TEXT("Level_01")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FAssertNoBootErrorsCommand());
	ADD_LATENT_AUTOMATION_COMMAND(NevergoneSmokeSave::FCleanTestSlotCommand());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
