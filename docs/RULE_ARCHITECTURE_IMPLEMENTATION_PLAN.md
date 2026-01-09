# Rule Registry Architecture Implementation Plan

## Overview

This document outlines the incremental migration from the current scattered rule handling to a centralized, registry-based architecture. The plan is designed to:

- Maintain backwards compatibility during migration
- Allow testing at each phase
- Minimize risk through incremental changes
- Prioritize highest-value improvements first

---

## Current State Problems

1. **79 hardcoded conditionals** scattered across `combat_engine.hpp`
2. **95% code duplication** between `resolve_shooting()` and `resolve_melee()`
3. **6+ locations to update** when adding a new rule
4. **No AI awareness** of most rules
5. **21 unimplemented rules** that silently fail
6. **Implicit execution order** with undocumented dependencies

---

## Target State Benefits

1. **Single location** for each rule definition
2. **Unified combat path** eliminating duplication
3. **Compile-time validation** catching missing implementations
4. **AI hints** integrated into rule definitions
5. **Explicit phase ordering** with clear documentation
6. **Easy extensibility** for new game phases

---

## Implementation Phases

### Phase 1: Foundation Data Structures

**Goal:** Create the core types without changing existing behavior.

**Files to Create:**
```
include/core/rule_definition.hpp    - RuleDefinition struct
include/core/phases.hpp             - Phase and SubPhase enums
include/core/traits.hpp             - Rule traits system
include/core/contexts.hpp           - Combat/Movement/etc contexts
```

**Tasks:**

1.1. Define Phase Enums
```cpp
// include/core/phases.hpp

enum class GamePhase : u8 {
    DEPLOYMENT,
    MOVEMENT,
    COMBAT,
    END_ROUND,
    PASSIVE
};

enum class CombatSubPhase : u8 {
    PRE_ATTACK,
    HIT_MODIFIERS,
    ROLL_HITS,
    HIT_SEPARATION,
    HIT_BONUSES,
    HIT_MULTIPLICATION,
    DEFENSE_RESOLUTION,
    WOUND_ALLOCATION
};

enum class MoveSubPhase : u8 {
    PRE_MOVE,
    CALCULATE_DISTANCE,
    EXECUTE_MOVE,
    CHARGE_DECLARE,
    CHARGE_RESOLVE,
    POST_MOVE
};

enum class DeploySubPhase : u8 {
    SETUP,
    SCOUT_MOVE,
    INFILTRATE,
    RESERVES
};

enum class EndRoundSubPhase : u8 {
    MORALE,
    REGENERATION,
    SCORING,
    CLEANUP
};
```

1.2. Define Trait System
```cpp
// include/core/traits.hpp

enum class RuleTrait : u32 {
    BYPASSES_REGENERATION  = 1 << 0,
    BYPASSES_RESISTANCE    = 1 << 1,
    FORCES_DEFENSE_REROLL  = 1 << 2,
    CHARGE_ONLY           = 1 << 3,
    MELEE_ONLY            = 1 << 4,
    RANGED_ONLY           = 1 << 5,
    REQUIRES_LOS          = 1 << 6,
    AURA_EFFECT           = 1 << 7,
};

using TraitMask = u32;
```

1.3. Define Context Structures
```cpp
// include/core/contexts.hpp

struct CombatContext {
    // Participants
    Unit& attacker;
    Unit& defender;
    Weapon& weapon;
    CombatType combat_type;
    u32 distance;
    bool is_charge;

    // Accumulated modifiers
    i32 hit_modifier = 0;
    std::optional<u8> quality_override;
    i32 defense_modifier = 0;
    i32 ap_modifier = 0;
    float wound_multiplier = 1.0f;
    u32 hit_multiplier = 1;

    // Hit tracking
    u32 base_hits = 0;
    u32 rending_hits = 0;
    u32 rupture_hits = 0;
    u32 bonus_hits = 0;
    u32 final_hits = 0;
    u32 natural_sixes = 0;

    // Wound tracking
    u32 wounds_to_allocate = 0;
    u32 wounds_allocated = 0;
    u32 models_killed = 0;

    // Aggregated traits
    bool bypasses_regeneration = false;
    bool bypasses_resistance = false;
    bool forces_reroll_defense = false;

    // Logging
    CombatLog* log = nullptr;
};

struct MovementContext {
    Unit& unit;
    Battlefield& battlefield;
    MoveType move_type;

    u32 base_distance;
    i32 distance_modifiers = 0;
    u32 final_distance;
    TerrainMask ignores_terrain = TerrainMask::NONE;
    bool ignores_engagement = false;
    bool can_advance_and_charge = false;
    i32 charge_bonus = 0;
};

struct DeploymentContext {
    Unit& unit;
    Player& player;
    Zone deployment_zone;
    Battlefield& battlefield;

    PositionSet allowed_positions;
    DeployTiming deploy_timing = DeployTiming::NORMAL;
    u32 forward_distance = 0;
};

struct EndRoundContext {
    u32 round_number;
    std::vector<Unit*> units;
    Battlefield& battlefield;

    std::unordered_map<Unit*, i32> morale_modifiers;
    std::unordered_map<Unit*, u32> heal_amounts;
    std::unordered_map<Player*, i32> score_modifiers;
};
```

1.4. Define Rule Definition Structure
```cpp
// include/core/rule_definition.hpp

enum class Target : u8 {
    ATTACKER,
    DEFENDER,
    WEAPON,
    SELF
};

enum class Trigger : u8 {
    ALWAYS,
    ON_CHARGE,
    ON_MODEL_DEATH,
    AFTER_COMBAT,
    FIRST_TURN,
    WHEN_WOUNDED
};

struct RuleDefinition {
    RuleId id;
    std::string_view name;
    std::vector<std::string_view> aliases;

    GamePhase game_phase;
    std::variant<
        CombatSubPhase,
        MoveSubPhase,
        DeploySubPhase,
        EndRoundSubPhase,
        std::monostate  // For PASSIVE
    > sub_phase;

    CombatType combat_type = CombatType::BOTH;
    Target target = Target::SELF;
    Trigger trigger = Trigger::ALWAYS;
    bool has_value = false;
    TraitMask traits = 0;

    // Effect function (type depends on game_phase)
    std::variant<
        std::function<void(CombatContext&, u8)>,
        std::function<void(MovementContext&, u8)>,
        std::function<void(DeploymentContext&, u8)>,
        std::function<void(EndRoundContext&, Unit&, u8)>,
        std::nullptr_t  // For PASSIVE/stat mods
    > effect;

    // Optional condition
    std::function<bool(const CombatContext&)> condition;

    std::string_view log_format;
    std::string_view description;
};
```

**Validation:**
- All new files compile
- No changes to existing behavior
- Unit tests for new types

---

### Phase 2: Rule Registry

**Goal:** Create the registry that stores and queries rule definitions.

**Files to Create:**
```
include/core/rule_registry.hpp      - Registry class
include/rules/combat_rules.hpp      - Combat rule definitions
include/rules/movement_rules.hpp    - Movement rule definitions
include/rules/deployment_rules.hpp  - Deployment rule definitions
include/rules/endround_rules.hpp    - End round rule definitions
```

**Tasks:**

2.1. Implement Rule Registry
```cpp
// include/core/rule_registry.hpp

class RuleRegistry {
public:
    static RuleRegistry& instance();

    // Registration
    void register_rule(const RuleDefinition& def);
    void register_rules(std::initializer_list<RuleDefinition> defs);

    // Queries
    const RuleDefinition* get_definition(RuleId id) const;
    std::vector<const RuleDefinition*> get_rules_for_phase(
        GamePhase phase,
        auto sub_phase
    ) const;

    // Collect applicable rules for a combat
    std::vector<ApplicableRule> collect_combat_rules(
        CombatSubPhase sub_phase,
        const Unit& attacker,
        const Unit& defender,
        const Weapon& weapon,
        CombatType combat_type
    ) const;

    // Collect applicable rules for movement
    std::vector<ApplicableRule> collect_movement_rules(
        MoveSubPhase sub_phase,
        const Unit& unit,
        MoveType move_type
    ) const;

    // Parser support
    std::optional<RuleId> parse_rule_name(std::string_view name) const;

    // Validation
    void validate() const;  // Called at startup

private:
    std::array<std::vector<RuleDefinition>,
               static_cast<size_t>(RuleId::COUNT)> definitions_;
    std::unordered_map<std::string_view, RuleId> alias_map_;
};

struct ApplicableRule {
    const RuleDefinition* definition;
    u8 value;  // Rule parameter value
    Entity* source;  // Who has this rule
};
```

2.2. Implement Compile-Time Validation
```cpp
// At startup or compile time
void RuleRegistry::validate() const {
    // Check every RuleId has at least one definition
    for (size_t i = 0; i < static_cast<size_t>(RuleId::COUNT); ++i) {
        if (definitions_[i].empty()) {
            throw std::runtime_error(
                "Missing definition for RuleId: " + std::to_string(i));
        }
    }

    // Check for duplicate aliases
    std::unordered_set<std::string_view> seen;
    for (const auto& [alias, id] : alias_map_) {
        if (seen.count(alias)) {
            throw std::runtime_error(
                "Duplicate alias: " + std::string(alias));
        }
        seen.insert(alias);
    }

    // Check all effects are non-null for non-PASSIVE rules
    for (const auto& defs : definitions_) {
        for (const auto& def : defs) {
            if (def.game_phase != GamePhase::PASSIVE &&
                std::holds_alternative<std::nullptr_t>(def.effect)) {
                throw std::runtime_error(
                    "Missing effect for rule: " + std::string(def.name));
            }
        }
    }
}
```

2.3. Create Initial Rule Definitions (subset)
```cpp
// include/rules/combat_rules.hpp

namespace rules {

// Start with a few rules to validate the pattern
inline const RuleDefinition Precise {
    .id = RuleId::Precise,
    .name = "Precise",
    .aliases = {"precise"},
    .game_phase = GamePhase::COMBAT,
    .sub_phase = CombatSubPhase::HIT_MODIFIERS,
    .combat_type = CombatType::BOTH,
    .target = Target::ATTACKER,
    .effect = [](CombatContext& ctx, u8 value) {
        ctx.hit_modifier += 1;
    },
    .log_format = "Precise: +1 to hit"
};

inline const RuleDefinition Stealth {
    .id = RuleId::Stealth,
    .name = "Stealth",
    .aliases = {"stealth"},
    .game_phase = GamePhase::COMBAT,
    .sub_phase = CombatSubPhase::HIT_MODIFIERS,
    .combat_type = CombatType::SHOOTING,
    .target = Target::DEFENDER,
    .condition = [](const CombatContext& ctx) {
        return ctx.distance > 9;
    },
    .effect = [](CombatContext& ctx, u8 value) {
        ctx.hit_modifier -= 1;
    },
    .log_format = "Stealth: -1 to hit (target beyond 9\")"
};

inline const RuleDefinition Rending {
    .id = RuleId::Rending,
    .name = "Rending",
    .aliases = {"rending"},
    .game_phase = GamePhase::COMBAT,
    .sub_phase = CombatSubPhase::HIT_SEPARATION,
    .combat_type = CombatType::BOTH,
    .target = Target::WEAPON,
    .traits = static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION),
    .effect = [](CombatContext& ctx, u8 value) {
        // Move natural 6s to rending_hits
        ctx.rending_hits = ctx.natural_sixes;
        ctx.base_hits -= ctx.natural_sixes;
    },
    .log_format = "Rending: {} hits separated (ignore defense)"
};

} // namespace rules
```

**Validation:**
- Registry compiles and initializes
- Parser uses registry for name lookup
- Definitions match existing behavior
- Startup validation passes

---

### Phase 3: Combat Migration (Hit Modifiers)

**Goal:** Migrate the first combat sub-phase to use registry, running in parallel with existing code.

**Files to Modify:**
```
include/engine/combat_engine.hpp    - Add new path, keep old path
```

**Tasks:**

3.1. Add Registry-Based Hit Modifier Application
```cpp
// In combat_engine.hpp, alongside existing code

void apply_hit_modifiers_v2(CombatContext& ctx) {
    auto rules = registry_.collect_combat_rules(
        CombatSubPhase::HIT_MODIFIERS,
        ctx.attacker,
        ctx.defender,
        ctx.weapon,
        ctx.combat_type
    );

    for (const auto& rule : rules) {
        // Check condition if present
        if (rule.definition->condition &&
            !rule.definition->condition(ctx)) {
            continue;
        }

        // Apply effect
        auto& effect = std::get<std::function<void(CombatContext&, u8)>>(
            rule.definition->effect);
        effect(ctx, rule.value);

        // Log
        if (logger_) {
            logger_->on_rule_applied(rule.definition->name,
                                     rule.definition->log_format);
        }
    }
}
```

3.2. Add A/B Testing Support
```cpp
// Compare old and new implementations during migration
void resolve_shooting(...) {
    CombatContext ctx = create_context(attacker, defender, weapon, ...);

    // New path
    apply_hit_modifiers_v2(ctx);
    i32 new_hit_modifier = ctx.hit_modifier;

    // Old path (existing code)
    i32 old_hit_modifier = 0;
    if (defender.has_rule(RuleId::Stealth) && distance > 9) {
        old_hit_modifier -= 1;
    }
    // ... rest of old code ...

    // Verify they match (during testing)
    assert(new_hit_modifier == old_hit_modifier &&
           "Hit modifier mismatch - check rule migration");

    // Use old path for now (switch to new after validation)
    hit_modifier = old_hit_modifier;
}
```

3.3. Migrate All Hit Modifier Rules
- Precise, GoodShot, BadShot (attacker bonuses)
- Stealth, RangedShrouding, MeleeEvasion, MeleeShrouding (defender penalties)
- Purge (conditional)
- Thrust (charge-only)

3.4. Run Full Test Suite with Both Paths
- Ensure outputs match exactly
- Performance comparison
- Log comparison

3.5. Switch to New Path, Remove Old Code

**Validation:**
- All hit modifier rules produce identical results
- No performance regression
- Test suite passes

---

### Phase 4: Combat Migration (Remaining Sub-Phases)

**Goal:** Migrate remaining combat sub-phases one at a time.

**Order of Migration:**
1. HIT_MODIFIERS ✓ (Phase 3)
2. ROLL_HITS (Reliable quality override)
3. HIT_SEPARATION (Rending, Rupture)
4. HIT_BONUSES (Relentless, Surge, PointBlankSurge, PredatorFighter)
5. HIT_MULTIPLICATION (Blast, Deadly for hits)
6. DEFENSE_RESOLUTION (AP mods, Shielded, Protected, Bane, Poison, Shred)
7. WOUND_ALLOCATION (Regeneration, Resistance, Takedown, Deadly for wounds, SelfDestruct)

**Tasks per Sub-Phase:**

4.X.1. Create rule definitions for all rules in sub-phase
4.X.2. Implement `apply_[subphase]_v2()` function
4.X.3. Add A/B testing in combat resolution
4.X.4. Verify outputs match
4.X.5. Switch to new path
4.X.6. Remove old conditional code

**Special Handling:**

4.4. HIT_BONUSES - PredatorFighter Recursion
```cpp
// PredatorFighter needs special handling for recursive hits
inline const RuleDefinition PredatorFighter {
    .id = RuleId::PredatorFighter,
    .name = "Predator Fighter",
    .game_phase = GamePhase::COMBAT,
    .sub_phase = CombatSubPhase::HIT_BONUSES,
    .target = Target::ATTACKER,
    .effect = [](CombatContext& ctx, u8 value) {
        if (ctx.natural_sixes == 0) return;

        u32 total_bonus = 0;
        u32 current_sixes = ctx.natural_sixes;

        while (current_sixes > 0) {
            auto result = ctx.dice->roll_quality_test(
                current_sixes,
                ctx.quality_override.value_or(ctx.attacker.quality()),
                ctx.hit_modifier
            );
            total_bonus += result.hits;
            current_sixes = result.sixes;
        }

        ctx.bonus_hits += total_bonus;
    },
    .log_format = "Predator Fighter: {} bonus hits from recursive 6s"
};
```

4.7. WOUND_ALLOCATION - Multiple Triggers
```cpp
// SelfDestruct triggers ON_MODEL_DEATH
inline const RuleDefinition SelfDestruct {
    .id = RuleId::SelfDestruct,
    .name = "Self-Destruct",
    .game_phase = GamePhase::COMBAT,
    .sub_phase = CombatSubPhase::WOUND_ALLOCATION,
    .combat_type = CombatType::MELEE,
    .target = Target::DEFENDER,
    .trigger = Trigger::ON_MODEL_DEATH,
    .effect = [](CombatContext& ctx, u8 value) {
        // Queue retaliatory hits against attacker
        ctx.queued_attacks.push_back(QueuedAttack{
            .source = ctx.defender,
            .target = ctx.attacker,
            .hits = 1,
            .ap = 0,
            .description = "Self-Destruct explosion"
        });
    },
    .log_format = "Self-Destruct: {} explodes, dealing damage to attacker"
};
```

**Validation per Sub-Phase:**
- Rule outputs match existing implementation
- Edge cases handled (empty units, no valid targets, etc.)
- Logging consistent

---

### Phase 5: Unified Combat Path

**Goal:** Merge `resolve_shooting()` and `resolve_melee()` into single function.

**Files to Modify:**
```
include/engine/combat_engine.hpp    - Create unified function
```

**Tasks:**

5.1. Create Unified Combat Resolution
```cpp
CombatResult resolve_combat(
    Unit& attacker,
    Unit& defender,
    const Weapon& weapon,
    CombatType combat_type,
    u32 distance,
    bool is_charge
) {
    // Create context (same for both types)
    CombatContext ctx{
        .attacker = attacker,
        .defender = defender,
        .weapon = weapon,
        .combat_type = combat_type,
        .distance = distance,
        .is_charge = is_charge,
        .log = logger_
    };

    // Execute all sub-phases in order
    for (auto sub_phase : {
        CombatSubPhase::PRE_ATTACK,
        CombatSubPhase::HIT_MODIFIERS,
        CombatSubPhase::ROLL_HITS,
        CombatSubPhase::HIT_SEPARATION,
        CombatSubPhase::HIT_BONUSES,
        CombatSubPhase::HIT_MULTIPLICATION,
        CombatSubPhase::DEFENSE_RESOLUTION,
        CombatSubPhase::WOUND_ALLOCATION
    }) {
        apply_sub_phase(ctx, sub_phase);
    }

    return CombatResult{
        .wounds_dealt = ctx.wounds_allocated,
        .models_killed = ctx.models_killed,
        .attacker_damage = ctx.attacker_wounds_taken  // From SelfDestruct etc.
    };
}

void apply_sub_phase(CombatContext& ctx, CombatSubPhase sub_phase) {
    // Collect applicable rules
    auto rules = registry_.collect_combat_rules(
        sub_phase, ctx.attacker, ctx.defender, ctx.weapon, ctx.combat_type);

    // Aggregate traits before wound allocation
    if (sub_phase == CombatSubPhase::WOUND_ALLOCATION) {
        aggregate_traits(ctx, rules);
    }

    // Apply each rule
    for (const auto& rule : rules) {
        if (should_apply(rule, ctx)) {
            apply_rule(ctx, rule);
        }
    }

    // Execute phase core logic (dice rolls, etc.)
    execute_phase_core(ctx, sub_phase);
}
```

5.2. Implement Trait Aggregation
```cpp
void aggregate_traits(CombatContext& ctx,
                      const std::vector<ApplicableRule>& all_rules) {
    for (const auto& rule : all_rules) {
        auto traits = rule.definition->traits;

        if (traits & static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)) {
            ctx.bypasses_regeneration = true;
        }
        if (traits & static_cast<TraitMask>(RuleTrait::BYPASSES_RESISTANCE)) {
            ctx.bypasses_resistance = true;
        }
        if (traits & static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)) {
            ctx.forces_reroll_defense = true;
        }
    }
}
```

5.3. A/B Test Unified vs Separate Paths
- Run both paths on same inputs
- Compare all outputs
- Verify edge cases

5.4. Remove `resolve_shooting()` and `resolve_melee()`
- Replace all call sites with `resolve_combat()`
- Delete duplicate code (~400 lines removed)

**Validation:**
- Single combat path produces identical results
- Significant code reduction
- All tests pass

---

### Phase 6: Movement and Deployment Migration

**Goal:** Extend registry to handle non-combat phases.

**Tasks:**

6.1. Migrate Movement Rules
```cpp
// include/rules/movement_rules.hpp

inline const RuleDefinition Fast {
    .id = RuleId::Fast,
    .name = "Fast",
    .game_phase = GamePhase::MOVEMENT,
    .sub_phase = MoveSubPhase::CALCULATE_DISTANCE,
    .effect = [](MovementContext& ctx, u8 value) {
        ctx.distance_modifiers += 4;
    },
    .log_format = "Fast: +4\" movement"
};

inline const RuleDefinition Slow {
    .id = RuleId::Slow,
    .name = "Slow",
    .game_phase = GamePhase::MOVEMENT,
    .sub_phase = MoveSubPhase::CALCULATE_DISTANCE,
    .effect = [](MovementContext& ctx, u8 value) {
        ctx.distance_modifiers -= 4;
    },
    .log_format = "Slow: -4\" movement"
};

inline const RuleDefinition Flying {
    .id = RuleId::Flying,
    .name = "Flying",
    .game_phase = GamePhase::MOVEMENT,
    .sub_phase = MoveSubPhase::CALCULATE_DISTANCE,
    .effect = [](MovementContext& ctx, u8 value) {
        ctx.ignores_terrain = TerrainMask::ALL;
        ctx.ignores_engagement = true;
    },
    .log_format = "Flying: ignores terrain and engagement"
};

inline const RuleDefinition Agile {
    .id = RuleId::Agile,
    .name = "Agile",
    .game_phase = GamePhase::MOVEMENT,
    .sub_phase = MoveSubPhase::CHARGE_DECLARE,
    .effect = [](MovementContext& ctx, u8 value) {
        ctx.charge_bonus += 2;
    },
    .log_format = "Agile: +2\" charge range"
};

inline const RuleDefinition HitAndRun_Movement {
    .id = RuleId::HitAndRun,
    .name = "Hit and Run",
    .game_phase = GamePhase::MOVEMENT,
    .sub_phase = MoveSubPhase::POST_MOVE,
    .trigger = Trigger::AFTER_COMBAT,
    .condition = [](const MovementContext& ctx) {
        return ctx.unit.has_flag(Flag::HIT_AND_RUN_PENDING);
    },
    .effect = [](MovementContext& ctx, u8 value) {
        ctx.bonus_move_distance = ctx.base_distance; // Full move
        ctx.unit.clear_flag(Flag::HIT_AND_RUN_PENDING);
    },
    .log_format = "Hit and Run: disengage move"
};
```

6.2. Migrate Deployment Rules
```cpp
// include/rules/deployment_rules.hpp

inline const RuleDefinition Scout {
    .id = RuleId::Scout,
    .name = "Scout",
    .game_phase = GamePhase::DEPLOYMENT,
    .sub_phase = DeploySubPhase::SCOUT_MOVE,
    .effect = [](DeploymentContext& ctx, u8 value) {
        ctx.forward_distance = 12;
    },
    .log_format = "Scout: 12\" forward deployment move"
};

inline const RuleDefinition Ambush {
    .id = RuleId::Ambush,
    .name = "Ambush",
    .game_phase = GamePhase::DEPLOYMENT,
    .sub_phase = DeploySubPhase::INFILTRATE,
    .effect = [](DeploymentContext& ctx, u8 value) {
        ctx.allowed_positions = ctx.battlefield.all_positions()
            .filter([&](const Position& pos) {
                return ctx.battlefield.min_enemy_distance(pos) > 9;
            });
        ctx.deploy_timing = DeployTiming::INFILTRATE;
    },
    .log_format = "Ambush: deploy anywhere >9\" from enemies"
};
```

6.3. Migrate End Round Rules
```cpp
// include/rules/endround_rules.hpp

inline const RuleDefinition Fearless_Morale {
    .id = RuleId::Fearless,
    .name = "Fearless",
    .game_phase = GamePhase::END_ROUND,
    .sub_phase = EndRoundSubPhase::MORALE,
    .effect = [](EndRoundContext& ctx, Unit& unit, u8 value) {
        ctx.morale_reroll[&unit] = true;
    },
    .log_format = "Fearless: may reroll failed morale test"
};

inline const RuleDefinition Regeneration_EndRound {
    .id = RuleId::Regeneration,
    .name = "Regeneration",
    .game_phase = GamePhase::END_ROUND,
    .sub_phase = EndRoundSubPhase::REGENERATION,
    .effect = [](EndRoundContext& ctx, Unit& unit, u8 value) {
        for (auto& model : unit.wounded_models()) {
            if (ctx.dice->roll_d6() >= 5) {
                ctx.heal_amounts[&model] += 1;
            }
        }
    },
    .log_format = "Regeneration: healed {} wounds at end of round"
};
```

6.4. Update Game Runner to Use Registry
- Modify `game_runner.hpp` to use registry for movement
- Modify deployment system (if exists) to use registry
- Modify end-of-round processing to use registry

**Validation:**
- Movement calculations match existing
- Deployment rules work correctly
- End-of-round processing unchanged

---

### Phase 7: AI Integration

**Goal:** Add AI hints to rule definitions and create rule-aware AI controller.

**Files to Create:**
```
include/ai/ai_hints.hpp            - AI hint structures
include/ai/rule_aware_ai.hpp       - New AI controller
```

**Files to Modify:**
```
include/core/rule_definition.hpp   - Add AIHints field
include/rules/*.hpp                - Add AI hints to rules
```

**Tasks:**

7.1. Define AI Hint Structures
```cpp
// include/ai/ai_hints.hpp

enum class DistancePreference : u8 {
    NONE,
    CLOSE,          // 0-6"
    POINT_BLANK,    // 6-9"
    MID_RANGE,      // 9-18"
    LONG_RANGE,     // >18"
    MELEE,          // Wants to charge
    AVOID_MELEE     // Stay out of charge range
};

enum class ActionPreference : u8 {
    NONE,
    CHARGE,
    SHOOT,
    HOLD,
    RETREAT
};

struct ActionPref {
    ActionPreference action = ActionPreference::NONE;
    float weight = 0.0f;
    std::function<bool(const AIContext&)> condition;
};

struct TargetModifier {
    std::variant<
        float,  // Static multiplier
        std::function<float(const AIContext&)>  // Dynamic
    > priority_multiplier = 1.0f;
    std::string_view reason;
};

struct ThreatModifier {
    float charge_penalty = 1.0f;    // <1 = avoid charging this
    float melee_penalty = 1.0f;     // <1 = avoid melee with this
    float shooting_penalty = 1.0f;  // <1 = avoid shooting this
};

struct AIHints {
    std::optional<DistancePreference> preferred_range;
    std::optional<ActionPref> action_preference;
    std::optional<TargetModifier> target_modifier;
    std::optional<ThreatModifier> threat_modifier;
    std::optional<std::function<auto(const AIContext&)>> strategic_choice;
    std::optional<std::function<float(const DeploymentContext&, Position)>>
        deployment_preference;
};
```

7.2. Add AI Hints to Rule Definitions
```cpp
// Update existing rule definitions

inline const RuleDefinition Impact {
    // ... existing fields ...
    .ai_hints = {
        .action_preference = {
            .action = ActionPreference::CHARGE,
            .weight = 0.8f
        },
        .preferred_range = DistancePreference::MELEE
    }
};

inline const RuleDefinition Stealth {
    // ... existing fields ...
    .ai_hints = {
        .preferred_range = DistancePreference::LONG_RANGE,
        .target_modifier = {
            .priority_multiplier = [](const AIContext& ctx) {
                return ctx.distance <= 9 ? 1.2f : 0.7f;
            },
            .reason = "Stealth negated at close range"
        }
    }
};

inline const RuleDefinition SelfDestruct {
    // ... existing fields ...
    .ai_hints = {
        .threat_modifier = {
            .melee_penalty = 0.5f  // Avoid melee
        },
        .target_modifier = {
            .priority_multiplier = 0.5f,
            .reason = "Avoid triggering explosion"
        }
    }
};

inline const RuleDefinition Counter {
    // ... existing fields ...
    .ai_hints = {
        .threat_modifier = {
            .charge_penalty = 0.7f  // Risky to charge
        }
    }
};

inline const RuleDefinition PointBlankSurge {
    // ... existing fields ...
    .ai_hints = {
        .preferred_range = DistancePreference::POINT_BLANK
    }
};

inline const RuleDefinition Relentless {
    // ... existing fields ...
    .ai_hints = {
        .preferred_range = DistancePreference::MID_RANGE
    }
};
```

7.3. Implement Rule-Aware AI Controller
```cpp
// include/ai/rule_aware_ai.hpp

class RuleAwareAIController {
public:
    struct AIPersonality {
        float aggression = 0.5f;
        float risk_tolerance = 0.5f;
        float objective_focus = 0.5f;
        float target_priority = 0.5f;
    };

    ActionDecision decide_action(Unit& unit, const GameState& state);

private:
    AIPreferences aggregate_hints(const Unit& unit);
    ThreatAssessment aggregate_threats(const Unit& target);

    float evaluate_shooting(const Unit& unit, const Unit& target,
                           const AIPreferences& prefs,
                           const ThreatAssessment& threats);
    float evaluate_charge(const Unit& unit, const Unit& target,
                         const AIPreferences& prefs,
                         const ThreatAssessment& threats);
    float evaluate_position(const Unit& unit, const Position& pos,
                           const AIPreferences& prefs);

    RuleRegistry& registry_;
    AIPersonality personality_;
};
```

7.4. Implement Strategic Choice Functions
```cpp
// VersatileAttack - choose AP vs hit bonus
inline const RuleDefinition VersatileAttack {
    // ... existing fields ...
    .ai_hints = {
        .strategic_choice = [](const AIContext& ctx) -> VersatileChoice {
            float ap_value = calculate_ap_improvement(
                ctx.weapon.ap, ctx.target.defense);
            float hit_value = calculate_hit_improvement(
                ctx.attacker.quality, ctx.hit_modifier);

            return ap_value > hit_value
                ? VersatileChoice::AP_BONUS
                : VersatileChoice::HIT_BONUS;
        }
    }
};

// Takedown - target selection
inline const RuleDefinition Takedown {
    // ... existing fields ...
    .ai_hints = {
        .strategic_choice = [](const AIContext& ctx) -> ModelIndex {
            // Priority: heroes > nearly-dead > special weapons > first
            auto& defender = ctx.target_unit;

            for (const auto& model : defender.models()) {
                if (model.is_hero && model.is_alive()) {
                    return model.index;
                }
            }
            for (const auto& model : defender.models()) {
                if (model.wounds_remaining() == 1 && model.is_alive()) {
                    return model.index;
                }
            }
            return defender.first_alive_model().index;
        }
    }
};
```

7.5. Add AI Personality Presets
```cpp
namespace ai_presets {
    constexpr AIPersonality CAUTIOUS   = { 0.3f, 0.2f, 0.7f, 0.5f };
    constexpr AIPersonality BALANCED   = { 0.5f, 0.5f, 0.5f, 0.5f };
    constexpr AIPersonality AGGRESSIVE = { 0.8f, 0.7f, 0.3f, 0.8f };
    constexpr AIPersonality BERSERKER  = { 1.0f, 1.0f, 0.0f, 1.0f };
}
```

7.6. A/B Test New AI vs Old AI
- Run simulations with both controllers
- Compare win rates, decision quality
- Tune personality defaults

**Validation:**
- AI makes smarter decisions based on rules
- No regression in basic AI behavior
- Personality presets produce expected behaviors

---

### Phase 8: Cleanup and Optimization

**Goal:** Remove all old code, optimize performance, add documentation.

**Tasks:**

8.1. Remove Old Conditional Code
- Delete all `if (unit.has_rule(RuleId::X))` checks from combat_engine.hpp
- Delete `resolve_shooting()` and `resolve_melee()` (replaced by `resolve_combat()`)
- Delete parser rule map (use registry)
- Remove duplicate code paths

8.2. Remove Redundant Files
- Old AI controller if fully replaced
- Any backup/compatibility code

8.3. Performance Optimization
```cpp
// Cache rule lookups per combat
class CombatRuleCache {
    std::array<std::vector<ApplicableRule>,
               static_cast<size_t>(CombatSubPhase::COUNT)> by_phase_;

public:
    void populate(const CombatContext& ctx);
    const std::vector<ApplicableRule>& get(CombatSubPhase phase) const;
};

// Optimize rule value lookup (currently O(n))
// Add direct value storage in rule_mask
```

8.4. Add Comprehensive Documentation
```cpp
// Document phase execution order
/**
 * Combat Resolution Phases:
 *
 * 1. PRE_ATTACK
 *    - Limited weapon checks
 *    - VersatileAttack mode selection
 *    - Model count determination
 *
 * 2. HIT_MODIFIERS
 *    - All +/- to hit effects applied
 *    - Order: attacker bonuses, defender penalties, conditional
 *
 * 3. ROLL_HITS
 *    - Quality overrides (Reliable)
 *    - Actual dice rolls
 *    - Natural 6s tracked for later phases
 *
 * ... etc ...
 */
```

8.5. Add Rule Implementation Guide
```markdown
# Adding a New Rule

1. Add RuleId to enum in types.hpp
2. Create RuleDefinition in appropriate rules/*.hpp file
3. Include all required fields:
   - id, name, aliases
   - game_phase, sub_phase
   - combat_type (if COMBAT phase)
   - target, trigger
   - effect function
   - traits (if applicable)
   - ai_hints (recommended)
   - log_format
4. Run tests to verify behavior
5. Done - no other files to modify!
```

**Validation:**
- All tests pass
- Performance equal or better
- Documentation complete
- Code significantly reduced

---

## Migration Timeline Estimate

| Phase | Description | Complexity | Dependencies |
|-------|-------------|------------|--------------|
| 1 | Foundation Data Structures | Low | None |
| 2 | Rule Registry | Medium | Phase 1 |
| 3 | Combat Migration (Hit Mods) | Medium | Phase 2 |
| 4 | Combat Migration (All) | High | Phase 3 |
| 5 | Unified Combat Path | Medium | Phase 4 |
| 6 | Movement/Deployment | Medium | Phase 2 |
| 7 | AI Integration | High | Phase 5, 6 |
| 8 | Cleanup & Optimization | Low | All |

**Recommended Order:** 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8

Phases 3-5 (Combat) and Phase 6 (Movement/Deployment) can be parallelized after Phase 2.

---

## Risk Mitigation

### A/B Testing Strategy
- Every phase runs new and old code in parallel
- Results compared automatically
- Mismatches flagged before switching

### Rollback Points
- Each phase is independently deployable
- Old code kept until new code validated
- Feature flags to switch between implementations

### Testing Requirements
- Existing test suite must pass at each phase
- New tests for registry validation
- Performance benchmarks at each phase
- AI behavior regression tests (Phase 7)

---

## Success Metrics

1. **Code Reduction:** ~40% fewer lines in combat_engine.hpp
2. **Files to Modify for New Rule:** 2 (down from 4-6)
3. **Locations to Update for New Rule:** 1 (down from 6+)
4. **Compile-Time Validation:** 100% of rules validated
5. **AI Rule Awareness:** 100% of rules with AI hints
6. **Test Coverage:** Maintained or improved

---

## Appendix: Rule Migration Checklist

### Combat Rules (Phase 3-4)
- [ ] AP
- [ ] Blast
- [ ] Deadly
- [ ] Lance
- [ ] Poison
- [ ] Precise
- [ ] Reliable
- [ ] Rending
- [ ] Bane
- [ ] Impact
- [ ] Indirect
- [ ] Sniper
- [ ] Lock_On
- [ ] Purge
- [ ] Regeneration
- [ ] Tough
- [ ] Protected
- [ ] Stealth
- [ ] ShieldWall
- [ ] Fearless
- [ ] Furious
- [ ] Hero
- [ ] Relentless
- [ ] Fear
- [ ] Counter
- [ ] Shielded
- [ ] Resistance
- [ ] NoRetreat
- [ ] MoraleBoost
- [ ] Rupture
- [ ] HitAndRun
- [ ] PointBlankSurge
- [ ] Shred
- [ ] Smash
- [ ] Battleborn
- [ ] PredatorFighter
- [ ] RapidCharge
- [ ] SelfDestruct
- [ ] VersatileAttack
- [ ] GoodShot
- [ ] BadShot
- [ ] MeleeEvasion
- [ ] MeleeShrouding
- [ ] RangedShrouding
- [ ] BaneInMelee
- [ ] HoldTheLine
- [ ] Thrust
- [ ] PiercingAssault
- [ ] Surge
- [ ] Limited
- [ ] Takedown

### Movement Rules (Phase 6)
- [ ] Fast
- [ ] Slow
- [ ] Flying
- [ ] Strider
- [ ] Scout
- [ ] Ambush
- [ ] Agile
- [ ] RapidCharge
- [ ] HitAndRun (movement part)

### Deployment Rules (Phase 6)
- [ ] Scout
- [ ] Ambush
- [ ] (Reserves - if implemented)

### End Round Rules (Phase 6)
- [ ] Fearless (morale)
- [ ] MoraleBoost
- [ ] NoRetreat
- [ ] HoldTheLine
- [ ] Regeneration (healing)
- [ ] Battleborn (rally)

### Passive Rules (Phase 6)
- [ ] Tough (stat mod)
- [ ] Hero (allocation priority)
- [ ] Fear (aura)

---

## ADDENDUM: Critical Issue Resolutions

This section addresses critical architectural issues identified in review.

---

### Issue 1: RuleMask Overflow (CRITICAL)

**Problem:** Current `RuleMask = u64` only supports 64 rules. COUNT is currently ~71, and the granting system could add 185+ more rules.

**Solution: Tiered Bitset Architecture**

```cpp
// include/core/types.hpp

// Primary rules (base game rules) - fits in cache line
using PrimaryRuleMask = u64;
constexpr size_t PRIMARY_RULE_LIMIT = 64;

// Extended rules (faction-specific, granted rules) - second tier
using ExtendedRuleMask = std::bitset<256>;

// Hybrid structure for units
struct RulePresence {
    PrimaryRuleMask primary = 0;      // Hot path - 8 bytes, cache-friendly
    ExtendedRuleMask* extended = nullptr;  // Cold path - allocated only if needed

    bool has_rule(RuleId id) const {
        auto idx = static_cast<size_t>(id);
        if (idx < PRIMARY_RULE_LIMIT) {
            return (primary & (1ULL << idx)) != 0;
        }
        return extended && extended->test(idx - PRIMARY_RULE_LIMIT);
    }

    void set_rule(RuleId id) {
        auto idx = static_cast<size_t>(id);
        if (idx < PRIMARY_RULE_LIMIT) {
            primary |= (1ULL << idx);
        } else {
            if (!extended) extended = new ExtendedRuleMask();
            extended->set(idx - PRIMARY_RULE_LIMIT);
        }
    }
};

// Organize RuleId enum: frequently-used rules first (0-63)
enum class RuleId : u16 {  // Expanded from u8 to u16
    None = 0,

    // === PRIMARY RULES (0-63) - Most frequently checked ===
    // Combat modifiers (hot path)
    Precise, Reliable, Rending, Blast, Deadly, Poison, Bane,
    // Defense (hot path)
    Regeneration, Tough, Stealth, Protected, Shielded,
    // Movement (hot path)
    Fast, Slow, Flying,
    // ... rest of frequently-used rules ...

    PRIMARY_COUNT = 64,  // Marker for primary/extended boundary

    // === EXTENDED RULES (64+) - Less frequent ===
    // Faction-specific
    Devout, Battleborn, PredatorFighter,
    // Granted rules (from auras/buffs)
    Granted_Fearless, Granted_Regeneration,
    // ... etc ...

    COUNT
};

static_assert(static_cast<size_t>(RuleId::PRIMARY_COUNT) == 64);
static_assert(static_cast<size_t>(RuleId::COUNT) <= 320);  // Extended limit
```

**Migration:**
1. Reorder existing RuleId enum (most used first)
2. Add RulePresence struct alongside existing RuleMask
3. Migrate has_rule() calls incrementally
4. Remove old RuleMask after full migration

---

### Issue 2: std::function Performance Overhead

**Problem:** `std::function` has heap allocation and type erasure overhead. Hot paths run 100K+ simulations.

**Solution: Function Pointer + Context Pattern**

```cpp
// include/core/rule_effect.hpp

// Fixed function signatures - no type erasure
using CombatEffect = void(*)(CombatContext& ctx, u8 value);
using MovementEffect = void(*)(MovementContext& ctx, u8 value);
using DeployEffect = void(*)(DeploymentContext& ctx, u8 value);
using EndRoundEffect = void(*)(EndRoundContext& ctx, Unit& unit, u8 value);
using ConditionCheck = bool(*)(const CombatContext& ctx);

// Compile-time rule definition (no heap allocation)
struct RuleEffectTable {
    CombatEffect combat_effects[COMBAT_SUB_PHASE_COUNT];
    MovementEffect movement_effects[MOVE_SUB_PHASE_COUNT];
    DeployEffect deploy_effects[DEPLOY_SUB_PHASE_COUNT];
    EndRoundEffect endround_effects[ENDROUND_SUB_PHASE_COUNT];
    ConditionCheck conditions[COMBAT_SUB_PHASE_COUNT];
};

// Static table indexed by RuleId - no indirection
constexpr RuleEffectTable RULE_EFFECTS[static_cast<size_t>(RuleId::COUNT)] = {
    // RuleId::None
    {},
    // RuleId::Precise
    {
        .combat_effects = {
            nullptr,  // PRE_ATTACK
            &precise_hit_modifier,  // HIT_MODIFIERS
            nullptr,  // ROLL_HITS
            // ...
        }
    },
    // ... etc for all rules
};

// Hot path: direct function pointer call
void apply_combat_rules(CombatContext& ctx, CombatSubPhase phase) {
    auto phase_idx = static_cast<size_t>(phase);

    // Iterate attacker rules
    for (const auto& rule : ctx.attacker.rules()) {
        auto rule_idx = static_cast<size_t>(rule.id);
        auto effect = RULE_EFFECTS[rule_idx].combat_effects[phase_idx];
        if (effect) {
            auto condition = RULE_EFFECTS[rule_idx].conditions[phase_idx];
            if (!condition || condition(ctx)) {
                effect(ctx, rule.value);
            }
        }
    }
    // Similar for defender, weapon
}

// Actual effect implementations (free functions, inlinable)
void precise_hit_modifier(CombatContext& ctx, u8 value) {
    ctx.hit_modifier += 1;
}

void stealth_hit_modifier(CombatContext& ctx, u8 value) {
    ctx.hit_modifier -= 1;
}

bool stealth_condition(const CombatContext& ctx) {
    return ctx.distance > 9;
}
```

**Benchmark Target:** Within 20% of current hardcoded performance.

---

### Issue 3: RuleDefinition Struct Size

**Problem:** Proposed struct is ~150-200 bytes vs 2-byte CompactRule.

**Solution: Separate Hot/Cold Data**

```cpp
// include/core/rule_data.hpp

// HOT DATA: Used during combat resolution (~16 bytes per rule)
struct RuleHotData {
    RuleId id;                    // 2 bytes
    GamePhase game_phase;         // 1 byte
    u8 sub_phase;                 // 1 byte (union of all sub-phase enums)
    CombatType combat_type;       // 1 byte
    Target target;                // 1 byte
    Trigger trigger;              // 1 byte
    u8 priority;                  // 1 byte (ordering within phase)
    TraitMask traits;             // 4 bytes
    u8 _padding[4];               // Alignment to 16 bytes
};
static_assert(sizeof(RuleHotData) == 16);

// COLD DATA: Used during parsing, logging, AI (separate allocation)
struct RuleColdData {
    const char* name;             // Pointer to static string
    const char* const* aliases;   // Pointer to static array
    u8 alias_count;
    const char* log_format;
    const char* description;
    AIHints ai_hints;             // Only if AI enabled
};

// Compact storage: array of hot data, lazy-loaded cold data
class RuleDataStore {
    std::array<RuleHotData, MAX_RULES> hot_data_;
    mutable std::array<RuleColdData*, MAX_RULES> cold_data_{};  // Lazy

public:
    const RuleHotData& hot(RuleId id) const {
        return hot_data_[static_cast<size_t>(id)];
    }

    const RuleColdData& cold(RuleId id) const {
        auto idx = static_cast<size_t>(id);
        if (!cold_data_[idx]) {
            cold_data_[idx] = load_cold_data(id);  // One-time load
        }
        return *cold_data_[idx];
    }
};

// Memory: 71 rules × 16 bytes = 1.1KB hot data (fits in L1 cache)
// Cold data loaded on-demand, not during combat
```

---

### Issue 4: Rule Value Lookup O(n)

**Problem:** `get_rule_value()` iterates through all rules on entity.

**Solution: Direct Value Array**

```cpp
// include/core/unit.hpp

struct UnitRuleState {
    RulePresence presence;        // Bitset for has_rule() - O(1)
    std::array<u8, MAX_RULES_PER_UNIT> values{};  // Direct indexing

    bool has_rule(RuleId id) const {
        return presence.has_rule(id);
    }

    u8 get_value(RuleId id) const {
        // O(1) lookup via direct array access
        return has_rule(id) ? values[rule_index(id)] : 0;
    }

    void add_rule(RuleId id, u8 value = 0) {
        presence.set_rule(id);
        values[rule_index(id)] = value;
    }

private:
    // Map RuleId to compact index (only for rules this unit has)
    u8 rule_index(RuleId id) const {
        // Use popcount for index calculation
        auto idx = static_cast<size_t>(id);
        if (idx < 64) {
            return __builtin_popcountll(presence.primary & ((1ULL << idx) - 1));
        }
        // Extended rules use separate indexing
        return 64 + extended_index(id);
    }
};
```

**Alternative: Parallel Arrays**
```cpp
// For units with few rules, parallel arrays are simpler
struct CompactRuleSet {
    std::array<RuleId, 16> ids{};
    std::array<u8, 16> values{};
    RuleMask presence = 0;  // Still O(1) for has_rule()
    u8 count = 0;

    u8 get_value(RuleId id) const {
        if (!(presence & rule_bit(id))) return 0;
        // Linear search through max 16 elements (cache-friendly)
        for (u8 i = 0; i < count; ++i) {
            if (ids[i] == id) return values[i];
        }
        return 0;
    }
};
```

---

### Issue 5: Granting System Architecture

**Problem:** 185+ rules that grant other rules (auras, buffs, marks) are unaddressed.

**Solution: Modifier Layer**

```cpp
// include/core/modifiers.hpp

enum class ModifierSource : u8 {
    PERMANENT,      // From unit definition
    AURA,           // From nearby friendly unit
    BUFF,           // Granted by ability (temporary)
    DEBUFF,         // Applied by enemy
    MARK,           // Target marker
    TERRAIN         // From terrain feature
};

enum class ModifierDuration : u8 {
    PERMANENT,
    UNTIL_END_OF_ROUND,
    UNTIL_END_OF_PHASE,
    UNTIL_ACTIVATED,
    ONCE_PER_GAME,
    X_ROUNDS
};

struct ActiveModifier {
    RuleId granted_rule;
    u8 granted_value;
    ModifierSource source;
    ModifierDuration duration;
    u8 rounds_remaining;  // For X_ROUNDS duration
    u16 source_unit_id;   // For auras - who is granting this

    bool is_expired(u32 current_round, GamePhase phase) const;
};

// Per-unit modifier tracking
struct UnitModifiers {
    // Permanent rules from unit definition
    CompactRuleSet permanent;

    // Active temporary modifiers (small_vector to avoid heap for common case)
    small_vector<ActiveModifier, 4> active;

    // Cached effective rules (rebuilt when modifiers change)
    mutable RulePresence effective_presence;
    mutable bool cache_dirty = true;

    bool has_rule(RuleId id) const {
        if (cache_dirty) rebuild_cache();
        return effective_presence.has_rule(id);
    }

    void add_modifier(ActiveModifier mod) {
        active.push_back(mod);
        cache_dirty = true;
    }

    void expire_modifiers(u32 round, GamePhase phase) {
        active.erase(
            std::remove_if(active.begin(), active.end(),
                [&](const auto& m) { return m.is_expired(round, phase); }),
            active.end()
        );
        cache_dirty = true;
    }

private:
    void rebuild_cache() const {
        effective_presence = permanent.presence;
        for (const auto& mod : active) {
            effective_presence.set_rule(mod.granted_rule);
        }
        cache_dirty = false;
    }
};

// Aura processing (run at start of each phase)
class AuraProcessor {
public:
    void process_auras(GameState& state, GamePhase phase) {
        // Remove expired aura modifiers
        for (auto& unit : state.all_units()) {
            unit.modifiers.expire_modifiers(state.round(), phase);
        }

        // Apply active auras
        for (const auto& unit : state.all_units()) {
            if (!unit.is_alive()) continue;

            for (const auto& rule : unit.rules_with_trait(RuleTrait::AURA_EFFECT)) {
                apply_aura(state, unit, rule);
            }
        }
    }

private:
    void apply_aura(GameState& state, const Unit& source, CompactRule rule) {
        auto& aura_def = registry_.get_aura_definition(rule.id);
        float range = aura_def.range;

        for (auto& target : state.units_within(source.position, range)) {
            if (aura_def.affects_enemies && target.is_enemy(source) ||
                aura_def.affects_allies && target.is_ally(source)) {

                target.modifiers.add_modifier(ActiveModifier{
                    .granted_rule = aura_def.granted_rule,
                    .granted_value = rule.value,
                    .source = ModifierSource::AURA,
                    .duration = ModifierDuration::UNTIL_END_OF_PHASE,
                    .source_unit_id = source.id
                });
            }
        }
    }
};

// Example aura definitions
struct AuraDefinition {
    RuleId source_rule;       // The aura rule itself
    RuleId granted_rule;      // What it grants
    float range;              // Aura radius
    bool affects_allies;
    bool affects_enemies;
    bool affects_self;
};

constexpr AuraDefinition AURA_DEFINITIONS[] = {
    // Fearless Aura grants Fearless to allies within 6"
    { RuleId::FearlessAura, RuleId::Fearless, 6.0f, true, false, false },
    // Fear affects enemies within 12"
    { RuleId::Fear, RuleId::Feared, 12.0f, false, true, false },
    // ... etc
};
```

---

### Issue 6: A/B Testing for Stochastic Code

**Problem:** Dice rolls make direct comparison impossible.

**Solution: Deterministic Test Harness**

```cpp
// include/testing/deterministic_dice.hpp

class DeterministicDice : public DiceRoller {
    std::vector<u8> predetermined_rolls_;
    size_t roll_index_ = 0;

public:
    explicit DeterministicDice(std::vector<u8> rolls)
        : predetermined_rolls_(std::move(rolls)) {}

    u8 roll_d6() override {
        assert(roll_index_ < predetermined_rolls_.size());
        return predetermined_rolls_[roll_index_++];
    }

    void reset() { roll_index_ = 0; }
};

// A/B test harness
class ABTestHarness {
public:
    struct TestResult {
        bool outputs_match;
        std::string differences;
        double old_time_ms;
        double new_time_ms;
    };

    TestResult compare_combat(
        const Unit& attacker,
        const Unit& defender,
        const Weapon& weapon,
        const std::vector<u8>& dice_sequence
    ) {
        // Create identical dice for both paths
        DeterministicDice dice_old(dice_sequence);
        DeterministicDice dice_new(dice_sequence);

        // Run old implementation
        auto start_old = high_resolution_clock::now();
        auto result_old = old_engine_.resolve_shooting(
            attacker, defender, weapon, dice_old);
        auto end_old = high_resolution_clock::now();

        // Run new implementation
        dice_new.reset();
        auto start_new = high_resolution_clock::now();
        auto result_new = new_engine_.resolve_combat(
            attacker, defender, weapon, CombatType::SHOOTING, dice_new);
        auto end_new = high_resolution_clock::now();

        // Compare results
        return TestResult{
            .outputs_match = compare_results(result_old, result_new),
            .differences = diff_results(result_old, result_new),
            .old_time_ms = duration<double, milli>(end_old - start_old).count(),
            .new_time_ms = duration<double, milli>(end_new - start_new).count()
        };
    }

    // Run exhaustive tests with coverage
    void run_comprehensive_tests() {
        // Generate test cases covering all rule combinations
        auto test_cases = generate_rule_combination_tests();

        for (const auto& tc : test_cases) {
            // Generate deterministic dice sequences
            for (int seed = 0; seed < 100; ++seed) {
                auto dice = generate_dice_sequence(seed, tc.expected_rolls);
                auto result = compare_combat(
                    tc.attacker, tc.defender, tc.weapon, dice);

                if (!result.outputs_match) {
                    report_failure(tc, result);
                }
            }
        }
    }
};

// Golden file testing for complex scenarios
class GoldenFileTests {
public:
    void record_golden(const std::string& name, const CombatScenario& scenario) {
        auto result = run_with_seed(scenario, GOLDEN_SEED);
        save_golden(name, result);
    }

    bool verify_against_golden(const std::string& name,
                               const CombatScenario& scenario) {
        auto expected = load_golden(name);
        auto actual = run_with_seed(scenario, GOLDEN_SEED);
        return expected == actual;
    }
};
```

---

### Issue 7: CombatContext Size Growth

**Problem:** Context struct grows unbounded with new rules.

**Solution: Tiered Context Structure**

```cpp
// include/core/combat_context.hpp

// CORE context: Always needed, kept small (~64 bytes)
struct CombatContextCore {
    Unit* attacker;
    Unit* defender;
    Weapon* weapon;
    CombatType combat_type;
    u32 distance;
    bool is_charge;

    // Essential modifiers
    i32 hit_modifier = 0;
    i32 defense_modifier = 0;
    i32 ap_modifier = 0;

    // Hit tracking
    u32 base_hits = 0;
    u32 final_hits = 0;
    u32 natural_sixes = 0;

    // Wound tracking
    u32 wounds_to_allocate = 0;
    u32 wounds_allocated = 0;

    // Aggregated traits (bitfield)
    u8 trait_flags = 0;
    static constexpr u8 BYPASS_REGEN = 1 << 0;
    static constexpr u8 BYPASS_RESIST = 1 << 1;
    static constexpr u8 FORCE_REROLL = 1 << 2;
};
static_assert(sizeof(CombatContextCore) <= 64);  // Fits in cache line

// EXTENDED context: Allocated only when needed
struct CombatContextExtended {
    // Rending/Rupture separation
    u32 rending_hits = 0;
    u32 rupture_hits = 0;
    u32 bonus_hits = 0;

    // Quality override
    std::optional<u8> quality_override;

    // Wound multipliers
    float wound_multiplier = 1.0f;
    u32 hit_multiplier = 1;

    // Queued effects
    std::vector<QueuedAttack> queued_attacks;

    // Detailed tracking for logging
    std::vector<RuleApplication> applied_rules;
};

// Full context wraps both
struct CombatContext {
    CombatContextCore core;
    CombatContextExtended* extended = nullptr;  // Lazy allocation

    CombatContextExtended& ext() {
        if (!extended) extended = new CombatContextExtended();
        return *extended;
    }

    // Convenience accessors that check extended
    u32 rending_hits() const {
        return extended ? extended->rending_hits : 0;
    }
};

// Pool allocator for extended contexts (avoid per-combat allocation)
class CombatContextPool {
    std::vector<CombatContextExtended> pool_;
    std::vector<CombatContextExtended*> free_list_;

public:
    CombatContextExtended* acquire() {
        if (free_list_.empty()) {
            pool_.emplace_back();
            return &pool_.back();
        }
        auto* ctx = free_list_.back();
        free_list_.pop_back();
        *ctx = CombatContextExtended{};  // Reset
        return ctx;
    }

    void release(CombatContextExtended* ctx) {
        free_list_.push_back(ctx);
    }
};
```

---

### Issue 8: Rule Ordering Within Phases

**Problem:** Order of rule application within a phase is undefined.

**Solution: Explicit Priority System**

```cpp
// include/core/rule_priority.hpp

// Priority levels within each phase
enum class RulePriority : u8 {
    FIRST = 0,       // Always applies first (e.g., quality overrides)
    EARLY = 32,      // Early modifiers
    NORMAL = 64,     // Default priority
    LATE = 96,       // Late modifiers
    LAST = 128,      // Always applies last (e.g., caps, floors)

    // Special priorities for known interactions
    RELIABLE = FIRST,           // Quality override before modifiers
    STEALTH = EARLY,            // Defender penalties early
    PRECISE = NORMAL,           // Standard hit bonus
    PURGE = LATE,               // Conditional bonus after others
    HIT_CAP = LAST,             // Any caps on final value
};

// Rule definition includes priority
struct RuleHotData {
    // ... other fields ...
    RulePriority priority = RulePriority::NORMAL;
};

// Collector returns rules sorted by priority
std::vector<ApplicableRule> collect_combat_rules(
    CombatSubPhase phase,
    const CombatContext& ctx
) {
    std::vector<ApplicableRule> rules;

    // Collect from attacker, defender, weapon
    collect_from_entity(rules, ctx.attacker, Target::ATTACKER, phase);
    collect_from_entity(rules, ctx.defender, Target::DEFENDER, phase);
    collect_from_entity(rules, ctx.weapon, Target::WEAPON, phase);

    // Sort by priority (stable sort preserves order within same priority)
    std::stable_sort(rules.begin(), rules.end(),
        [](const auto& a, const auto& b) {
            return a.definition->priority < b.definition->priority;
        });

    return rules;
}

// Document ordering explicitly
/**
 * HIT_MODIFIERS Phase Execution Order:
 *
 * FIRST (0):
 *   - (none currently)
 *
 * EARLY (32):
 *   - Stealth: -1 if distance > 9"
 *   - RangedShrouding: -1 to be hit at range
 *   - MeleeEvasion: -1 to be hit in melee
 *   - MeleeShrouding: -1 to be hit in melee
 *
 * NORMAL (64):
 *   - Precise: +1 to hit
 *   - GoodShot: +1 when shooting
 *   - BadShot: -1 when shooting
 *   - Thrust: +1 when charging
 *
 * LATE (96):
 *   - Purge: +1 vs Tough(3+) targets
 *
 * LAST (128):
 *   - (future: hit modifier caps)
 */
```

---

### Issue 9: Trait Aggregation Timing

**Problem:** Traits aggregated at WOUND_ALLOCATION but needed earlier.

**Solution: Aggregate at Combat Start**

```cpp
// include/engine/combat_resolution.hpp

CombatResult resolve_combat(CombatContext& ctx) {
    // FIRST: Aggregate traits from ALL rules that will apply
    // This happens BEFORE any phase executes
    aggregate_all_traits(ctx);

    // Now execute phases with trait info available
    for (auto phase : COMBAT_PHASES) {
        apply_phase(ctx, phase);
    }

    return build_result(ctx);
}

void aggregate_all_traits(CombatContext& ctx) {
    ctx.core.trait_flags = 0;

    // Check attacker rules
    for (const auto& rule : ctx.core.attacker->rules()) {
        auto traits = RULE_HOT_DATA[static_cast<size_t>(rule.id)].traits;
        ctx.core.trait_flags |= extract_combat_traits(traits);
    }

    // Check weapon rules
    for (const auto& rule : ctx.core.weapon->rules()) {
        auto traits = RULE_HOT_DATA[static_cast<size_t>(rule.id)].traits;
        ctx.core.trait_flags |= extract_combat_traits(traits);
    }

    // Now defense resolution can check ctx.bypasses_regeneration()
}

// Accessor methods
bool CombatContext::bypasses_regeneration() const {
    return core.trait_flags & CombatContextCore::BYPASS_REGEN;
}
```

---

### Issue 10: Registry Singleton

**Problem:** Singleton has testing, thread safety, and initialization order issues.

**Solution: Dependency Injection**

```cpp
// include/core/rule_registry.hpp

// Registry is no longer a singleton - passed explicitly
class RuleRegistry {
public:
    // No static instance() method

    // Construction
    explicit RuleRegistry(const RuleDataStore& data);

    // Non-copyable but movable
    RuleRegistry(const RuleRegistry&) = delete;
    RuleRegistry& operator=(const RuleRegistry&) = delete;
    RuleRegistry(RuleRegistry&&) = default;

    // ... query methods ...
};

// Factory function for production
RuleRegistry create_default_registry();

// Factory function for testing
RuleRegistry create_test_registry(const std::vector<RuleHotData>& rules);

// Combat engine takes registry as constructor parameter
class CombatEngine {
public:
    explicit CombatEngine(const RuleRegistry& registry, DiceRoller& dice)
        : registry_(registry), dice_(dice) {}

    CombatResult resolve_combat(CombatContext& ctx);

private:
    const RuleRegistry& registry_;
    DiceRoller& dice_;
};

// Game initializes and owns the registry
class Game {
    RuleRegistry registry_;
    RandomDice dice_;
    CombatEngine combat_engine_;

public:
    Game()
        : registry_(create_default_registry())
        , combat_engine_(registry_, dice_)
    {}
};

// Tests can create isolated registries
TEST(CombatEngine, PreciseAddsOneToHit) {
    auto registry = create_test_registry({
        rules::Precise  // Only this rule
    });
    MockDice dice({5, 5, 5});  // Predetermined rolls
    CombatEngine engine(registry, dice);

    // Test in isolation
    auto result = engine.resolve_combat(make_test_context());
    EXPECT_EQ(result.hit_modifier, 1);
}
```

---

### Issue 11: Compilation Time Impact

**Problem:** Inline constexpr with lambdas in headers slows compilation.

**Solution: Explicit Instantiation in .cpp Files**

```cpp
// include/rules/combat_rules.hpp
// Header only declares, doesn't define

namespace rules {

// Declaration only
extern const RuleHotData Precise_Data;
extern const RuleHotData Stealth_Data;
extern const RuleHotData Rending_Data;
// ... etc

// Effect function declarations (defined in .cpp)
void precise_effect(CombatContext& ctx, u8 value);
void stealth_effect(CombatContext& ctx, u8 value);
bool stealth_condition(const CombatContext& ctx);
void rending_effect(CombatContext& ctx, u8 value);

} // namespace rules

// ============================================
// src/rules/combat_rules.cpp
// Definitions in single compilation unit

#include "rules/combat_rules.hpp"

namespace rules {

// Effect implementations
void precise_effect(CombatContext& ctx, u8 value) {
    ctx.hit_modifier += 1;
}

void stealth_effect(CombatContext& ctx, u8 value) {
    ctx.hit_modifier -= 1;
}

bool stealth_condition(const CombatContext& ctx) {
    return ctx.distance > 9;
}

void rending_effect(CombatContext& ctx, u8 value) {
    ctx.ext().rending_hits = ctx.natural_sixes;
    ctx.base_hits -= ctx.natural_sixes;
}

// Data definitions
const RuleHotData Precise_Data {
    .id = RuleId::Precise,
    .game_phase = GamePhase::COMBAT,
    .sub_phase = static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    .combat_type = CombatType::BOTH,
    .target = Target::ATTACKER,
    .priority = RulePriority::NORMAL,
    .traits = 0
};

const RuleHotData Stealth_Data {
    .id = RuleId::Stealth,
    .game_phase = GamePhase::COMBAT,
    .sub_phase = static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    .combat_type = CombatType::SHOOTING,
    .target = Target::DEFENDER,
    .priority = RulePriority::EARLY,
    .traits = 0
};

// ... etc

// Global effect table (single definition)
const RuleEffectTable EFFECT_TABLE[] = {
    // RuleId::None
    {},
    // RuleId::Precise
    { .combat_effects = { nullptr, precise_effect, nullptr, ... } },
    // RuleId::Stealth
    { .combat_effects = { nullptr, stealth_effect, nullptr, ... },
      .conditions = { nullptr, stealth_condition, nullptr, ... } },
    // ...
};

} // namespace rules
```

---

### Issue 12: AI Hints Type System

**Problem:** `std::function<auto(...)>` is invalid C++.

**Solution: Typed Choice Variants**

```cpp
// include/ai/ai_choices.hpp

// Define all possible choice types
struct VersatileChoice { bool use_ap_bonus; };
struct TakedownChoice { u8 target_model_index; };
struct DeploymentChoice { Position position; };

// Variant of all choice types
using StrategicChoice = std::variant<
    std::monostate,      // No choice needed
    VersatileChoice,
    TakedownChoice,
    DeploymentChoice
>;

// Choice function type - returns variant
using ChoiceFunction = StrategicChoice(*)(const AIContext& ctx);

// AI hints with proper typing
struct AIHints {
    DistancePreference preferred_range = DistancePreference::NONE;
    ActionPref action_preference{};
    TargetModifier target_modifier{};
    ThreatModifier threat_modifier{};
    ChoiceFunction strategic_choice = nullptr;  // Function pointer, not std::function
};

// Choice implementations
StrategicChoice versatile_attack_choice(const AIContext& ctx) {
    float ap_value = calculate_ap_improvement(ctx);
    float hit_value = calculate_hit_improvement(ctx);
    return VersatileChoice{ .use_ap_bonus = (ap_value > hit_value) };
}

StrategicChoice takedown_choice(const AIContext& ctx) {
    // Find best target
    for (const auto& model : ctx.target_unit.models()) {
        if (model.is_hero && model.is_alive()) {
            return TakedownChoice{ .target_model_index = model.index };
        }
    }
    return TakedownChoice{ .target_model_index = 0 };
}

// Using the choice
void apply_versatile_attack(CombatContext& ctx, const AIHints& hints) {
    if (!hints.strategic_choice) {
        // Random fallback
        ctx.use_ap_bonus = (dice_.roll_d6() <= 3);
        return;
    }

    auto choice = hints.strategic_choice(make_ai_context(ctx));
    if (auto* vc = std::get_if<VersatileChoice>(&choice)) {
        ctx.use_ap_bonus = vc->use_ap_bonus;
    }
}
```

---

### Issue 13: Rule Interactions

**Problem:** Complex interactions between rules not captured.

**Solution: Explicit Interaction Rules**

```cpp
// include/core/rule_interactions.hpp

enum class InteractionType : u8 {
    MUTUALLY_EXCLUSIVE,  // Only one can apply
    STACKS,              // Both apply additively
    MULTIPLIES,          // Second multiplies first
    OVERRIDES,           // Second completely replaces first
    CONDITIONAL,         // Custom logic
};

struct RuleInteraction {
    RuleId rule_a;
    RuleId rule_b;
    InteractionType type;
    // For CONDITIONAL type, custom handler
    void (*handler)(CombatContext& ctx, u8 value_a, u8 value_b) = nullptr;
};

// Defined interactions
constexpr RuleInteraction RULE_INTERACTIONS[] = {
    // Blast + Takedown: Takedown limits Blast effectiveness
    { RuleId::Blast, RuleId::Takedown, InteractionType::CONDITIONAL,
      &blast_takedown_interaction },

    // Deadly bypasses Regeneration (handled via traits, but documented)
    { RuleId::Deadly, RuleId::Regeneration, InteractionType::OVERRIDES },

    // Rending + Rupture: A hit can trigger both, but goes to Rending
    { RuleId::Rending, RuleId::Rupture, InteractionType::CONDITIONAL,
      &rending_rupture_interaction },

    // Multiple hit modifiers stack
    { RuleId::Precise, RuleId::GoodShot, InteractionType::STACKS },
    { RuleId::Stealth, RuleId::RangedShrouding, InteractionType::STACKS },

    // Takedown + Deadly: Takedown determines target, Deadly applies wounds
    { RuleId::Takedown, RuleId::Deadly, InteractionType::STACKS },
};

// Interaction handlers
void blast_takedown_interaction(CombatContext& ctx, u8 blast_value, u8) {
    // Blast hits still multiply, but Takedown targets single model
    // So effective wounds are capped by target model's health
    if (ctx.has_takedown_target()) {
        ctx.ext().hit_multiplier = blast_value;
        ctx.ext().takedown_active = true;
        // Wounds will be capped during allocation
    }
}

void rending_rupture_interaction(CombatContext& ctx, u8, u8) {
    // Natural 6s go to Rending (higher priority), not Rupture
    // Rupture only gets 6s if weapon has Rupture but not Rending
    if (ctx.weapon_has_rule(RuleId::Rending)) {
        ctx.ext().rending_hits = ctx.natural_sixes;
        ctx.ext().rupture_hits = 0;
    } else {
        ctx.ext().rupture_hits = ctx.natural_sixes;
    }
}

// Lookup during combat
void check_interactions(CombatContext& ctx, RuleId new_rule) {
    for (const auto& interaction : RULE_INTERACTIONS) {
        if (interaction.rule_a == new_rule || interaction.rule_b == new_rule) {
            RuleId other = (interaction.rule_a == new_rule)
                ? interaction.rule_b : interaction.rule_a;

            if (ctx.has_active_rule(other)) {
                apply_interaction(ctx, interaction);
            }
        }
    }
}
```

---

### Issue 14: Unified Path Optimization

**Problem:** Unified path adds branch overhead for combat type checks.

**Solution: Template Specialization**

```cpp
// include/engine/combat_resolution.hpp

// Template for combat-type-specific optimization
template<CombatType Type>
class CombatResolver {
public:
    CombatResult resolve(CombatContext& ctx);

private:
    // Only instantiated for relevant combat type
    void apply_shooting_rules(CombatContext& ctx);  // Only in SHOOTING specialization
    void apply_melee_rules(CombatContext& ctx);     // Only in MELEE specialization
};

// Shooting specialization - no melee checks compiled in
template<>
class CombatResolver<CombatType::SHOOTING> {
public:
    CombatResult resolve(CombatContext& ctx) {
        // Only shooting-applicable rules are in this code path
        apply_hit_modifiers<CombatType::SHOOTING>(ctx);
        roll_hits(ctx);
        apply_hit_bonuses<CombatType::SHOOTING>(ctx);
        // ...
        return build_result(ctx);
    }
};

// Melee specialization - no shooting checks compiled in
template<>
class CombatResolver<CombatType::MELEE> {
public:
    CombatResult resolve(CombatContext& ctx) {
        apply_hit_modifiers<CombatType::MELEE>(ctx);
        roll_hits(ctx);
        apply_hit_bonuses<CombatType::MELEE>(ctx);
        // ...
        return build_result(ctx);
    }
};

// Rule application also templated
template<CombatType Type>
void apply_hit_modifiers(CombatContext& ctx) {
    // Compile-time filter: only rules for this combat type
    for (const auto& rule : ctx.attacker->rules()) {
        auto& data = RULE_HOT_DATA[static_cast<size_t>(rule.id)];

        // constexpr check - optimized away at compile time
        if constexpr (Type == CombatType::SHOOTING) {
            if (data.combat_type == CombatType::MELEE) continue;
        } else {
            if (data.combat_type == CombatType::SHOOTING) continue;
        }

        if (data.sub_phase == static_cast<u8>(CombatSubPhase::HIT_MODIFIERS)) {
            auto effect = EFFECT_TABLE[static_cast<size_t>(rule.id)]
                .combat_effects[static_cast<size_t>(CombatSubPhase::HIT_MODIFIERS)];
            if (effect) effect(ctx, rule.value);
        }
    }
}

// Dispatch at runtime (single branch)
CombatResult resolve_combat(CombatContext& ctx) {
    if (ctx.combat_type == CombatType::SHOOTING) {
        return CombatResolver<CombatType::SHOOTING>{}.resolve(ctx);
    } else {
        return CombatResolver<CombatType::MELEE>{}.resolve(ctx);
    }
}
```

---

### Issue 15: Migration Order Optimization

**Revised Migration Order:**

```
Phase 1: Foundation Data Structures
    ↓
Phase 2: Rule Registry + RuleMask Fix
    ↓
    ├─────────────────────────────────┐
    ↓                                 ↓
Phase 3-5: Combat Migration     Phase 6: Movement/Deployment
(can run in parallel)           (can run in parallel)
    ↓                                 ↓
    └─────────────────────────────────┘
                    ↓
            Phase 7: AI Integration
                    ↓
        Phase 8: Cleanup & Optimization
```

**Revised Phase 2 (Critical Path):**

```
Phase 2: Foundation + RuleMask Fix (BLOCKING)
├── 2.1: Implement RulePresence with tiered bitset
├── 2.2: Migrate has_rule() to new structure
├── 2.3: Implement function pointer effect table
├── 2.4: Create RuleRegistry with DI pattern
├── 2.5: Add hot/cold data separation
├── 2.6: Implement priority system
└── 2.7: Benchmark against current performance
```

**Parallel Tracks After Phase 2:**

```
Track A: Combat System                Track B: Non-Combat
─────────────────────                ─────────────────────
Phase 3: Hit Modifiers               Phase 6.1: Movement Rules
Phase 4: Remaining Sub-Phases        Phase 6.2: Deployment Rules
Phase 5: Unified Combat Path         Phase 6.3: End Round Rules
```

---

## Updated Risk Assessment

| Issue | Severity | Status |
|-------|----------|--------|
| RuleMask overflow | Critical | ✅ Addressed (tiered bitset) |
| std::function performance | High | ✅ Addressed (function pointers) |
| RuleDefinition size | High | ✅ Addressed (hot/cold split) |
| Rule value lookup O(n) | Medium | ✅ Addressed (direct array) |
| Granting system | High | ✅ Addressed (modifier layer) |
| A/B testing stochastic | High | ✅ Addressed (deterministic harness) |
| CombatContext size | Medium | ✅ Addressed (tiered context) |
| Rule ordering | Medium | ✅ Addressed (priority system) |
| Trait aggregation timing | Medium | ✅ Addressed (aggregate at start) |
| Singleton issues | Medium | ✅ Addressed (dependency injection) |
| Compilation time | Medium | ✅ Addressed (.cpp definitions) |
| AI hints type system | Medium | ✅ Addressed (typed variants) |
| Rule interactions | Medium | ✅ Addressed (interaction table) |
| Unified path optimization | Low | ✅ Addressed (template specialization) |
| Migration order | Low | ✅ Addressed (parallel tracks) |

---

## Performance Targets

| Metric | Current | Target | Validation |
|--------|---------|--------|------------|
| has_rule() | O(1) bitwise | O(1) bitwise | Maintain |
| get_rule_value() | O(n) linear | O(1) direct | Benchmark |
| Combat resolution | ~X μs | ≤1.2X μs | Benchmark |
| Memory per unit | ~70 bytes | ≤100 bytes | sizeof() |
| Compilation time | ~X sec | ≤1.5X sec | CI timing |

**Benchmark Suite:**
- 100K combat resolutions with varied rule combinations
- Memory profiling for unit/rule storage
- Cache miss analysis for hot path
