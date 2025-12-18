# Special Rules Granting Analysis

This document analyzes all special rules that grant other special rules and provides implementation recommendations.

---

## Summary

Found **185+ rules** that grant other special rules, categorized into 5 types:

| Category | Count | Handling Approach |
|----------|-------|-------------------|
| Aura Rules | 82 | Grant rule to self/unit - process granted rule's effect |
| Buff Rules | 65 | Grant rule temporarily to friendly unit |
| Mark Rules | 25 | Grant effect conditionally against marked enemy |
| Debuff Rules | 6 | Apply negative effect to enemy unit |
| Boost Rules | 7 | Enhance existing faction rule's parameters |

---

## Category 1: Aura Rules

**Pattern:** `This model and its unit get [Rule Name]`

**Handling:** These rules simply grant another rule to the model and its unit. The engine should:
1. Add the granted rule to the unit's active rules list
2. Process the granted rule's effect when its trigger condition is met
3. No need to duplicate the effect logic - just grant the rule

### Example Rules

| Granting Rule | Grants |
|--------------|--------|
| Furious Aura | Furious |
| Fearless Aura | Fearless |
| Regeneration Aura | Regeneration |
| Evasive Aura | Evasive |
| Shielded Aura | Shielded |
| Counter-Attack Aura | Counter-Attack |
| Stealth Aura | Stealth |
| No Retreat Aura | No Retreat |

### Complete List (82 rules)

**Combat Auras:**
- Furious Aura → Furious
- Relentless Aura → Relentless
- Rending in Melee Aura → Rending in melee
- Rending when Shooting Aura → Rending when shooting
- Shred in Melee Aura → Shred in melee
- Shred when Shooting Aura → Shred when shooting
- Bane in Melee Aura → Bane in melee
- Bane when Shooting Aura → Bane when shooting
- Piercing Assault Aura → Piercing Assault
- Piercing Hunter Aura → Piercing Hunter
- Melee Slayer Aura → Melee Slayer
- Ranged Slayer Aura → Ranged Slayer
- Quick Shot Aura → Quick Shot
- Unpredictable Fighter Aura → Unpredictable Fighter
- Unpredictable Shooter Aura → Unpredictable Shooter
- Unstoppable in Melee Aura → Unstoppable in melee
- Unstoppable when Shooting Aura → Unstoppable when shooting
- Thrust in Melee Aura → Thrust in melee
- Ignores Cover Aura → Ignores Cover when shooting
- Indirect when Shooting Aura → Indirect when shooting

**Defense Auras:**
- Evasive Aura → Evasive
- Shielded Aura → Shielded
- Melee Evasion Aura → Melee Evasion
- Melee Shrouding Aura → Melee Shrouding
- Ranged Shrouding Aura → Ranged Shrouding
- Stealth Aura → Stealth
- Resistance Aura → Resistance
- Regeneration Aura → Regeneration
- Fortified Aura → Fortified
- Reanimation Aura → Reanimation
- Versatile Defense Aura → Versatile Defense

**Morale Auras:**
- Fearless Aura → Fearless
- Courage Aura → +1 to morale test rolls
- No Retreat Aura → No Retreat
- Steadfast Aura → Steadfast

**Movement Auras:**
- Ambush Aura → Ambush
- Infiltrate Aura → Infiltrate
- Scout Aura → Scout
- Teleport Aura → Teleport
- Bounding Aura → Bounding
- Strider Aura → Strider
- Swift Aura → Swift
- Rapid Advance Aura → Rapid Advance
- Rapid Rush Aura → Rapid Rush
- Hit & Run Fighter Aura → Hit & Run Fighter
- Hit & Run Shooter Aura → Hit & Run Shooter
- Speed Feat Aura → Speed Feat
- Versatile Reach Aura → Versatile Reach
- Counter-Attack Aura → Counter-Attack

**Precision Auras:**
- Precision Fighter Aura → +1 to hit rolls in melee
- Precision Charge Aura → +1 to hit rolls when charging
- Precision Shooter Aura → +1 to hit rolls when shooting
- Grounded Precision Aura → Grounded Precision
- Grounded Reinforcement Aura → Grounded Reinforcement
- Defensive Growth Aura → Defensive Growth

**Faction Boost Auras:**
- Changebound Boost Aura → Changebound Boost
- Clan Warrior Boost Aura → Clan Warrior Boost
- Devout Boost Aura → Devout Boost
- Ferocious Boost Aura → Ferocious Boost
- Guardian Boost Aura → Guardian Boost
- Guerrilla Boost Aura → Guerrilla Boost
- Harassing Boost Aura → Harassing Boost
- Havocbound Boost Aura → Havocbound Boost
- Highborn Boost Aura → Highborn Boost
- Hive Bond Boost Aura → Hive Bond Boost
- Hold the Line Boost Aura → Hold the Line Boost
- Infected Boost Aura → Infected Boost
- Lustbound Boost Aura → Lustbound Boost
- Machine-Fog Boost Aura → Machine-Fog Boost
- Mischievous Boost Aura → Mischievous Boost
- Plaguebound Boost Aura → Plaguebound Boost
- Rapid Blink Boost Aura → Rapid Blink Boost
- Scrapper Boost Aura → Scrapper Boost
- Scurry Boost Aura → Scurry Boost
- Self-Repair Boost Aura → Self-Repair Boost (implied)
- Sturdy Boost Aura → Sturdy Boost
- Targeting Visor Boost Aura → Targeting Visor Boost
- Warbound Boost Aura → Warbound Boost

---

## Category 2: Buff Rules

**Pattern:** `Pick [one/up to X] friendly unit(s) within [range]", which gets [Rule] once`

**Handling:** These rules grant another rule temporarily to a friendly unit. The engine should:
1. Apply a temporary buff that adds the granted rule
2. The buff expires after "next time the effect would apply"
3. Process the granted rule's effect when triggered

### Complete List (65 rules)

| Granting Rule | Grants | Max Targets |
|--------------|--------|-------------|
| Aura of Peace (1) | Fearless | 1 |
| Asphyxiating Fog (1) | Counter-Attack | 1 |
| Animate Spirit (1) | Hit & Run Fighter | 1 |
| Cyborg Assault (1) | Hit & Run Shooter | 1 |
| Elder Protection (1) | Resistance | 1 |
| Spirit Power (1) | Flying | 1 |
| Insidious Protection (1) | Grounded Reinforcement | 1 |
| Psy-Adrenaline (1) | Harassing Boost | 1 |
| Psy-Hunter (1) | Scrapper Boost | 1 |
| Psy-Injected Courage (1) | Hold the Line Boost | 1 |
| Violent Onslaught (1) | Infected Boost | 1 |
| Weapon Booster (1) | Scurry Boost | 1 |
| Armor Rune (2) | Sturdy Boost | 2 |
| Blessed Ammo (2) | Shred when shooting | 2 |
| Blessed Virus (2) | Rapid Rush | 2 |
| Blissful Dance (2) | Melee Evasion | 2 |
| Breath of Change (2) | Bane when Shooting | 2 |
| Burst of Rage (2) | Furious | 2 |
| Dark Assault (2) | Shred in melee | 2 |
| Fateful Guidance (2) | Furious | 2 |
| Fiery Protection (2) | Melee Evasion | 2 |
| Focused Defender (2) | Unpredictable Fighter | 2 |
| Hidden Spirits (2) | Unpredictable Shooter | 2 |
| Holy Rage (2) | Piercing Hunter | 2 |
| Infuse Bloodthirst (2) | Hive Bond Boost | 2 |
| Inspiring Bots (2) | Rapid Advance | 2 |
| Path of War (2) | Ferocious Boost | 2 |
| Psychic Stabilization (2) | Targeting Visor Boost | 2 |
| Righteous Fury (2) | Piercing Assault | 2 |
| Shadow Dance (2) | Rapid Blink Boost | 2 |
| Shrouding Incense (2) | Machine-Fog Boost | 2 |
| Spirit Resolve (2) | Clan Warrior Boost | 2 |
| Bio-Displacer (3) | Teleport | 3 |
| Blessing of Souls (3) | Highborn Boost | 3 |
| Blood Dome (3) | Evasive | 3 |
| Celestial Roar (3) | Primal Boost | 3 |
| Change Boon (3) | Changebound Boost | 3 |
| Dark Dome (3) | Evasive | 3 |
| Enhance Serum (3) | Regeneration | 3 |
| Fade in the Dark (3) | Stealth | 3 |
| Guardian Protection (3) | Guardian Boost | 3 |
| Havoc Boon (3) | Havocbound Boost | 3 |
| Knight Dome (3) | Evasive | 3 |
| Litanies of War (3) | Devout Boost | 3 |
| Lust Boon (3) | Lustbound Boost | 3 |
| Mending Bots (3) | Self-Repair Boost | 3 |
| Plague Boon (3) | Plaguebound Boost | 3 |
| Power Field (3) | Shielded | 3 |
| Protective Dome (3) | Evasive | 3 |
| Rapid Mutation (3) | Regeneration | 3 |
| War Boon (3) | Warbound Boost | 3 |
| Watch Dome (3) | Evasive | 3 |
| Wolf Dome (3) | Evasive | 3 |
| Battle Rune (1) | +6" range when shooting | 1 |
| Increased Shooting Range Buff | +6" range when shooting | 1 |
| Entrenched Buff | Entrenched | 1 |
| Guarded Buff | Guarded | 1 |
| No Retreat Buff | No Retreat | 1 |
| Regeneration Buff | Regeneration | 1 |
| Steadfast Buff | Steadfast | 1 |
| Stealth Buff | Stealth | 1 |
| Swift Buff | Swift | 1 |
| Rapid Advance Buff | Rapid Advance | 1 |
| Primal Boost Buff | Primal Boost | 1 |
| Self-Repair Boost Buff | Self-Repair Boost | 1 |

---

## Category 3: Mark Rules

**Pattern:** `Pick [one/up to X] enemy unit(s) within [range]", which friendly units gets [Rule] against once`

**Handling:** These rules mark an enemy unit, giving friendly units a bonus when attacking it. The engine should:
1. Place a "mark" on the target enemy unit
2. When any friendly unit attacks the marked unit, apply the granted rule
3. Remove the mark after "next time the effect would apply"

### Complete List (25 rules)

| Granting Rule | Grants Against Target | Max Targets |
|--------------|----------------------|-------------|
| Bane Mark | Bane | 1 |
| Toxin Mist (1) | Bane | 1 |
| Furious Mark | Furious | 1 |
| Bad Omen (2) | Furious | 2 |
| Indirect Mark | Indirect | 1 |
| Triangulation Bots (1) | Indirect | 1 |
| Quick Shot Mark | Quick Shot | 1 |
| Combat Ecstasy (1) | Quick Shot | 1 |
| Relentless Mark | Relentless | 1 |
| Calculated Foresight (2) | Relentless | 2 |
| Rending in Melee Mark | Rending in melee | 1 |
| Head Bang (3) | Rending in melee | 3 |
| Shred Mark | Shred | 1 |
| The Founder's Curse (1) | Shred | 1 |
| Slayer Mark | Slayer | 1 |
| Veil of Madness (3) | Slayer | 3 |
| Unpredictable Fighter Mark | Unpredictable Fighter | 1 |
| Terror Seeker (3) | Unpredictable Fighter | 3 |
| Unstoppable Shooting Mark | Unstoppable when shooting | 1 |
| Advanced Sight (1) | Unstoppable when shooting | 1 |
| Blood Sight (1) | Unstoppable when shooting | 1 |
| Dark Sight (1) | Unstoppable when shooting | 1 |
| Knight Sight (1) | Unstoppable when shooting | 1 |
| Watch Sight (1) | Unstoppable when shooting | 1 |
| Wolf Sight (1) | Unstoppable when shooting | 1 |
| Increased Shooting Range Mark | +6" range when shooting | 1 |
| Eternal Guidance (3) | +6" range when shooting | 3 |

---

## Category 4: Debuff Rules

**Pattern:** `Pick [one/up to X] enemy unit(s) within [range]", which gets [negative effect] once`

**Handling:** These rules apply a negative effect directly to an enemy unit. The engine should:
1. Apply the debuff to the target enemy unit
2. The debuff expires after "next time the effect would apply"

### Complete List (6 rules)

| Granting Rule | Debuff Applied |
|--------------|----------------|
| Burn the Heretic (1) | -3 to casting rolls |
| Shifting Form (1) | -3 to casting rolls |
| Casting Debuff | -1 to casting rolls |
| Morale Debuff | -1 to morale test rolls |
| Creator of Illusions (1) | Unwieldy in melee |
| Unwieldy Debuff | Unwieldy in melee |

---

## Category 5: Boost Rules

**Pattern:** `If [this model/all models] has [Base Rule], [enhanced effect] (instead of [standard effect])`

**Handling:** These rules enhance an existing faction rule. The engine should:
1. Check if the model/unit has the base rule
2. If boost is present, modify the base rule's parameters
3. Apply the enhanced effect instead of the standard effect

### Complete List (7 rules)

| Boost Rule | Base Rule | Standard Effect | Enhanced Effect |
|-----------|-----------|-----------------|-----------------|
| Changebound Boost | Changebound | -1 to hit when shot/charged from >9" | -1 to hit always |
| Guardian Boost | Guardian | AP(-1) when shot/charged from >9" | AP(-1) always |
| Devout Boost | Devout | Extra hits on 6s | Extra hits on 5-6s |
| Ferocious Boost | Ferocious | Extra hits on 6s | Extra hits on 5-6s |
| Lustbound Boost | Lustbound | +1"/+3" movement | +2"/+6" movement |
| Plaguebound Boost | Plaguebound | Ignore wounds on 6+ | Ignore wounds on 5-6 |
| Warbound Boost | Warbound | Extra wound on defense 1s | Extra wound on 1-2s |

---

## Missing Base Rule Definitions

The following rules are granted by Aura/Buff/Mark rules but have no standalone definition in the matrix:

| Rule Name | Inferred Effect | Needs Definition |
|-----------|-----------------|------------------|
| Entrenched | Defense buff (likely +1 defense or ignore some AP) | YES |
| Guarded | Defense buff (likely AP reduction) | YES |
| Quick Shot | Shooting buff (likely extra shot or +1 hit) | YES |
| Rapid Advance | Movement buff (likely +X" to advance) | YES |
| Rapid Rush | Movement buff (likely +X" to rush/charge) | YES |
| Piercing Hunter | Attack buff vs certain targets (likely AP bonus) | YES |
| Ranged Slayer | AP(+2) against Tough(3+) when shooting | Defined as Melee Slayer variant |

---

## Implementation Recommendations

### For Aura and Buff Rules (Categories 1-2):
1. **Just grant the rule** - Don't duplicate effect logic
2. The granted rule should be processed by its own handler
3. Track the source of the granted rule for debugging
4. Handle expiration for temporary buffs

### For Mark Rules (Category 3):
1. Store marks on the enemy unit with the associated effect
2. When attacking, check if target is marked
3. Apply the marked effect during the appropriate phase
4. Remove mark after use

### For Debuff Rules (Category 4):
1. Apply as a negative modifier to the enemy unit
2. Track expiration (usually "once")
3. Process during the relevant phase

### For Boost Rules (Category 5):
1. Check for boost presence when processing base rule
2. Modify the base rule's trigger threshold or value
3. Use parameter modification rather than separate handlers

---

## Engine Architecture Notes

The granting system should follow a layered approach:

```
Granting Rule (e.g., Furious Aura)
    ↓
Grants: Add "Furious" to unit's active rules
    ↓
Furious Rule Handler processes when trigger condition met
    ↓
Effect: Extra hits on 6s when charging
```

This avoids duplicating effect logic and keeps rule definitions modular.
