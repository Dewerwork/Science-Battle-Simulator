# Battle Simulator - Complete Technical Documentation

This document provides a comprehensive explanation of the OPR Grimdark Future Battle Simulator, including its architecture, core mechanics, data structures, and everything needed to understand or recreate the system.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Technology Stack](#technology-stack)
4. [Core Data Structures](#core-data-structures)
5. [Combat Resolution Engine](#combat-resolution-engine)
6. [Dice System](#dice-system)
7. [Special Rules System](#special-rules-system)
8. [Scenario System](#scenario-system)
9. [Input Formats](#input-formats)
10. [API Reference](#api-reference)
11. [Frontend Components](#frontend-components)
12. [Simulation Flow](#simulation-flow)

---

## Overview

The Battle Simulator is a Monte Carlo combat simulation tool for **One Page Rules (OPR) Grimdark Future** tabletop wargame. It allows players to:

- Load army lists from Army Forge (JSON or TXT formats)
- Select units for head-to-head matchup analysis
- Run thousands of simulated combat iterations
- Analyze win rates, damage output, and rule impacts
- Visualize combat turn-by-turn with dice rolls

The simulator accurately implements OPR core rules including:
- Quality and Defense tests
- Armor Penetration (AP)
- 40+ special rules (Blast, Deadly, Furious, Regeneration, etc.)
- Morale system (Shaken, Routed)
- Multiple combat scenarios

---

## Architecture

```
Battle_Simulator/
├── backend/                 # FastAPI REST API
│   └── api/
│       ├── main.py         # Application entry point with CORS, rate limiting
│       ├── routes/         # API endpoints
│       │   ├── armies.py   # Army parsing endpoints
│       │   ├── simulation.py # Combat simulation endpoints
│       │   ├── rules.py    # Special rules query endpoints
│       │   └── factions.py # Faction rules endpoints
│       ├── schemas.py      # Pydantic request/response models
│       └── analysis.py     # Combat analysis engine
│
├── src/                    # Core simulation engine (Python)
│   ├── models/             # Data models
│   │   ├── unit.py         # Unit and Model classes
│   │   ├── weapon.py       # Weapon class
│   │   ├── special_rule.py # SpecialRule, RuleTrigger, RuleEffect
│   │   └── army_list.py    # ArmyList container
│   │
│   ├── engine/             # Combat resolution logic
│   │   ├── combat.py       # Core attack resolution
│   │   ├── dice.py         # NumPy-based dice rolling
│   │   ├── wound_allocation.py # OPR wound allocation rules
│   │   ├── morale.py       # Morale test system
│   │   ├── scenario.py     # Combat scenarios
│   │   └── multi_round.py  # Extended battles
│   │
│   ├── rules/              # Special rules system
│   │   ├── builtin_rules.py # Rule definitions
│   │   ├── rule_engine.py  # Rule registration/lookup
│   │   └── rule_parser.py  # Parse rule strings
│   │
│   └── parsers/            # Army list parsing
│       ├── army_forge.py   # JSON parser
│       └── army_forge_txt.py # TXT parser
│
├── frontend/               # React/TypeScript web UI
│   └── src/
│       ├── components/     # UI components
│       ├── services/api.ts # API client
│       └── types/index.ts  # TypeScript types
│
└── data/                   # Configuration files
    ├── builtin_rules.yaml  # Special rule definitions
    └── faction_rules.yaml  # Faction-specific rules
```

---

## Technology Stack

### Backend
| Component | Technology | Purpose |
|-----------|------------|---------|
| Web Framework | FastAPI | Async REST API |
| Data Validation | Pydantic | Request/response schemas |
| Dice Rolling | NumPy | Vectorized Monte Carlo simulation |
| Configuration | PyYAML | Rule definitions |
| Error Tracking | Sentry (optional) | Production monitoring |

### Frontend
| Component | Technology | Purpose |
|-----------|------------|---------|
| UI Framework | React 18 | Component-based UI |
| Language | TypeScript | Type safety |
| Build Tool | Vite | Fast development/builds |
| Styling | TailwindCSS | Utility-first CSS |
| Components | Radix UI | Accessible primitives |
| Icons | Lucide React | Icon library |
| Animation | Motion | Smooth transitions |

---

## Core Data Structures

### Unit

A unit is a group of one or more models that act together in combat.

```python
class Unit:
    name: str                           # Unit name (e.g., "Assault Squad")
    models: list[Model]                 # Individual models in unit
    special_rules: list[SpecialRule]    # Unit-wide special rules
    points_cost: int                    # Points value

    # Computed properties:
    model_count: int                    # Total models
    alive_count: int                    # Living models (cached)
    alive_models: list[Model]           # Living models list (cached)
    base_quality: int                   # Most common quality
    base_defense: int                   # Most common defense
    total_wounds: int                   # Sum of all tough values
```

### Model

A single model within a unit. Models have individual stats and can have different equipment.

```python
class Model:
    name: str                           # Model name
    quality: int                        # Quality value (2-6, roll this or higher to hit)
    defense: int                        # Defense value (2-6, roll this or higher to save)
    tough: int                          # Wounds required to kill (default: 1)
    wounds_taken: int                   # Current damage
    weapons: list[Weapon]               # Equipped weapons
    special_rules: list[SpecialRule]    # Model-specific rules
    is_hero: bool                       # Heroes take wounds last
    state: ModelState                   # HEALTHY, WOUNDED, or DEAD
```

### Weapon

A weapon used to attack enemies. Can be melee (range=0) or ranged.

```python
class Weapon:
    name: str                           # Weapon name (e.g., "Plasma Rifle")
    attacks: int                        # Number of attack dice (A value)
    range: int                          # Range in inches (0 = melee)
    ap: int                             # Armor Penetration value
    special_rules: list[SpecialRule]    # Weapon special rules

    # Properties:
    is_melee: bool                      # True if range == 0
    is_ranged: bool                     # True if range > 0
```

### SpecialRule

A rule that modifies combat mechanics. Has a trigger (when it activates) and effect (what it does).

```python
class SpecialRule:
    name: str                           # Rule name (e.g., "Blast", "Deadly")
    value: int | None                   # Parameterized value (e.g., 3 for Blast(3))
    trigger: RuleTrigger                # When rule activates
    effect: RuleEffect                  # What the rule does
    condition: str | None               # Optional condition (e.g., "roll == 6")
    description: str                    # Human-readable description
```

#### Rule Triggers
```python
class RuleTrigger(Enum):
    BEFORE_ATTACK = "before_attack"     # Before rolling to hit
    ON_HIT_ROLL = "on_hit_roll"         # After each hit roll
    AFTER_HIT_ROLLS = "after_hit_rolls" # After all hit rolls
    ON_DEFENSE_ROLL = "on_defense_roll" # When defender rolls
    ON_WOUND = "on_wound"               # When wound is assigned
    ON_CHARGE = "on_charge"             # When charging
    ON_BEING_CHARGED = "on_being_charged"
    ON_SHOOTING = "on_shooting"
    ON_MELEE = "on_melee"
    MODIFY_ATTACKS = "modify_attacks"
    MODIFY_AP = "modify_ap"
    MODIFY_QUALITY = "modify_quality"
    MODIFY_DEFENSE = "modify_defense"
    ALWAYS = "always"                   # Passive effect
```

#### Rule Effects
```python
class RuleEffect(Enum):
    # Hit modifiers
    MULTIPLY_HITS = "multiply_hits"     # Blast(X)
    EXTRA_HIT_ON_SIX = "extra_hit_on_six" # Furious, Surge

    # Wound modifiers
    MULTIPLY_WOUNDS = "multiply_wounds" # Deadly(X)
    BYPASS_REGENERATION = "bypass_regeneration" # Bane
    REROLL_DEFENSE_SIXES = "reroll_defense_sixes" # Poison

    # AP modifiers
    SET_AP = "set_ap"                   # Rending (AP(4) on 6s)
    ADD_AP = "add_ap"                   # Lance (+2 AP on charge)

    # Defense modifiers
    IGNORE_WOUND = "ignore_wound"       # Regeneration (5+)
    EXTRA_WOUNDS_TO_KILL = "extra_wounds_to_kill" # Tough(X)

    # Quality modifiers
    MODIFY_HIT_ROLL = "modify_hit_roll" # +/- to hit
    SET_QUALITY = "set_quality"         # Reliable (Quality 2+)

    # Others
    ROLL_FOR_HITS = "roll_for_hits"     # Impact(X)
    IGNORE_COVER = "ignore_cover"       # Indirect, Blast
    STRIKE_FIRST = "strike_first"       # Counter
```

---

## Combat Resolution Engine

### Overview

The combat engine (`src/engine/combat.py`) resolves attacks following OPR rules:

```
Attacks → Hit Rolls → Hit Modifiers → Defense Rolls → Wounds → Wound Allocation
```

### Step 1: Calculate Attacks

For each model in the attacking unit:
1. Get weapons for current phase (melee or shooting)
2. Get attack count from weapon's `attacks` stat

### Step 2: Roll to Hit (Quality Test)

```python
def roll_quality_test(attacks: int, quality: int, modifier: int = 0):
    target = max(2, min(6, quality - modifier))  # Clamp to 2-6
    rolls = roll_d6(attacks)
    hits = count(rolls >= target)
    return hits, rolls
```

**Modifiers:**
- **Reliable**: Forces Quality 2+ rolls
- **Precise**: +1 to hit rolls
- **Shaken**: -1 to hit rolls
- **Fatigued**: Only hits on unmodified 6s (melee only)

### Step 3: Apply Hit Modifiers

After hit rolls, apply these modifiers in order:

1. **Furious** (when charging): Each unmodified 6 deals +1 extra hit
2. **Blast(X)**: Multiply total hits by X (capped at defender's model count)

```python
def apply_hit_modifiers(hits, hit_rolls, weapon, unit, defender, is_charging):
    # Furious: Extra hits on 6s when charging
    if is_charging and unit.has_rule("Furious"):
        sixes = count_sixes(hit_rolls)
        hits += sixes

    # Blast: Multiply hits (capped at target unit size)
    blast = weapon.get_rule("Blast")
    if blast:
        multiplier = min(blast.value, defender.model_count)
        hits *= multiplier

    return hits
```

### Step 4: Roll Defense (Defense Test)

```python
def roll_defense_test(hits: int, defense: int, ap: int, modifier: int, reroll_sixes: bool):
    # AP increases the target number
    # Modifier: positive is good for defender (cover), negative is bad (shaken)
    effective_defense = max(2, min(6, defense + ap - modifier))

    rolls = roll_d6(hits)

    # Poison: Force reroll of 6s
    if reroll_sixes:
        sixes = (rolls == 6)
        rolls[sixes] = roll_d6(count(sixes))

    successes = count(rolls >= effective_defense)
    wounds = hits - successes  # Failed saves = wounds
    return wounds, rolls
```

**Modifiers:**
- **AP(X)**: Defender needs +X on defense rolls
- **Cover**: +1 to defense rolls
- **Shaken**: -1 to defense rolls
- **Poison**: Force reroll of unmodified defense 6s
- **Rending**: Hits on 6 have AP(4)
- **Lance**: +2 AP when charging

### Step 5: Apply Wound Modifiers

```python
def apply_wound_modifiers(wounds, weapon, defender):
    # Deadly(X): Multiply wounds against single model
    deadly = weapon.get_rule("Deadly")
    if deadly and wounds > 0:
        wounds *= deadly.value
    return wounds
```

### Step 6: Wound Allocation

Wounds are allocated following strict OPR rules:

```python
def get_wound_allocation_order(unit):
    """Get models in order for wound allocation."""
    alive = unit.alive_models

    # 1. Non-tough models first
    non_tough = [m for m in alive if m.tough == 1 and not m.is_hero]

    # 2. Tough models by wounds taken (most wounded first)
    tough_non_hero = [m for m in alive if m.tough > 1 and not m.is_hero]
    tough_non_hero.sort(key=lambda m: m.wounds_taken, reverse=True)

    # 3. Heroes last
    heroes = [m for m in alive if m.is_hero]
    heroes.sort(key=lambda m: m.wounds_taken, reverse=True)

    return non_tough + tough_non_hero + heroes
```

After allocation order is determined:
1. **Regeneration**: Roll 5+ to ignore each wound (unless Bane)
2. **Protected**: Roll 6+ to reduce incoming AP by 1
3. Apply wounds to models in order until all allocated

---

## Dice System

The dice system (`src/engine/dice.py`) uses NumPy for high-performance vectorized rolling:

```python
class DiceRoller:
    def __init__(self, seed: int | None = None):
        self.rng = np.random.default_rng(seed)

    def roll_d6(self, count: int) -> NDArray[np.int_]:
        """Roll multiple D6 dice efficiently."""
        return self.rng.integers(1, 7, size=count)

    def roll_d6_target(self, count: int, target: int) -> tuple[int, NDArray]:
        """Roll dice and count successes against target."""
        rolls = self.roll_d6(count)
        successes = int(np.sum(rolls >= target))
        return successes, rolls

    def count_sixes(self, rolls: NDArray) -> int:
        """Count natural 6s for special rules."""
        return int(np.sum(rolls == 6))

    def reroll_failures(self, rolls: NDArray, target: int) -> tuple[NDArray, int]:
        """Reroll failed dice and return additional successes."""
        failures = rolls < target
        rerolls = self.roll_d6(np.sum(failures))
        new_successes = np.sum(rerolls >= target)
        new_rolls = rolls.copy()
        new_rolls[failures] = rerolls
        return new_rolls, new_successes
```

### Why NumPy?

For Monte Carlo simulation with 1000+ iterations:
- **Vectorized operations**: Roll thousands of dice in single operation
- **Memory efficient**: Preallocated arrays
- **Thread-safe**: Each simulation gets new `DiceRoller` instance
- **Reproducible**: Optional seed for debugging

---

## Special Rules System

### Implemented Rules (40+)

#### Weapon Rules
| Rule | Effect | Description |
|------|--------|-------------|
| AP(X) | `modify_defense_roll` | Targets get -X to Defense rolls |
| Bane | `bypass_regeneration` | Ignores Regeneration, reroll defense 6s |
| Blast(X) | `multiply_hits` | Each hit multiplied by X (max target models) |
| Counter | `strike_first` | Strikes first when charged |
| Deadly(X) | `multiply_wounds` | Multiply wounds by X against single model |
| Impact(X) | `roll_for_hits` | Roll X dice on charge, 2+ = hit |
| Indirect | `ignore_cover` | Ignores cover and LoS |
| Lance | `add_ap` | +2 AP when charging |
| Limited | `once_per_game` | May only use once |
| Lock-On | `modify_hit_roll` | +1 to hit if didn't move |
| Poison | `reroll_defense_sixes` | Force reroll of defense 6s |
| Precise | `modify_hit_roll` | +1 to hit rolls |
| Reliable | `set_quality` | Attack at Quality 2+ |
| Rending | `set_ap` | Unmodified 6s to hit get AP(4) |
| Sniper | `pick_target` | Pick which model is hit |
| Surge | `extra_hit_on_six` | 6s to hit deal extra hit |
| Thrust | `add_ap` | +1 to hit and AP(+1) when charging |

#### Defense Rules
| Rule | Effect | Description |
|------|--------|-------------|
| Regeneration | `ignore_wound` | 5+ to ignore each wound |
| Tough(X) | `extra_wounds_to_kill` | Requires X wounds to kill |
| Protected | `ignore_wound` | 6+ to reduce incoming AP |
| Stealth | `stealth` | -1 to be hit from >9" |
| Shield Wall | `modify_defense_roll` | +1 Defense in melee |

#### Unit Rules
| Rule | Effect | Description |
|------|--------|-------------|
| Fearless | `none` | 4+ to pass failed morale |
| Furious | `extra_hit_on_six` | Extra hits on 6s when charging |
| Hero | `none` | Takes wounds last |
| Relentless | `extra_hit_on_six` | Extra hits on 6s when shooting >9" |
| Fear(X) | `fear_bonus` | Counts as +X wounds in melee |
| War Chant | `modify_hit_roll` | +1 to hit in melee on charge turn |

### Rule Definition Format (YAML)

```yaml
rules:
  - name: "Blast"
    trigger: "after_hit_rolls"
    effect: "multiply_hits"
    description: "Each hit multiplied by X, capped at target unit size"
    category: "weapon"

  - name: "Lance"
    trigger: "on_charge"
    effect: "add_ap"
    effect_value: 2
    description: "This weapon gets +2 AP when charging."
    category: "weapon"

  - name: "Reliable"
    trigger: "on_hit_roll"
    effect: "set_quality"
    effect_value: 2
    description: "Models attack at Quality 2+ with this weapon."
    category: "weapon"
```
---
## Scenario System
The simulator supports multiple combat scenarios that model different tactical situations:
### Available Scenarios
| Scenario | Description | Flow |
|----------|-------------|------|
| `shooting_only` | Attacker shoots once | A shoots B, B takes morale |
| `mutual_shooting` | Both units shoot | A shoots B, B shoots A |
| `charge` | Attacker charges into melee | A charges (strikes first), B strikes back |
| `receive_charge` | Defender charges attacker | B charges A (strikes first), A strikes back |
| `shoot_then_charge` | Attacker shoots, defender charges | A shoots B, B charges A, A strikes back |
| `approach_1_turn` | Close 1 turn then charge | B shoots A (1 turn), A charges B |
| `approach_2_turns` | Close 2 turns then charge | B shoots A (2 turns), A charges B |
| `full_engagement` | Complete battle | Mutual shooting, then A charges B |
| `fighting_retreat` | Kiting scenario | A shoots, B charges, repeat up to 5 rounds |

### Scenario Runner

```python
class ScenarioRunner:
    def run_scenario(
        self,
        scenario_type: ScenarioType,
        attacker: Unit,
        defender: Unit,
        defender_in_cover: bool = False,
    ) -> ScenarioResult:
        # Create fresh copies of units
        attacker_copy = attacker.copy_fresh()
        defender_copy = defender.copy_fresh()
        # Run appropriate scenario
        match scenario_type:
            case ScenarioType.CHARGE:
                self._run_charge(attacker_copy, defender_copy, result)
            # ... other scenarios
        # Compute winner
        result.compute_outcome()
        return result
```

### Outcome Determination

```python
def compute_outcome(self):
    attacker_out = self.attacker_models_end == 0 or self.attacker_routed
    defender_out = self.defender_models_end == 0 or self.defender_routed
    if attacker_out and defender_out:
        self.draw = True
    elif defender_out:
        self.attacker_won = True
    elif attacker_out:
        self.defender_won = True
    else:
        # Compare remaining percentage
        attacker_pct = self.attacker_models_end / self.attacker_models_start
        defender_pct = self.defender_models_end / self.defender_models_start
        if attacker_pct > defender_pct:
            self.attacker_won = True
        elif defender_pct > attacker_pct:
            self.defender_won = True
        else:
            self.draw = True
```

---

## Input Formats

### Army Forge JSON Format

The simulator accepts exports from [Army Forge](https://army-forge.onepagerules.com/). Two formats are supported:

#### Full Export (embedded data)
```json
{
  "name": "My Battle Brothers",
  "gameSystem": "gf",
  "armyFaction": "Battle Brothers",
  "listPoints": 1000,
  "units": [
    {
      "id": "abc123",
      "customName": "Assault Squad Alpha",
      "size": 5,
      "quality": 3,
      "defense": 4,
      "cost": 150,
      "specialRules": [
        {"name": "Furious"},
        {"name": "Tough", "rating": "3"}
      ],
      "equipment": [
        {
          "name": "Plasma Rifle",
          "attacks": 2,
          "range": 24,
          "ap": 2,
          "specialRules": ["Deadly(3)"]
        },
        {
          "name": "CCW",
          "attacks": 1,
          "range": 0,
          "ap": 0
        }
      ]
    }
  ]
}
```

#### Reference Export (army book lookup)
```json
{
  "armyId": "battle_brothers",
  "armyName": "Battle Brothers",
  "list": {
    "name": "Strike Force",
    "units": [
      {
        "id": "unit-def-id-from-army-book",
        "customName": "My Custom Name",
        "selectedUpgrades": [
          {"upgradeId": "upgrade-1", "optionId": "option-a"}
        ]
      }
    ]
  }
}
```

### Army Forge TXT Format

Plain text format exported from Army Forge:

```
++ Battle Brothers [1000pts] ++
Assault Squad [150pts]: 5x Battle Brother, CCW, Plasma Rifle
- Tough(3), Furious
Veterans [200pts]: 5x Veteran, Storm Rifle (A2, AP(1)), Power Sword (A3, AP(2))
- Hero, Fearless
```

---

## API Reference

### Endpoints

#### Parse Army
```
POST /api/armies/parse
Content-Type: application/json

Body: {
  "content": "<JSON or TXT content>",
  "format": "json" | "txt"
}

Response: ArmyResponse {
  name: string,
  faction: string,
  points: number,
  points_limit: number,
  units: UnitResponse[]
}
```
#### Run Simulation
```
POST /api/simulation/run
Content-Type: application/json

Body: {
  "army_a": ArmyResponse,
  "army_b": ArmyResponse,
  "selected_unit_ids_a": string[],
  "selected_unit_ids_b": string[],
  "iterations": number (default: 1000, max: 2000),
  "scenarios": string[] (optional, default: all)
}

Response: SimulationResponse {
  matchups: MatchupResponse[],
  timestamp: string
}
```
#### Enhanced Simulation (with combat factors)
```
POST /api/simulation/run-enhanced

Response: EnhancedSimulationResponse {
  simulation_id: string,
  matchups: EnhancedMatchupResponse[],
  row_aggregates: RowAggregateResponse[],
  best_counters: { [defender_id]: BestCounterResponse },
  timestamp: string
}
```
#### Combat Visualization
```
POST /api/simulation/visualize

Body: {
  "attacker_unit": UnitResponse,
  "defender_unit": UnitResponse,
  "scenario": string
}

Response: DetailedVisualizationResponse {
  events: DetailedCombatEventResponse[],
  winner: string,
  total_rounds: number
}
```
#### Get Special Rules
```
GET /api/rules
GET /api/rules?category=weapon
GET /api/rules/search?q=blast

Response: RulesResponse {
  rules: SpecialRuleResponse[],
  categories: string[]
}
```
#### Get Faction Rules
```
GET /api/factions
GET /api/factions/{faction_id}
GET /api/factions/search?q=hive

Response: FactionRulesResponse {
  faction_id: string,
  name: string,
  army_wide_rules: FactionRuleResponse[],
  special_rules: FactionRuleResponse[],
  aura_rules: FactionRuleResponse[]
}
```
---
## Frontend Components
### Component Hierarchy
```
App
├── ArmyPanel (x2: left and right)
│   ├── Unit list with checkboxes
│   ├── Unit details button
│   └── UnitDetailsDialog
│
├── MatchupMatrixView
│   ├── Win rate matrix grid
│   ├── Scenario filter dropdown
│   └── Color-coded cells (green=win, red=loss)
│
├── DetailedResultsView
│   ├── Selected matchup stats
│   ├── Scenario breakdown
│   └── WinRateAnalysisDialog (AI explanations)
│
├── CombatVisualization
│   ├── Turn-by-turn events
│   ├── Dice roll display
│   └── Model count tracker
│
├── SpecialRulesDialog
│   ├── Category filter
│   ├── Search box
│   └── Rule list with descriptions
│
├── ScenarioSelectionDialog
│   ├── Scenario checkboxes
│   └── Iterations slider
│
└── LearnDialog
    └── Tutorial content
```
### State Management
The frontend uses React hooks for state:
```typescript
// Main state in App.tsx
const [armyA, setArmyA] = useState<Army | null>(null);
const [armyB, setArmyB] = useState<Army | null>(null);
const [selectedUnitsA, setSelectedUnitsA] = useState<string[]>([]);
const [selectedUnitsB, setSelectedUnitsB] = useState<string[]>([]);
const [simulationResults, setSimulationResults] = useState<SimulationResponse | null>(null);
const [enhancedResults, setEnhancedResults] = useState<EnhancedSimulationResponse | null>(null);
const [selectedMatchup, setSelectedMatchup] = useState<string | null>(null);
```

---

## Simulation Flow

### Complete Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        USER INTERFACE                                │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 1. LOAD ARMIES                                                       │
│    - User uploads JSON/TXT files                                     │
│    - Parser converts to internal Unit/Model/Weapon objects           │
│    - Special rules are matched to rule definitions                   │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 2. SELECT UNITS                                                      │
│    - User selects which units to include in simulation               │
│    - User configures scenarios and iterations                        │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 3. RUN SIMULATION                                                    │
│    For each matchup (Unit A vs Unit B):                              │
│      For each scenario (charge, shooting, etc.):                     │
│        For each iteration (1000x):                                   │
│          - Create fresh copies of units                              │
│          - Run scenario steps (shoot, charge, strike back)           │
│          - Track wounds dealt, models killed                         │
│          - Check morale (shaken/routed)                              │
│          - Determine winner                                          │
│        - Aggregate results (win rate, avg kills, combat factors)     │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│ 4. DISPLAY RESULTS                                                   │
│    - Show matchup matrix with win rates                              │
│    - Display detailed stats for selected matchup                     │
│    - Explain key factors affecting outcome                           │
│    - Visualize individual combat turn-by-turn                        │
└─────────────────────────────────────────────────────────────────────┘
```

### Monte Carlo Process

```python
def run_simulation(attacker: Unit, defender: Unit, iterations: int = 1000):
    wins = 0
    losses = 0
    draws = 0
    total_kills = 0
    total_wounds = 0

    for _ in range(iterations):
        # Run single combat
        result = run_scenario(CHARGE, attacker, defender)

        # Track outcome
        if result.attacker_won:
            wins += 1
        elif result.defender_won:
            losses += 1
        else:
            draws += 1

        # Track damage
        total_kills += result.total_models_killed_by_attacker
        total_wounds += result.total_wounds_dealt_by_attacker

    return {
        "win_rate": wins / iterations * 100,
        "loss_rate": losses / iterations * 100,
        "draw_rate": draws / iterations * 100,
        "avg_kills": total_kills / iterations,
        "avg_wounds": total_wounds / iterations,
    }
```

---

## Appendix: Key File Locations

| Purpose | File Path |
|---------|-----------|
| Combat resolution | `src/engine/combat.py` |
| Dice rolling | `src/engine/dice.py` |
| Wound allocation | `src/engine/wound_allocation.py` |
| Morale system | `src/engine/morale.py` |
| Scenario runner | `src/engine/scenario.py` |
| Unit model | `src/models/unit.py` |
| Weapon model | `src/models/weapon.py` |
| Special rules model | `src/models/special_rule.py` |
| Rule definitions | `data/builtin_rules.yaml` |
| Faction rules | `data/faction_rules.yaml` |
| Army parser (JSON) | `src/parsers/army_forge.py` |
| API schemas | `backend/api/schemas.py` |
| API routes | `backend/api/routes/` |
| Frontend types | `frontend/src/types/index.ts` |
| Main React app | `frontend/src/App.tsx` |
| API client | `frontend/src/services/api.ts` |

---

## Version History

- **Current**: Full Monte Carlo simulation with 40+ special rules, multiple scenarios, combat visualization
- **Game System**: One Page Rules - Grimdark Future (v3.0 compatible)
