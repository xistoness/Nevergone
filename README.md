[README.md](https://github.com/user-attachments/files/26572950/README.md)
# Nevergone

A turn-based tactical RPG built from scratch in **Unreal Engine 5 (C++)** — solo project by [Cristopher / Xyzto Works](https://github.com/xistoness).

Grid-based combat, exploration, and tower-floor progression. Every system described here was designed, architected, and implemented entirely in C++, with no third-party gameplay plugins beyond Unreal's own GAS.

> **Status:** Early vertical slice (Pass 1) — core combat loop functional, animations and audio pass in progress.

---

## Table of contents

- [Architecture overview](#architecture-overview)
- [Core systems](#core-systems)
  - [Grid generation](#1-grid-generation)
  - [A* pathfinding](#2-a-pathfinding)
  - [Utility AI](#3-utility-ai--scoring-system)
  - [Combat event bus](#4-combat-event-bus)
  - [GAS + data-driven abilities](#5-gas--data-driven-abilities)
  - [Game context manager](#6-game-context-manager)
- [Project structure](#project-structure)
- [Tech stack](#tech-stack)

---

## Architecture overview

```
UGameInstance
└── UGameContextManager          (subsystem — owns game flow state machine)
    ├── UCombatManager           (owns a single battle session)
    │   ├── UBattleState         (authoritative HP / status snapshot)
    │   ├── UCombatEventBus      (observer bus — abilities → HUD / audio / VFX)
    │   ├── UTurnManager         (turn order, AP, cooldown tick)
    │   ├── UBattleTeamAIPlanner (utility scoring AI for enemy team)
    │   └── UStatusEffectManager
    └── UPartyManagerSubsystem   (party persistence across floors)

UWorldSubsystem
└── UGridManager                 (runtime grid + A* pathfinding)
```

The design separates **session lifetime** (`UGameInstanceSubsystem`) from **world lifetime** (`UWorldSubsystem`). `UGridManager` lives only while the battle level is loaded; `UGameContextManager` and party data survive level transitions.

---

## Core systems

### 1. Grid generation

**Files:** `GridManager.h / .cpp`

The tactical grid is **not hand-placed**. On battle start, `GenerateGrid()` fires a downward line trace for every tile position across the encounter volume's bounding box. Tiles that hit geometry are walkable and snap to the actual surface height; tiles that miss are blocked.

```cpp
// GridManager.cpp — runtime grid from level geometry
const FVector TraceStart(WorldX, WorldY, Origin.Z + GridHeight);
const FVector TraceEnd  (WorldX, WorldY, Origin.Z - GridHeight);

const bool bHit = World->LineTraceSingleByChannel(
    Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

if (bHit)
{
    Tile.Height   = Hit.Location.Z;   // snaps to actual surface
    Tile.bBlocked = false;
}
else
{
    Tile.bBlocked = true;             // no geometry → impassable
}
```

After generation, `BuildNeighborCache()` pre-computes valid adjacency (8-directional, with diagonal corner-cutting rules) so pathfinding queries never recompute it at runtime.

---

### 2. A\* pathfinding

**Files:** `GridManager.h / .cpp` — `FindPath()`, `CalculatePathCost()`, `IsTraversalAllowed()`

Unreal's NavMesh is not used. A* is implemented from scratch on top of the tile grid.

**Features:**
- 8-directional movement with diagonal corner-cutting prevention
- Per-tile move cost support
- Height-aware traversal via `FGridTraversalParams` (max step-up / step-down per unit)
- Chebyshev distance heuristic (correct for 8-directional grids)
- Separate `CalculatePathCost()` for range checks (AI scoring) without reconstructing the full path

```cpp
// Traversal respects terrain height per unit stats
bool UGridManager::IsTraversalAllowed(
    const FGridTile& From, const FGridTile& To,
    const FGridTraversalParams& Params) const
{
    if (To.bBlocked) return false;

    const float DeltaZ = To.Height - From.Height;
    if ( DeltaZ >  Params.MaxStepUpHeight)   return false;
    if (-DeltaZ >  Params.MaxStepDownHeight) return false;

    return true;
}
```

> **Known tradeoff:** The open set uses `TSet` with linear search — O(n) per iteration. A priority queue (binary heap) would be the production optimization for large grids. The current grid sizes make this imperceptible in practice.

---

### 3. Utility AI — scoring system

**Files:** `BattleTeamAIPlanner`, `BattleAICandidateScorer`, `BattleAIScoringRule`, `BattleAIComponent`, `ScoringRules/`

The enemy AI uses a **utility scoring** architecture — the same pattern used in commercial tactics games. Each enemy has a `UBattleAIBehaviorProfile` (a data asset) containing a list of `UBattleAIScoringRule` objects with individual weights.

For every possible action (move to tile X, use ability Y on target Z), the system scores the action by summing all weighted rule scores and selects the highest.

```
BattleTeamAIPlanner
  └── per unit: BattleAIComponent.GatherCandidateActions()
        └── BattleAICandidateScorer.ScoreCandidate()
              └── foreach rule: rule.ComputeRawScore() * rule.Weight
  └── ChooseBestAction() → ExecuteAction()
```

**Adding a new behavior** requires only a new `UBattleAIScoringRule` subclass. Nothing else changes.

```cpp
// BattleAIScoringRule.h — the contract every rule fulfills
UCLASS(Abstract, Blueprintable, EditInlineNew)
class UBattleAIScoringRule : public UObject
{
    UPROPERTY(EditAnywhere)
    float Weight = 1.f;

    virtual float ComputeRawScore(
        const FTeamCandidateAction& Candidate,
        const FAICandidateEvalContext& Context) const PURE_VIRTUAL(...);
};
```

**Implemented scoring rules:**

| Rule | What it scores |
|---|---|
| `AISR_ExpectedDamage` | Normalizes expected damage output into a 0–1 score |
| `AISR_KillConfirm` | Rewards actions that secure a kill; penalizes overkill |
| `AISR_MoveTowardEnemy` | Rewards closing distance to the nearest enemy |
| `AISR_PreferCloserTargets` | Penalizes long-range attacks when closer targets exist |
| `AISR_KeepFormation` | Rewards staying near allied units |
| `AISR_TargetNearAlliedCluster` | Rewards attacking targets surrounded by allies |
| `AISR_PreferAbilityOverMove` | Weights ability use over passive repositioning |
| `AISR_APCostPenalty` | Penalizes actions that burn AP without proportional payoff |

**Inter-action damage reservation** is one of the more non-obvious features: before scoring a kill-confirm action, the AI reads how much damage other units have already *committed* to the same target this turn. This prevents two units from both choosing to finish the same low-HP target when only one is needed.

```cpp
// AISR_KillConfirm.cpp
const float Reserved   = QueryService->GetReservedDamageForTarget(Target, *TurnContext);
const float Remaining  = TargetState->CurrentHP - Reserved;

if (Remaining  <= 0.f)                      return AlreadyCoveredPenalty; // already dying
if (Candidate.ExpectedDamage >= Remaining)  return KillBonus;             // confirm kill
return 0.f;
```

---

### 4. Combat event bus

**Files:** `CombatEventBus.h / .cpp`

Abilities don't call the HUD, audio system, or floating text widgets directly. They call a single `Notify*` method on `UCombatEventBus`. All other systems subscribe via multicast delegates.

```cpp
// CombatEventBus.h
// Adding a new event type only requires:
//   1. A new delegate declaration.
//   2. A new Notify* method here.
//   3. The caller (ability / status system) to invoke it.
// No other existing code needs to change.

DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FOnDamageApplied,
    ACharacterBase* /*Source*/,
    ACharacterBase* /*Target*/,
    int32           /*Amount*/
);

void NotifyDamageApplied(ACharacterBase* Source, ACharacterBase* Target, int32 Amount);

FOnDamageApplied  OnDamageApplied;  // HUD, audio, VFX subscribe here
FOnUnitDied       OnUnitDied;
FOnStatusApplied  OnStatusApplied;
FOnStatusCleared  OnStatusCleared;
```

`NotifyDamageApplied` also handles shield absorption (via `UStatusEffectManager`) and triggers a brief emissive hit flash on the target mesh via a `FTimerHandle` — all centralized, none of it in the ability itself.

---

### 5. GAS + data-driven abilities

**Files:** `BattleGameplayAbility`, `GA_AoE`, `GA_Attack_Melee`, `GA_Attack_Ranged_LOS`, `GA_Move_Simple`, `UAbilityDefinition`

Abilities use Unreal's **Gameplay Ability System (GAS)**. The key architectural choice is the separation between:

- **GA class** — execution template (how the ability runs: targeting, activation, resolution)
- **`UAbilityDefinition` data asset** — all configuration (damage, range, cooldown, SFX, status effects)

Multiple abilities can share the same GA class with different definitions. Cooldowns are keyed per `UAbilityDefinition` asset, not per GA class, so two abilities sharing the same template have independent cooldown tracking.

**Targeting** uses a `CompositeTargetingPolicy` — a pure C++ (non-UCLASS) composite of `ITargetRule` interface implementations, owned by `TUniquePtr` for RAII lifetime:

```cpp
// CompositeTargetingPolicy.h — Rule of Composite Pattern, no UObject overhead
class CompositeTargetingPolicy
{
    void AddRule(TUniquePtr<ITargetRule> Rule) { Rules.Add(MoveTemp(Rule)); }

    bool IsValid(const FActionContext& Context) const
    {
        for (const TUniquePtr<ITargetRule>& Rule : Rules)
            if (!Rule->IsSatisfied(Context)) return false;
        return true;
    }

    TArray<TUniquePtr<ITargetRule>> Rules;
};
```

Implemented rules: `RangeRule`, `LineOfSightRule`, `FactionRule`, `TileNotBlockedRule`, `TileNotOccupiedRule`, `MoveRangeRule`.

---

### 6. Game context manager

**Files:** `GameContextManager.h / .cpp`

A `UGameInstanceSubsystem` that owns the game flow state machine: `None → MainMenu → Exploration → BattlePreparation → Combat → BattleResults → Exploration → ...`

All state transitions go through `CanEnterState()` guards. The `EnterState()` call that broadcasts `FOnGameContextChanged` is always the **last line** of any transition method — ensuring all context data is populated before any subscriber reacts.

`FBattleSessionData` stores both combat result fields and exploration restore fields (character class, transform, control rotation). A dedicated `ResetCombatResults()` method zeroes only the combat fields, preserving the exploration restore data needed for the return transition.

---

## Project structure

```
Source/Nevergone/
├── Private/
│   ├── Characters/
│   │   ├── Abilities/              # GA classes + targeting rules
│   │   └── PlayerControllers/      # one controller per game context
│   ├── GameMode/
│   │   ├── Combat/
│   │   │   ├── AI/                 # planner, scorer, executor, 8 scoring rules
│   │   │   └── Resolvers/          # ActionResolver
│   │   └── TurnManager.cpp
│   ├── GameInstance/               # GameContextManager, SaveGame
│   ├── Level/                      # GridManager, EncounterGeneratorSubsystem
│   ├── Party/                      # PartyManagerSubsystem
│   ├── Audio/                      # AudioSubsystem, AnimNotify_PlaySFX
│   └── Widgets/                    # UMG C++ widgets (HUD, hotbar, HP bars)
└── Public/                         # mirrors Private, all .h files
```

---

## Tech stack

| | |
|---|---|
| Engine | Unreal Engine 5 (5.7), C++ |
| Ability system | GAS (Gameplay Ability System) |
| Audio | UAudioComponent + MetaSound (native UE, no Wwise/FMOD) |
| Save system | `USaveGame` with slot management |
| AI | Custom utility scoring (no Behavior Trees, no EQS) |
| Pathfinding | Custom A* on runtime-generated tile grid |
| Version control | Git — [github.com/xistoness/Nevergone](https://github.com/xistoness/Nevergone) |
