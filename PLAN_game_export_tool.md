# Game Export Tool - Implementation Plan

## Overview

This document outlines the design for a **Game Export Tool** that captures and exports all data from a single simulated game in a human-readable format for validation and debugging purposes.

---

## Goals

1. **Complete Data Capture** - Record every event, decision, and state change during a game
2. **Human Readable** - Output should be easy to read and understand without special tools
3. **Validation Support** - Enable manual verification that the simulator is working correctly
4. **Single Game Focus** - Optimized for detailed analysis of individual games (not batch processing)

---

## Proposed Output Format: JSON + Text Summary

The tool will produce **two output formats**:

### 1. Detailed JSON Export (`game_export.json`)
- Machine-parseable, complete data
- Can be loaded into tools for analysis
- Preserves all numeric precision

### 2. Human-Readable Text Report (`game_report.txt`)
- Narrative-style game log
- Easy to read in terminal or text editor
- Summarized combat results with key details

---

## Data Structure Design

### Root Export Object

```json
{
  "metadata": {
    "export_version": "1.0",
    "timestamp": "2026-01-05T12:00:00Z",
    "simulator_version": "x.x.x",
    "seed": 12345
  },
  "setup": { ... },
  "rounds": [ ... ],
  "result": { ... }
}
```

### Setup Section

Captures initial game state before any actions:

```json
{
  "setup": {
    "unit_a": {
      "name": "Space Marines",
      "faction": "Battle Brothers",
      "unit_id": 12345,
      "points_cost": 150,
      "ai_type": "Hybrid",
      "quality": 3,
      "defense": 4,
      "model_count": 5,
      "models": [
        {
          "name": "Sergeant",
          "quality": 3,
          "defense": 4,
          "tough": 1,
          "is_hero": true,
          "weapons": [
            {
              "name": "Plasma Pistol",
              "range": 12,
              "attacks": 1,
              "ap": 2,
              "rules": ["Deadly(3)"]
            },
            {
              "name": "Power Sword",
              "range": 0,
              "attacks": 3,
              "ap": 1,
              "rules": ["Rending"]
            }
          ],
          "rules": ["Hero", "Fearless"]
        }
        // ... more models
      ],
      "unit_rules": ["Relentless", "Fast"],
      "max_range": 24
    },
    "unit_b": { /* same structure */ },
    "initial_positions": {
      "unit_a": -12,
      "unit_b": 12,
      "distance_between": 24,
      "objective_position": 0
    }
  }
}
```

### Round Section

Captures everything that happens in each round:

```json
{
  "rounds": [
    {
      "round_number": 1,
      "state_at_start": {
        "unit_a": {
          "position": -12,
          "models_alive": 5,
          "total_wounds_taken": 0,
          "status": "Normal",
          "is_fatigued": false,
          "model_states": [
            {"model_index": 0, "wounds_taken": 0, "state": "Healthy"},
            {"model_index": 1, "wounds_taken": 0, "state": "Healthy"}
            // ...
          ]
        },
        "unit_b": { /* same structure */ },
        "in_melee": false,
        "objective_holder": "None"
      },
      "activation_order": ["unit_a", "unit_b"],
      "activations": [
        {
          "unit": "unit_a",
          "action": {
            "type": "Charge",
            "reason": "AI decision: Enemy in charge range, melee advantageous",
            "movement": {
              "from": -12,
              "to": 0,
              "distance_moved": 12
            }
          },
          "combat": {
            "phase": "melee",
            "attacker": "unit_a",
            "defender": "unit_b",
            "attacks": [
              {
                "model_name": "Sergeant",
                "weapon_name": "Power Sword",
                "base_attacks": 3,
                "bonus_attacks": 1,
                "bonus_source": "Furious (charged)",
                "total_attacks": 4,
                "rolls": {
                  "to_hit": {
                    "target": 3,
                    "dice_rolled": [2, 4, 5, 6],
                    "natural_sixes": 1,
                    "hits_before_modifiers": 3,
                    "modifiers_applied": [],
                    "hits_after_modifiers": 3
                  },
                  "to_wound": {
                    "hits_allocated": 3,
                    "ap_value": 1,
                    "rending_applied": true,
                    "rending_ap_bonus": 4,
                    "defense_target": 6,
                    "defense_rolls": [3, 5, 6],
                    "saves_made": 1,
                    "wounds_dealt": 2
                  }
                },
                "special_rules_triggered": ["Rending"]
              }
              // ... more attacks from other models
            ],
            "wound_allocation": {
              "total_wounds_to_allocate": 5,
              "allocation_order": [
                {
                  "target_model": "Grunt #1",
                  "wounds_allocated": 1,
                  "regeneration_roll": null,
                  "tough_saves": 0,
                  "wounds_after_saves": 1,
                  "model_killed": true
                },
                {
                  "target_model": "Grunt #2",
                  "wounds_allocated": 2,
                  "regeneration_roll": [4, 6],
                  "regenerated": 1,
                  "tough_saves": 0,
                  "wounds_after_saves": 1,
                  "model_killed": true
                }
                // ...
              ],
              "total_wounds_dealt": 5,
              "total_wounds_regenerated": 1,
              "models_killed": 2,
              "overkill_wounds": 0
            },
            "combat_result_summary": {
              "wounds_dealt": 5,
              "models_killed": 2,
              "target_destroyed": false,
              "target_shaken": false,
              "target_routed": false
            }
          },
          "morale": {
            "check_required": true,
            "reason": "Unit at half strength",
            "test_type": "melee_morale",
            "quality_target": 4,
            "roll": 3,
            "fearless_reroll": {
              "triggered": true,
              "reroll": 5
            },
            "result": "Passed",
            "status_change": null
          },
          "state_after_activation": {
            "unit_a": { /* updated state */ },
            "unit_b": { /* updated state */ }
          }
        },
        {
          "unit": "unit_b",
          "action": { /* ... */ },
          "combat": { /* if any */ },
          "morale": { /* if any */ }
        }
      ],
      "end_of_round": {
        "objective_control": {
          "unit_a_distance_to_objective": 0,
          "unit_b_distance_to_objective": 0,
          "holder": "Contested"
        },
        "state_summary": {
          "unit_a_alive": 5,
          "unit_b_alive": 3,
          "in_melee": true
        }
      }
    }
    // ... rounds 2, 3, 4
  ]
}
```

### Result Section

Final game outcome:

```json
{
  "result": {
    "winner": "unit_a",
    "victory_type": "Objective Control",
    "rounds_played": 4,
    "ending_condition": "Max rounds reached",
    "final_state": {
      "unit_a": {
        "status": "Normal",
        "models_remaining": 4,
        "total_wounds_taken": 3
      },
      "unit_b": {
        "status": "Routed",
        "models_remaining": 0,
        "total_wounds_taken": 8
      }
    },
    "statistics": {
      "unit_a": {
        "total_wounds_dealt": 12,
        "total_models_killed": 5,
        "rounds_holding_objective": 2,
        "first_blood": true
      },
      "unit_b": {
        "total_wounds_dealt": 3,
        "total_models_killed": 1,
        "rounds_holding_objective": 0,
        "first_blood": false
      }
    }
  }
}
```

---

## Human-Readable Text Report Format

```
================================================================================
                         GAME EXPORT REPORT
================================================================================
Generated: 2026-01-05 12:00:00
Seed: 12345
================================================================================

UNIT SETUP
--------------------------------------------------------------------------------

UNIT A: Space Marines (Battle Brothers)
  Points: 150 | Quality: 3+ | Defense: 4+ | AI: Hybrid
  Models: 5
  Unit Rules: [Relentless] [Fast]

  [1] Sergeant (Hero)
      Q:3+ D:4+ Tough(1)
      Rules: [Hero] [Fearless]
      Weapons:
        - Plasma Pistol (12", A1, AP2) [Deadly(3)]
        - Power Sword (Melee, A3, AP1) [Rending]

  [2-5] Battle Brother x4
      Q:3+ D:4+
      Weapons:
        - Rifle (24", A1, AP0)
        - CCW (Melee, A1, AP0)

UNIT B: Ork Boyz (Green Tide)
  Points: 100 | Quality: 5+ | Defense: 6+ | AI: Melee
  Models: 10
  ...

INITIAL POSITIONS
  Unit A: 12" from center (own side)
  Unit B: 12" from center (own side)
  Distance between units: 24"

================================================================================
                              ROUND 1
================================================================================

--- Activation Order: Unit A first (random) ---

UNIT A ACTIVATION
--------------------------------------------------------------------------------
Position: 12" from center → 0" (moved 12")
Action: CHARGE
Reason: AI decision - Enemy in charge range, melee advantageous

  MELEE COMBAT vs Unit B
  ----------------------

  Sergeant attacks with Power Sword:
    Base attacks: 3, Bonus: +1 (Furious - charged) = 4 attacks
    To Hit (3+): rolled [2, 4, 5, 6] → 3 hits (1 natural six)
    Rending triggered! AP1 → AP5 for 1 hit
    Defense rolls:
      - Hit 1 (AP5): needs 6+ → rolled 3 → WOUND
      - Hit 2 (AP1): needs 5+ → rolled 5 → SAVED
      - Hit 3 (AP1): needs 5+ → rolled 6 → SAVED
    Wounds dealt: 1

  Battle Brother #1 attacks with CCW:
    ...

  TOTAL: 5 wounds dealt

  Wound Allocation:
    → Ork Boy #1: 1 wound allocated → KILLED
    → Ork Boy #2: 2 wounds allocated, Regeneration roll [4,6] → 1 regenerated
                  1 wound taken → KILLED
    → Ork Boy #3: 2 wounds allocated → KILLED

  Models killed: 3 | Wounds regenerated: 1 | Overkill: 0

  MORALE CHECK
  ------------
  Trigger: Unit at half strength (7/10 → below threshold after casualties)
  Wait - unit not at half strength yet (7 remaining of 10)
  No morale check required.

  Combat Summary: 5 wounds dealt, 3 models killed

UNIT B ACTIVATION
--------------------------------------------------------------------------------
Position: 12" → 0" (moved 12")
Action: CHARGE (already in melee - consolidate)
...

--- End of Round 1 ---
Objective: CONTESTED (both units at 0")
Unit A: 5/5 models | Normal
Unit B: 7/10 models | Normal

================================================================================
                              ROUND 2
================================================================================
...

================================================================================
                           GAME RESULT
================================================================================

WINNER: UNIT A (Space Marines)
Victory Type: Objective Control
Rounds Played: 4
Ending: Max rounds reached

Final State:
  Unit A: 4/5 models remaining, Normal
  Unit B: 0/10 models remaining, Destroyed

Statistics:
                        Unit A          Unit B
  Wounds Dealt:         12              3
  Models Killed:        10              1
  Obj Rounds:           2               0
  First Blood:          Yes             No

================================================================================
```

---

## Implementation Architecture

### New Files to Create

```
include/
  export/
    game_exporter.hpp       # Main exporter class
    export_types.hpp        # Export data structures
    text_formatter.hpp      # Human-readable formatting
    json_formatter.hpp      # JSON serialization

src/
  export/
    game_exporter.cpp
    text_formatter.cpp
    json_formatter.cpp

  export_game.cpp           # CLI entry point for single game export
```

### Core Classes

#### 1. GameEventLog
Accumulates events during game execution:

```cpp
class GameEventLog {
public:
    void log_game_start(const Unit& a, const Unit& b, i8 pos_a, i8 pos_b);
    void log_round_start(u8 round, const GameState& state);
    void log_activation_start(const Unit& unit, ActionType action, const char* reason);
    void log_movement(i8 from, i8 to);
    void log_attack(const AttackEventData& attack);
    void log_defense_roll(u8 target, u8 roll, bool saved);
    void log_wound_allocation(const WoundAllocationEvent& event);
    void log_morale_check(const MoraleCheckEvent& event);
    void log_round_end(const RoundEndData& data);
    void log_game_end(const GameResult& result);

    const ExportData& get_export_data() const;

private:
    ExportData data_;
    RoundData* current_round_ = nullptr;
    ActivationData* current_activation_ = nullptr;
};
```

#### 2. ExportGameRunner
Extended GameRunner that logs all events:

```cpp
class ExportGameRunner {
public:
    ExportGameRunner(DiceRoller& dice, GameEventLog& log);

    GameResult run_game(const Unit& a, const Unit& b);

private:
    GameRunner runner_;
    GameEventLog& log_;

    // Override/wrap key methods to add logging
    void log_before_combat(...);
    void log_after_combat(...);
    // etc.
};
```

#### 3. GameExporter
Orchestrates export process:

```cpp
class GameExporter {
public:
    struct Options {
        bool include_dice_rolls = true;
        bool include_model_details = true;
        bool verbose_ai_decisions = true;
        OutputFormat format = OutputFormat::Both;
    };

    static void export_to_json(const ExportData& data, std::ostream& out);
    static void export_to_text(const ExportData& data, std::ostream& out);
    static void export_to_files(const ExportData& data,
                                 const std::string& base_path,
                                 const Options& options);
};
```

### Integration Points

The export tool needs to hook into these existing functions:

| Location | What to Log |
|----------|-------------|
| `GameRunner::run_game()` | Game start/end |
| `GameRunner::run_round()` | Round transitions |
| `GameRunner::activate_unit()` | Action decisions |
| `AIController::decide_action()` | AI reasoning |
| `CombatEngine::resolve_shooting()` | Shooting attacks |
| `CombatEngine::resolve_melee()` | Melee attacks |
| `CombatEngine::roll_to_hit()` | Hit rolls |
| `CombatEngine::roll_defense()` | Defense rolls |
| `CombatEngine::apply_wounds()` | Wound allocation |
| `CombatEngine::check_morale()` | Morale tests |

### Approach Options

**Option A: Callback/Observer Pattern (Recommended)**
- Create a `GameObserver` interface
- Existing code calls observer methods at key points
- Minimal changes to existing code
- Easy to add/remove logging

**Option B: Wrapper Classes**
- Create wrapper classes around GameRunner, CombatEngine
- Intercept all calls and log before/after
- No changes to existing code
- More boilerplate

**Option C: Direct Integration**
- Add logging calls directly into existing code
- Simplest implementation
- Clutters existing code
- Harder to maintain

---

## CLI Interface

### New Executable: `export_game`

```bash
# Basic usage - exports both JSON and text
./export_game -a units/space_marines.txt -b units/ork_boyz.txt

# Specify output files
./export_game -a units/a.txt -b units/b.txt -o game_001

# JSON only
./export_game -a units/a.txt -b units/b.txt --json-only

# Text only
./export_game -a units/a.txt -b units/b.txt --text-only

# Set specific seed for reproducibility
./export_game -a units/a.txt -b units/b.txt --seed 12345

# Verbose mode (include AI decision details)
./export_game -a units/a.txt -b units/b.txt -v
```

### Output Files

```
game_001.json     # Complete JSON export
game_001.txt      # Human-readable report
```

---

## Implementation Steps

### Phase 1: Data Structures
1. Define `ExportData` and related structs in `export_types.hpp`
2. Define event data types for each loggable event
3. Implement `GameEventLog` class

### Phase 2: Event Logging Integration
1. Create `GameObserver` interface
2. Add observer hooks to `GameRunner`
3. Add observer hooks to `CombatEngine`
4. Add observer hooks to `AIController` (for decision logging)

### Phase 3: Formatters
1. Implement `JsonFormatter` using nlohmann/json or manual serialization
2. Implement `TextFormatter` with the narrative style shown above

### Phase 4: CLI Tool
1. Create `export_game.cpp` with argument parsing
2. Wire up unit loading, game execution, and export
3. Add seed control for reproducibility

### Phase 5: Testing & Validation
1. Run known scenarios and verify output matches expectations
2. Test edge cases (rout, destruction, draws, etc.)
3. Verify all special rules are captured when triggered

---

## Validation Use Cases

This tool will help validate:

1. **Combat Math** - Verify hit/wound calculations are correct
2. **Special Rules** - Confirm rules trigger at right times with right effects
3. **Morale System** - Check morale triggers and outcomes
4. **AI Decisions** - Understand why AI chose specific actions
5. **Wound Allocation** - Verify allocation order and regeneration
6. **Objective Control** - Check position/distance calculations
7. **Game Flow** - Ensure rounds progress correctly

---

## Future Enhancements (Out of Scope)

- Interactive replay viewer (web-based)
- Diff tool to compare two game exports
- Statistical analysis of exported games
- Import/replay from JSON export

---

## Questions for User

1. **JSON Library**: Use nlohmann/json (header-only, easy) or manual serialization (no dependencies)?

2. **Dice Roll Detail Level**:
   - Minimal: Just final results
   - Standard: Rolls grouped by attack
   - Verbose: Every individual die roll

3. **AI Decision Logging**: How much detail about AI reasoning?
   - Basic: Just the action chosen
   - Detailed: Factors considered and scores

4. **Output Location**:
   - Current directory?
   - Dedicated `exports/` folder?
   - User-specified?

5. **Seed Control**: Should there be an option to run the same game multiple times with different seeds to see variance?
