# Nevergone — Localization Pipeline

This document describes the runtime architecture and the editor workflow for
adding/translating UI text in Nevergone.

---

## 1. Runtime architecture (already wired in C++)

| Layer | Responsibility | Class |
|---|---|---|
| Persistence | Stores the player's chosen language in `GameUserSettings.ini` | `UNevergoneGameUserSettings` |
| Execution | Applies the culture, exposes API, broadcasts changes | `ULocalizationSubsystem` (lives on `UGameInstance`) |
| Config | Lists supported cultures + fallback | `UMyGameInstance` (`SupportedCultures`, `FallbackCulture`) |

### Boot flow (first launch)
```
UMyGameInstance::Init()
 └─ Super::Init() → ULocalizationSubsystem::Initialize()
     ├─ Read SupportedCultures + FallbackCulture from UMyGameInstance
     ├─ DetermineInitialCulture():
     │   1. Persisted UNevergoneGameUserSettings::PreferredCulture (if set & supported)
     │   2. OS-detected culture (full code, e.g. "pt-BR")
     │   3. OS language prefix (e.g. "pt")
     │   4. UMyGameInstance::FallbackCulture
     │   5. SupportedCultures[0]
     └─ SetCurrentCulture(...) → applies + persists + broadcasts
```

### Runtime switch (player picks a new language)
```
[BP Settings panel] → ULocalizationSubsystem::SetCurrentCulture("pt-BR")
 ├─ FInternationalization::Get().SetCurrentCulture("pt-BR")
 │   └─ FTextLocalizationManager bumps revision counter
 │       └─ Every visible UTextBlock with LOCTEXT/NSLOCTEXT/Format text
 │          re-resolves on next paint — automatic
 ├─ UNevergoneGameUserSettings::SetPreferredCulture("pt-BR") → SaveSettings()
 └─ OnCultureChanged.Broadcast("pt-BR")
```

---

## 2. One-time engine config (REQUIRED — manual step)

Add the following to `Config/DefaultEngine.ini`:

```ini
[/Script/Engine.Engine]
GameUserSettingsClassName=/Script/Nevergone.NevergoneGameUserSettings
```

Without this, the engine instantiates the base `UGameUserSettings` and the
preferred-culture field is never persisted (the warning
`NevergoneGameUserSettings unavailable` will fire on every culture change).

Optional but recommended — list shipped cultures in `Config/DefaultGame.ini`:

```ini
[Internationalization]
+LocalizedCultures=en
+LocalizedCultures=pt-BR
NativeCulture=en
```

This is metadata used by packaging — it does not affect runtime selection,
which is driven by `UMyGameInstance::SupportedCultures`.

---

## 3. Editor workflow — adding/updating translations

The Localization Dashboard (`Window → Localization Dashboard` in the editor)
manages the gather → export → translate → import → compile cycle.

### Initial setup (do once)

1. Open the Localization Dashboard.
2. Create a target named `Game` (probably already exists).
3. **Gather Text** sources:
   - **From Packages** → `Content/` (for `FText` fields in `UDataAsset`s like
     `UItemDefinition`, `UQuestData`, `UDialogueData`, etc.).
   - **From Text Files** → `Source/Nevergone/` (for `LOCTEXT` / `NSLOCTEXT` in C++).
4. Add culture rows: `en` (native) and `pt-BR`.
5. Click **Gather Text** — populates the `Game.manifest` and `*.archive` files
   under `Content/Localization/Game/`.

### Iterating after code or asset changes

Whenever you add new `LOCTEXT` macros, new `FText` fields, or new DataAssets:

1. Open the Localization Dashboard.
2. Click **Gather Text** for the `Game` target.
3. For each non-native culture (`pt-BR`):
   - Click **Export Text** → produces `pt-BR.po` in
     `Content/Localization/Game/pt-BR/`.
   - Translate the `.po` file (in editor, in Poedit, or send to translators).
   - Click **Import Text** → reads the translated `.po` back in.
4. Click **Compile Text** → produces `Game.locres` per culture, which is the
   binary file actually loaded at runtime.

The compiled `.locres` files are packaged with the project automatically.

---

## 4. Author rules — keep these in mind when writing new code

### DO

* `LOCTEXT("UniqueKey", "Default English text")` for inline strings in C++
  (requires `LOCTEXT_NAMESPACE` defined at the top of the file and undefined at
  the bottom).
* `NSLOCTEXT("Namespace", "UniqueKey", "Default English text")` for one-off
  strings in headers or rare cases without a namespace.
* `FText::Format(LOCTEXT("Key", "Score: {0}"), Score)` for any string with
  variables — never concatenate localized strings.
* `StaticEnum<EMyEnum>()->GetDisplayValueAsText(Value)` to translate enum
  values (uses `UMETA(DisplayName = ...)` automatically).
* `FText` typed UPROPERTY fields in DataAssets — they are auto-gathered.

### DO NOT

* `FText::FromString(TEXT("..."))` for player-facing text — these have no
  namespace/key, are not gathered, and do **not** auto-refresh on culture change.
* Concatenate FText with `+` or `FString::Printf` — always use `FText::Format`.
* Pre-`.ToString()` an FText into an FString that flows back to the UI — once
  it's an FString, it's frozen in the current language.

---

## 5. Files added/changed in the localization pass

### New
- `Public/Localization/NevergoneGameUserSettings.h` / `.cpp`
- `Public/Localization/LocalizationSubsystem.h` / `.cpp`

### Modified
- `Public/GameInstance/MyGameInstance.h` — adds `SupportedCultures` + `FallbackCulture`.
- `Public/Data/StatusEffectDefinition.h` — `FString DisplayLabel/Description` → `FText`.
- `Public/Data/UnitDefinition.h` — `GetClassName` now uses `UEnum::GetDisplayValueAsText`.
- `Public/Characters/CharacterBase.h` + `.cpp` — `SpawnFloatingText` takes `FText`.
- `Public/Widgets/Combat/FloatingCombatTextWidget.h` + `.cpp` — text channel migrated to `FText`.
- `Public/GameMode/Combat/CombatEventBus.h` + `.cpp` — `NotifyStatusApplied` takes `FText`.
- `Private/Widgets/Combat/TurnIndicatorWidget.cpp` — `LOCTEXT` for "PLAYER TURN" / "ENEMY TURN".
- `Private/Widgets/Combat/BattleResultsWidget.cpp` — `LOCTEXT` + `FText::Format` for headers/counts.
- `Private/Widgets/Combat/BattlePreparationWidget.cpp` — same treatment.
- `Private/Widgets/Combat/UnitHPBarWidget.cpp` — `FText::Format` for "{Current} / {Max}".
- `Private/Widgets/Party/PartyMemberEntryWidget.cpp` — `LOCTEXT` for "Unknown".
- `Private/Actors/ContainerActor.cpp` — fallback uses `NSLOCTEXT`.

### Author note — re-saving DataAssets
Existing `UDataAsset`s with `FText` fields whose `FString` siblings were
migrated (`StatusEffectDefinition::DisplayLabel`, `Description`) need to be
re-opened in the editor and have their text re-typed once. Old serialized
FString data is dropped because the property type changed.
