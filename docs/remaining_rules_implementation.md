# Remaining Special Rules Implementation Guide

This document details the special rules that are defined in the `RuleId` enum but not yet implemented in the combat engine. Each rule is categorized by the systems or changes required for implementation.

**Recently Implemented:**
- ✅ Purge - +1 to hit vs Tough(3+)
- ✅ VersatileAttack - Dice roll chooses AP+1 or +1 hit
- ✅ Shred - Each unmodified 1 on defense = +1 wound, bypasses regen
- ✅ Rupture - 6s to hit cause +1 wound per wound, bypasses regen
- ✅ SelfDestruct - When model dies, deal X hits back to attacker
- ✅ Limited - Weapon can only be used once per game
- ✅ Takedown - Target specific model, Blast capped at 1, excess wounds don't carry over
- ✅ HitAndRun - After melee, unit can disengage and move away full move distance

**Removed (no faction data):**
- ❌ Lock-On - No units in current data use this rule
- ❌ Sniper - Removed from scope

---

## Summary

| Category | Rules | Complexity |
|----------|-------|------------|
| **Wound Tracking** | Rupture, Shred | Medium |
| **Targeting** | Sniper, Takedown | Medium |
| **Weapon State** | Limited, Linked | Medium |
| **Post-Combat** | HitAndRun, SelfDestruct | Medium |
| **Deployment** | Scout, Ambush | High (new system) |
| **Terrain** | Flying, Strider, Indirect | High (new system) |
| **Magic** | Casting, Devout | High (new system) |

---

## Category 1: Wound Tracking Rules (Medium Complexity)

These require tracking specific die roll results through the combat resolution.

### Rupture
**Description**: Wounds caused by unmodified 6s to hit ignore regeneration and cause +1 wound

**Requirements**:
- Track which wounds came from 6s to hit (separate from Rending)
- Apply +1 wound multiplier for those specific hits
- Bypass regeneration for those wounds

**Current Challenge**: The combat flow doesn't track individual hit results through to wound application. Hits are aggregated into a total.

**Proposed Solution**:
```cpp
struct HitResult {
    u32 normal_hits = 0;
    u32 rending_hits = 0;  // 6s with Rending weapon
    u32 rupture_hits = 0;  // 6s with Rupture weapon
};

// Track separately through defense rolls
// Rupture wounds = rupture_hits that failed defense * 2 (the +1 wound)
// These wounds bypass regeneration
```

**Implementation Location**: `combat_engine.hpp` - requires refactoring hit/wound tracking

---

### Shred
**Description**: Each unmodified 1 rolled on defense causes +1 wound

**Requirements**:
- Track 1s rolled during defense test
- Add bonus wounds equal to number of 1s

**Current Challenge**: `roll_defense_test()` in `dice.hpp` doesn't return the count of 1s rolled.

**Proposed Solution**:
```cpp
// In dice.hpp, modify DefenseResult:
struct DefenseResult {
    u32 saves = 0;
    u32 wounds = 0;
    u32 ones_rolled = 0;  // NEW: for Shred
};

// In combat_engine.hpp:
if (w.has_rule(RuleId::Shred)) {
    total_wounds += def_result.ones_rolled;
    if (logger_) logger_->on_rule_triggered("Shred", "bonus_wounds_from_1s", def_result.ones_rolled);
}
```

**Implementation Location**: `dice.hpp` and `combat_engine.hpp`

---

## Category 2: Targeting Rules (Medium Complexity)

These affect wound allocation rather than combat resolution.

### Sniper
**Description**: May pick which model in the target unit takes the wounds

**Requirements**:
- Override normal wound allocation order
- Allow attacker to specify target model index
- AI logic to pick optimal target (e.g., special weapon carriers, heroes)

**Proposed Solution**:
```cpp
// New parameter in apply_wounds:
WoundResult apply_wounds(UnitView unit, u32 wounds, bool bypass_regen,
                         i8 sniper_target = -1) {  // -1 = normal allocation
    if (sniper_target >= 0 && unit.model_is_alive(sniper_target)) {
        // Apply all wounds to specified model first
        // Excess wounds go to normal allocation
    }
    // ... rest of allocation
}
```

**Implementation Location**: `combat_engine.hpp` - `apply_wounds()` function

---

### Takedown
**Description**: Pick one enemy model, resolve attack as if targeting unit of 1

**Requirements**:
- Similar to Sniper but more restrictive
- Target model treated as solo unit for hit/wound calculations
- Blast capped at 1, other AoE effects limited

**Proposed Solution**: Implement as variant of Sniper with additional restrictions on multiplier effects.

**Implementation Location**: `combat_engine.hpp`

---

## Category 3: Weapon State Rules (Medium Complexity)

These require tracking weapon/attack state across activations.

### Limited
**Description**: This weapon may only be used once per game

**Requirements**:
- Track weapon usage per unit per game (not per round)
- Skip weapon in attack loop if already used
- Reset at match start

**Proposed Solution**:
```cpp
// In UnitState struct:
u64 limited_weapons_used = 0;  // Bitmask of weapon indices used

// In combat resolution:
if (w.has_rule(RuleId::Limited)) {
    if (unit.state->limited_weapons_used & (1 << weapon_idx)) {
        continue;  // Skip this weapon
    }
    unit.state->limited_weapons_used |= (1 << weapon_idx);
}
```

**Implementation Location**: `sim_state.hpp` and `combat_engine.hpp`

---

### Linked
**Description**: This weapon can only be used if another specific weapon is also used

**Requirements**:
- Define weapon pairing (either by name or index)
- Check if paired weapon was/is being used in same activation

**Current Challenge**: No weapon pairing metadata exists.

**Proposed Solution**: Add `linked_weapon_name` field to Weapon struct, or use naming convention (e.g., "Linked Rifle" pairs with any "Rifle").

**Implementation Location**: `types.hpp` (Weapon struct) and `combat_engine.hpp`

---

### VersatileAttack
**Description**: Before attacking, choose either AP+1 or +1 to hit

**Requirements**:
- Pre-attack decision point
- AI logic to choose optimal option based on defender stats
- Track choice for the activation

**Proposed Solution**:
```cpp
// Before attack resolution:
enum class VersatileChoice { None, AP_Bonus, Hit_Bonus };

VersatileChoice choose_versatile(UnitView attacker, UnitView defender) {
    // If defender has high defense, AP+1 is better
    // If defender has low defense, +1 to hit is better
    return (defender.defense() >= 4) ? VersatileChoice::AP_Bonus
                                      : VersatileChoice::Hit_Bonus;
}
```

**Implementation Location**: `combat_engine.hpp` or `ai_controller.hpp`

---

## Category 4: Post-Combat Rules (Medium Complexity)

These trigger after combat resolution.

### HitAndRun
**Description**: After fighting in melee, this unit may move away

**Requirements**:
- Post-melee movement phase
- Break engagement without enemy getting attacks
- Movement distance (typically move speed)

**Proposed Solution**:
```cpp
// In game_runner.hpp, after melee resolution:
if (attacker.has_rule(RuleId::HitAndRun) && !attacker.is_destroyed()) {
    // Move attacker away from defender
    i8 retreat_distance = state_.get_move_speed(*attacker.unit);
    // Update position
    state_.in_melee = false;
    if (logger_) logger_->on_rule_triggered("HitAndRun", "retreated", retreat_distance);
}
```

**Implementation Location**: `game_runner.hpp` - `resolve_melee_round()` function

---

### SelfDestruct
**Description**: When this model is killed, deal X hits to the enemy unit

**Requirements**:
- On-death trigger for each model
- Apply hits to attacker (reversed direction)
- Value X from rule

**Proposed Solution**:
```cpp
// In apply_wounds, when model dies:
if (unit.has_rule(RuleId::SelfDestruct)) {
    u8 destruct_value = unit.get_rule_value(RuleId::SelfDestruct);
    // Queue damage to be applied to attacker
    pending_self_destruct_hits += destruct_value;
}

// After apply_wounds returns, apply pending hits to attacker
```

**Current Challenge**: Need reference to attacker in wound application, or return pending effects.

**Implementation Location**: `combat_engine.hpp`

---

## Category 5: Deployment Rules (High Complexity - New System Required)

These require a deployment phase that doesn't currently exist.

### Scout
**Description**: After deployment, may move up to 12" (before game starts)

**Requirements**:
- Pre-game deployment phase
- Position adjustment after initial placement
- Distance from deployment zone tracking

**System Needed**: Deployment phase with zone definitions and placement order.

---

### Ambush
**Description**: May be placed in reserve and deployed anywhere on the battlefield more than 9" from enemy units

**Requirements**:
- Reserve tracking
- Mid-game deployment
- Distance validation from all enemy units

**System Needed**: Reserve system with deployment triggers.

---

## Category 6: Terrain Rules (High Complexity - New System Required)

These require terrain/cover mechanics.

### Indirect
**Description**: May target enemies out of line of sight, ignores cover bonuses

**Requirements**:
- Line of sight system
- Cover system with defense bonuses
- Ability to bypass both

**Current State**: No cover system exists. Stealth is distance-based, not cover-based.

**System Needed**: Cover/terrain system with LOS calculations.

---

### Flying
**Description**: Ignores terrain and units during movement

**Requirements**:
- Terrain system with blocking/difficult terrain
- Movement path validation
- Flying units bypass these checks

**System Needed**: Terrain grid with movement cost/blocking.

---

### Strider
**Description**: Ignores difficult terrain penalties

**Requirements**:
- Difficult terrain zones
- Movement penalty system
- Strider bypasses penalties only (not blocking)

**System Needed**: Same terrain system as Flying, but simpler check.

---

## Category 7: Magic Rules (High Complexity - New System Required)

These require a spell/psychic phase.

### Casting(X)
**Description**: This model may cast X spells per round

**Requirements**:
- Spell phase in game loop
- Spell list with effects
- Casting mechanics (rolls, deny, etc.)
- Spell range and targeting

**System Needed**: Complete magic system with spell definitions.

---

### Devout
**Description**: Faction-specific rule (effects vary)

**Requirements**:
- Faction identification
- Faction-specific rule effects
- Varies by army book

**System Needed**: Faction system with rule variations.

---

## Implementation Priority

Based on complexity and impact:

### Phase 1 (Combat Enhancements)
1. **Rupture** - Wound tracking refactor
2. **Shred** - Defense roll tracking
3. **Sniper/Takedown** - Wound allocation override
4. **Limited** - Weapon state tracking

### Phase 2 (Post-Combat)
5. **SelfDestruct** - On-death effects
6. **HitAndRun** - Post-melee movement

### Phase 3 (New Systems)
7. **Indirect/Flying/Strider** - Terrain system
8. **Scout/Ambush** - Deployment system
9. **Casting/Devout** - Magic system
10. **Linked** - Weapon pairing

---

## Files to Modify

| File | Rules Affected |
|------|----------------|
| `combat_engine.hpp` | Rupture, Shred, Sniper, Takedown, Limited, SelfDestruct |
| `dice.hpp` | Shred (return 1s count) |
| `game_runner.hpp` | HitAndRun |
| `sim_state.hpp` | Limited (weapon usage tracking) |
| `types.hpp` | Linked (weapon pairing) |
| `game_state.hpp` | Flying, Strider, Scout, Ambush (if terrain/deployment added) |
| NEW: `spell_system.hpp` | Casting, Devout |
| NEW: `terrain.hpp` | Indirect, Flying, Strider |
| NEW: `deployment.hpp` | Scout, Ambush |
