# Combat Resolution Phases

This document describes the combat resolution pipeline implemented by the
`RegistryCombatResolver` class. Combat resolution is divided into distinct
phases, each handling a specific aspect of the attack sequence.

## Phase Overview

Combat resolution follows this sequence:

```
1. PRE_ATTACK      - Weapon checks, mode selection
2. HIT_MODIFIERS   - Apply +/- to hit effects
3. ROLL_HITS       - Quality overrides, dice rolling
4. HIT_SEPARATION  - Rending/Rupture hit categorization
5. HIT_BONUSES     - Extra hit generation
6. HIT_MULTIPLICATION - Blast effects
7. DEFENSE_RESOLUTION - AP modifiers, defense rolls
8. WOUND_ALLOCATION - Regeneration, wound distribution
```

## Phase Details

### 1. PRE_ATTACK Phase

**Purpose:** Prepare for the attack, handle one-time effects.

**Rules Applied:**
- `Limited` - Check if one-use weapon is available
- `VersatileAttack` - Roll to choose AP+1 or +1 hit

**Context Required:**
- Attacker unit
- Weapon being used
- Combat type (shooting/melee)

### 2. HIT_MODIFIERS Phase

**Purpose:** Calculate total hit modifier before rolling.

**Rules Applied (Attacker):**
- `GoodShot` (+1 shooting)
- `BadShot` (-1 shooting)
- `VersatileAttack` (+1 if rolled 4-6)

**Rules Applied (Defender):**
- `Stealth` (-1 at >9")
- `RangedShrouding` (-1 shooting)
- `MeleeEvasion` (-1 melee)
- `MeleeShrouding` (-1 melee)

**Rules Applied (Weapon):**
- `Precise` (+1)
- `Thrust` (+1 when charging)
- `Purge` (+1 vs Tough 3+)

### 3. ROLL_HITS Phase

**Purpose:** Roll dice and determine hits.

**Rules Applied:**
- `Reliable` - Override quality to 2+

**Output:**
- Total hits
- Natural 6s count (for subsequent phases)

### 4. HIT_SEPARATION Phase

**Purpose:** Categorize hits for special treatment.

**Rules Applied:**
- `Rending` - Natural 6s get AP+4
- `Rupture` - Natural 6s bypass regen and deal +1 wound

**Note:** Rending and Rupture can stack. A natural 6 can trigger both
effects simultaneously.

### 5. HIT_BONUSES Phase

**Purpose:** Generate extra hits from natural 6s.

**Rules Applied:**
- `Relentless` - Extra hits on 6s (shooting >9")
- `Surge` - Extra hits on 6s (weapon)
- `PointBlankSurge` - Extra hits on 6s (shooting ≤9")
- `Furious` - Extra hits on 6s (charging)
- `PredatorFighter` - Recursive extra attacks on 6s (melee)

**Note:** Bonus hits are added to normal hits, not rending/rupture hits.

### 6. HIT_MULTIPLICATION Phase

**Purpose:** Multiply hits based on Blast value.

**Rules Applied:**
- `Blast(X)` - Multiply all hit types by X
- `Takedown` - Caps Blast multiplier at 1

**Capping:** Blast multiplier is capped at defender model count.

### 7. DEFENSE_RESOLUTION Phase

**Purpose:** Roll defense saves and determine wounds.

**AP Modifiers:**
- `Lance` (+2 AP when charging)
- `Thrust` (+1 AP when charging)
- `PiercingAssault` (minimum AP 1 when charging)
- `Protected` (6+ to reduce AP by 1)

**Defense Modifiers:**
- `Shielded` (+1 Defense vs non-spell)
- `ShieldWall` (+1 Defense in melee)

**Reroll Effects:**
- `Poison` - Defender rerolls 6s
- `Bane` - Defender rerolls 6s

**Wound Tracking:**
- `Shred` - Track defense 1s for bonus wounds

### 8. WOUND_ALLOCATION Phase

**Purpose:** Distribute wounds to models.

**Save Effects:**
- `Regeneration` - 5+ to ignore wounds (unless bypassed)
- `Resistance` - 6+ to ignore wounds

**Wound Distribution:**
- Normal allocation (non-hero first)
- `Takedown` - Target specific model
- `Hero` - Takes wounds last

**Wound Multiplication:**
- `Deadly(X)` - Each wound becomes X wounds (no carry-over)
- `Rupture` - Wounds from 6s deal +1 wound

**Counter Effects:**
- `SelfDestruct(X)` - Returns X hits when model dies in melee

## Trait Aggregation

At combat start, traits are aggregated from all applicable rules:

```cpp
// Example trait aggregation
if (weapon.has_rule(RuleId::Bane) ||
    weapon.has_rule(RuleId::Rending) ||
    weapon.has_rule(RuleId::Rupture)) {
    bypasses_regeneration = true;
}
```

Key trait flags:
- `BYPASSES_REGENERATION` - Regen saves don't apply
- `BYPASSES_RESISTANCE` - Resistance saves don't apply
- `FORCES_DEFENSE_REROLL` - Defender rerolls successful 6s

## Combat Type Restrictions

Some rules only apply in specific combat types:

**Shooting Only:**
- Stealth (at range)
- Relentless
- RangedShrouding
- GoodShot/BadShot

**Melee Only:**
- ShieldWall
- MeleeEvasion/MeleeShrouding
- Furious
- PredatorFighter
- SelfDestruct
- BaneInMelee

**Charge Only:**
- Impact
- Lance
- Furious
- Thrust
- PiercingAssault

## Performance Considerations

For optimal performance:

1. Use `CombatRuleCache` to avoid repeated rule lookups:
   ```cpp
   CombatRuleCache cache;
   cache.initialize(attacker, defender, weapon, combat_type, is_charge);
   if (cache.bypasses_regeneration()) { ... }
   ```

2. Use `CombatRuleCachePool` for parallel simulations:
   ```cpp
   CombatRuleCachePool<16> pool;
   CacheGuard guard(pool);
   if (guard) {
       guard->initialize(...);
   }
   ```

3. Pre-compute trait masks at combat start rather than checking
   individual rules repeatedly.
