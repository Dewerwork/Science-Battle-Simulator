# Rule Implementation Guide

This guide explains how to add new rules to the Battle Simulator using the
Rule Registry Architecture.

## Overview

Adding a new rule requires modifications to only **2 files**:

1. `include/core/types.hpp` - Add the RuleId enum value
2. `src/rules/combat_rules.cpp` - Add rule definition and effect

Compare this to the old approach which required 4-6 files!

## Step-by-Step Guide

### Step 1: Add RuleId to Enum

In `include/core/types.hpp`, add your rule to the `RuleId` enum:

```cpp
enum class RuleId : u16 {
    // ... existing rules ...

    // Your new rule (add in appropriate section)
    MyNewRule,    // Brief description

    // Keep COUNT at the end
    COUNT
};
```

**Important:** If your rule is frequently checked during combat (hot path),
add it before `PRIMARY_COUNT` (rules 1-63). Less frequent rules go after.

### Step 2: Define Rule Hot Data

In `src/rules/combat_rules.cpp`, add the hot data definition:

```cpp
// In the PHASE_HOT_DATA array
{
    RuleId::MyNewRule,              // id
    GamePhase::COMBAT,               // game_phase
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),  // sub_phase
    CombatType::BOTH,                // combat_type (SHOOTING, MELEE, or BOTH)
    Target::ATTACKER,                // target (ATTACKER, DEFENDER, WEAPON)
    Trigger::ALWAYS,                 // trigger (ALWAYS, ON_CHARGE, etc.)
    RulePriority::NORMAL,            // priority
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)  // traits
},
```

### Step 3: Define Rule Cold Data

Add the cold data (names, aliases, description):

```cpp
// In the PHASE_COLD_DATA array
{
    "My New Rule",                   // name
    my_new_rule_aliases,             // aliases array
    ARRAY_SIZE(my_new_rule_aliases), // alias_count
    "{attacker} applies MyNewRule",  // log_format
    "Description of what the rule does"  // description
},

// Aliases (define before the array)
static const char* const my_new_rule_aliases[] = {
    "MyNewRule", "mynewrule", "my-new-rule"
};
```

### Step 4: Implement the Effect Function

Create the effect function:

```cpp
// Effect function for combat rules
void my_new_rule_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value) {
    // Example: Add +1 to hit modifier
    ctx.hit_modifier += 1;

    // Example: Use rule value (for rules like Blast(X))
    // u8 blast_value = value;
}

// Effect function for movement rules
void my_new_movement_effect(MovementContext& ctx, u8 value) {
    // Example: Add distance modifier
    ctx.distance_modifier += value;
}
```

### Step 5: Register the Effect

In the `register_effects()` function:

```cpp
// Register using EffectBuilder
registry.register_effects(
    RuleId::MyNewRule,
    EffectBuilder()
        .combat_effect(CombatSubPhase::HIT_MODIFIERS, my_new_rule_effect)
        .build()
);
```

### Step 6: Add AI Hints (Optional)

In `include/ai/rule_aware_ai.hpp`, add hints in `get_rule_hints()`:

```cpp
case RuleId::MyNewRule:
    return AIHintBuilder()
        .offensive_bonus()
        .offense(20)
        .consistent();
```

### Step 7: Add Tests

Create or update tests to verify your rule:

```cpp
void test_my_new_rule() {
    RuleRegistry registry = create_default_registry();

    // Verify registration
    ASSERT_TRUE(registry.is_registered(RuleId::MyNewRule));

    // Verify hot data
    const auto& hot = registry.get_hot_data(RuleId::MyNewRule);
    ASSERT_EQ(hot.game_phase, GamePhase::COMBAT);

    // Test effect application
    // ...
}
```

## Rule Categories

### Combat Rules

Combat rules apply during attack resolution. Sub-phases:

- `PRE_ATTACK` - Before rolling (Limited, VersatileAttack)
- `HIT_MODIFIERS` - Modify hit roll (Precise, Stealth)
- `ROLL_HITS` - Override quality (Reliable)
- `HIT_SEPARATION` - Categorize hits (Rending, Rupture)
- `HIT_BONUSES` - Generate extra hits (Relentless, Surge)
- `HIT_MULTIPLICATION` - Multiply hits (Blast)
- `DEFENSE_RESOLUTION` - Modify defense (Shielded, ShieldWall)
- `WOUND_ALLOCATION` - Apply wounds (Regeneration, Deadly)

### Movement Rules

Movement rules apply during movement phase. Sub-phases:

- `CALCULATE_DISTANCE` - Modify move distance (Fast, Slow)
- `EXECUTE_MOVE` - Terrain effects (Flying, Strider)
- `CHARGE_RESOLVE` - Charge bonuses (RapidCharge)
- `POST_MOVE` - After movement (HitAndRun)

### Deployment Rules

Deployment rules apply during setup. Sub-phases:

- `SCOUT_MOVE` - Forward deployment (Scout)
- `INFILTRATE` - Anywhere deployment (Ambush)

### End Round Rules

End-of-round rules apply after combat. Sub-phases:

- `MORALE_CHECK` - Morale effects (Fearless, NoRetreat)
- `REGENERATION` - End-of-turn healing (Regeneration)
- `BATTLESHOCK` - Rally effects (Battleborn)

## Rule Traits

Traits provide fast behavioral checks. Common traits:

```cpp
RuleTrait::MODIFIES_HIT_ROLL      // Affects hit rolls
RuleTrait::MODIFIES_DEFENSE       // Affects defense
RuleTrait::MODIFIES_AP            // Affects armor piercing
RuleTrait::GENERATES_EXTRA_HITS   // Creates bonus hits
RuleTrait::BYPASSES_REGENERATION  // Ignores regen saves
RuleTrait::CHARGE_ONLY            // Only when charging
RuleTrait::MELEE_ONLY             // Only in melee
RuleTrait::RANGED_ONLY            // Only when shooting
RuleTrait::HAS_VALUE              // Has numeric value (X)
```

## Condition Functions

For rules with complex activation conditions:

```cpp
bool my_condition(const CombatContextCore& ctx) {
    // Example: Only active at long range
    return ctx.distance > 9;
}

// Register with condition
registry.register_effects(
    RuleId::MyRule,
    EffectBuilder()
        .combat_effect(CombatSubPhase::HIT_MODIFIERS, my_effect)
        .condition(CombatSubPhase::HIT_MODIFIERS, my_condition)
        .build()
);
```

## Best Practices

1. **Use appropriate sub-phase** - Put effects in the phase where they
   logically belong.

2. **Use traits for fast checks** - Hot path code should check traits,
   not iterate through rules.

3. **Keep effects simple** - One effect per phase. Complex rules may
   need multiple sub-phase effects.

4. **Add AI hints** - Help the AI make good decisions with your rule.

5. **Test thoroughly** - Include positive and negative test cases.

6. **Document in cold data** - Description should explain the rule
   clearly for players.

## Example: Adding "Lucky" Rule

```cpp
// types.hpp
enum class RuleId : u16 {
    // ...
    Lucky,    // Lucky - Reroll one hit die per attack
    // ...
};

// combat_rules.cpp
static const char* const lucky_aliases[] = {"Lucky", "lucky"};

void lucky_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value) {
    // Handled during ROLL_HITS by setting a flag
    if (ext) {
        ext->reroll_one_miss = true;
    }
}

// Hot data
{RuleId::Lucky, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::ROLL_HITS),
 CombatType::BOTH, Target::ATTACKER, Trigger::ALWAYS, RulePriority::NORMAL,
 static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)},

// Cold data
{"Lucky", lucky_aliases, 2, "{attacker} uses Lucky", "Reroll one miss per attack"},

// Effect registration
registry.register_effects(RuleId::Lucky,
    EffectBuilder()
        .combat_effect(CombatSubPhase::ROLL_HITS, lucky_effect)
        .build());

// AI hints
case RuleId::Lucky:
    return AIHintBuilder().offensive_bonus().offense(10).consistent();
```

## Checklist

- [ ] Added RuleId to enum (correct position: primary vs extended)
- [ ] Added hot data with correct phase, sub-phase, and traits
- [ ] Added cold data with name, aliases, and description
- [ ] Implemented effect function
- [ ] Registered effect with EffectBuilder
- [ ] Added condition function (if needed)
- [ ] Added AI hints
- [ ] Added tests
- [ ] Verified all tests pass
