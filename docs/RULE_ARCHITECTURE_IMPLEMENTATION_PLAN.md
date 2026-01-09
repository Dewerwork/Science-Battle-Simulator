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
