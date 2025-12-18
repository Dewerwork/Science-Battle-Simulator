# Special Rules Review Matrix

This document provides a comprehensive matrix for reviewing how special rules should be processed by the combat engine. Review each rule's phase applicability, trigger conditions, and effects before implementation.

---

## Legend

### Phase Columns
- **DEP** = Deployment phase
- **MOV** = Movement phase
- **CHG** = Charge declaration/movement
- **SHT** = Shooting phase
- **MEL** = Melee phase
- **MOR** = Morale check
- **RND** = Round start/end

### Sub-Step Columns (within combat resolution)
- **ATK** = Attack declaration (choosing targets)
- **HIT** = Hit roll (Quality test)
- **DEF** = Defense roll (Save test)
- **WND** = Wound calculation/modification
- **ALC** = Wound allocation to models
- **RGN** = Regeneration rolls

### Effect Columns
- **Grants** = What the rule provides (modifier, extra dice, etc.)
- **Condition** = When/how the effect triggers
- **Value** = Numeric value if applicable
- **Target** = Who is affected (Self, Enemy, Weapon)

### Implementation Status
- **IMP** = Currently implemented in codebase
- ✅ = Fully implemented
- ⚠️ = Partially implemented
- ❌ = Not implemented

---

## WEAPON RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **AP(X)** | | | | Y | Y | | | | | Y | | | | Defense penalty | Always on weapon attack | X | Enemy | ✅ |
| **Blast(X)** | | | | Y | Y | | | | | | Y | | | Multiply hits | After hit roll | X (capped by models) | Enemy | ✅ |
| **Deadly(X)** | | | | Y | Y | | | | | | Y | | | Multiply wounds | After wound calc | X | Enemy | ✅ |
| **Lance** | | | Y | | Y | | | | | Y | | | | +2 AP | When charging | +2 | Weapon | ✅ |
| **Poison** | | | | Y | Y | | | | | Y | | | | Reroll defense 6s | On defense roll | - | Enemy | ✅ |
| **Precise** | | | | Y | Y | | | | Y | | | | | +1 to hit | Always | +1 | Weapon | ✅ |
| **Reliable** | | | | Y | Y | | | | Y | | | | | Quality becomes 2+ | Always | 2+ | Weapon | ✅ |
| **Rending** | | | | Y | Y | | | | Y | Y | | | | AP(4) | On unmodified 6 to hit | AP4 | Enemy | ⚠️ |
| **Bane** | | | | Y | Y | | | | | | | | Y | Bypass regeneration | On wound | - | Enemy | ✅ |
| **Impact(X)** | | | Y | | Y | | | | Y | | | | | X extra attacks | On charge only | X | Self | ⚠️ |
| **Indirect** | | | | Y | | | | | | Y | | | | Ignore cover | Always | -1 cover | Enemy | ⚠️ |
| **Sniper** | | | | Y | | | | Y | | | | | | Pick target model | When declaring | - | Enemy | ❌ |
| **Lock-On** | | | | Y | Y | | | | Y | | | | | +1 to hit vs vehicles | vs Vehicle keyword | +1 | Self | ❌ |
| **Purge** | | | | Y | Y | | | | Y | | | | | +1 to hit vs Tough(3+) | vs Tough(3+) | +1 | Self | ❌ |
| **Limited** | | | | Y | Y | | | Y | | | | | | Use once per game | First use only | 1 | Weapon | ❌ |
| **Linked** | | | | Y | Y | | | | Y | | | | | Only usable with another weapon | With paired weapon | - | Weapon | ❌ |

---

## ATTACK MODIFIER RULES (Unit/Model)

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Furious** | | | Y | | Y | | | | | | Y | | | +Sixes as extra hits | When charging | +6s | Self | ✅ |
| **PredatorFighter** | | | | | Y | | | | Y | | | | | 6s generate more attacks | On melee 6s | Recursive | Self | ✅ |
| **Relentless** | | | | Y | | | | | Y | | | | | 6s = extra hits | Shooting >9" | +6s | Self | ⚠️ |
| **GoodShot** | | | | Y | | | | | Y | | | | | +1 to hit | Shooting only | +1 | Self | ✅ |
| **BadShot** | | | | Y | | | | | Y | | | | | -1 to hit | Shooting only | -1 | Self | ✅ |
| **Surge** | | | | Y | Y | | | | Y | | | | | 6s to hit = +1 hit | On 6 to hit | +1 | Self | ❌ |
| **Thrust** | | | Y | | Y | | | | Y | Y | | | | +1 hit, +1 AP | When charging | +1/+1 | Self | ❌ |
| **VersatileAttack** | | | | Y | Y | | | Y | Y | Y | | | | Choose: AP+1 or +1 hit | Before attack | +1 | Self | ✅ |
| **PointBlankSurge** | | | | Y | | | | | Y | | | | | 6s at 0-9" = extra hit | Shooting 0-9" | +1 | Self | ❌ |

---

## DEFENSE MODIFIER RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Tough(X)** | | | | Y | Y | | | | | | | Y | | X wounds to kill | Model property | X | Self | ✅ |
| **Regeneration** | | | | Y | Y | | | | | | | | Y | 5+ ignore wound | On each wound | 5+ | Self | ✅ |
| **Shielded** | | | | Y | Y | | | | | Y | | | | +1 Defense | vs non-spell | +1 | Self | ✅ |
| **ShieldWall** | | | | | Y | | | | | Y | | | | +1 Defense | In melee only | +1 | Self | ✅ |
| **Stealth** | | | | Y | | | | | | Y | | | | -1 to be hit | From >12" away | -1 enemy | Self | ❌ |
| **MeleeEvasion** | | | | | Y | | | | | Y | | | | +1 Defense | In melee only | +1 | Self | ✅ |
| **MeleeShrouding** | | | | | Y | | | | | Y | | | | +1 Defense | In melee only | +1 | Self | ✅ |
| **RangedShrouding** | | | | Y | | | | | | Y | | | | +1 Defense | When shot | +1 | Self | ✅ |
| **Resistance** | | | | Y | Y | | | | | | | | Y | 6+ ignore wound (2+ vs spell) | After wound | 6+/2+ | Self | ✅ |
| **Protected** | | | | Y | Y | | | | | Y | | | | 6+ reduce AP by 1 | Before defense roll | 6+ | Self | ❌ |

---

## WOUND MODIFIER RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Rupture** | | | | Y | Y | | | | | | Y | | Y | +1 wound per 6, ignore regen | On 6s to hit | +1 | Enemy | ✅ |
| **Shred** | | | | Y | Y | | | | | | Y | | | +1 wound per 1 to defend | On 1s to defend | +1 | Enemy | ✅ |

---

## WOUND ALLOCATION RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Hero** | | | | Y | Y | | | | | | | Y | | Takes wounds last | Allocation order | Last | Self | ✅ |
| **Takedown** | | | | Y | Y | | | Y | | | | Y | | Target single model | Declaration | 1 model | Enemy | ❌ |

---

## REGENERATION BYPASS RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Bane** | | | | Y | Y | | | | | | | | Y | No regeneration | On wounds dealt | - | Enemy | ✅ |
| **Rupture** | | | | Y | Y | | | | | | Y | | Y | No regeneration | On wounds dealt | - | Enemy | ✅ |
| **Unstoppable** | | | | Y | Y | | | | | | | | Y | Ignores enemy regen | On wounds dealt | - | Enemy | ⚠️ |

---

## MORALE RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Fearless** | | | | | | Y | | | | | | | | Reroll failed morale | On morale failure | Reroll | Self | ✅ |
| **MoraleBoost** | | | | | | Y | | | | | | | | +1 to morale tests | Always | +1 | Self | ✅ |
| **NoRetreat** | | | | | | Y | | | | | | | | Take wounds instead of Shaken | On morale failure | wounds | Self | ✅ |
| **Battleborn** | | | | | | | Y | | | | | | | 4+ to rally from Shaken | Round start | 4+ | Self | ✅ |
| **Fear(X)** | | | | | Y | Y | | | | | | | | Count as +X wounds | In melee morale | +X | Self | ❌ |

---

## COMBAT ORDER RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Counter** | | | | | Y | | | Y | | | | | | Strike first when charged | When receiving charge | First | Self | ❌ |
| **HitAndRun** | | | | | Y | | Y | | | | | | | Retreat after combat | End of melee | - | Self | ❌ |
| **SelfDestruct** | | | | | Y | | | | | | | Y | | Deal X hits when killed | On model death | X | Enemy | ❌ |

---

## MOVEMENT/DEPLOYMENT RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Scout** | Y | | | | | | | | | | | | | Deploy 12" ahead | Deployment | +12" | Self | ❌ |
| **Ambush** | Y | | | | | | | | | | | | | Deploy >9" from enemy | Deployment | Reserve | Self | ❌ |
| **Fast** | | Y | Y | | | | | | | | | | | 9" move | Movement | 9" | Self | ❌ |
| **Slow** | | Y | Y | | | | | | | | | | | 4" move | Movement | 4" | Self | ❌ |
| **Agile** | | Y | Y | | | | | | | | | | | +1" advance, +2" rush/charge | Movement | +1/+2 | Self | ❌ |
| **Flying** | | Y | Y | | | | | | | | | | | Ignore terrain/units | Movement | - | Self | ❌ |
| **Strider** | | Y | Y | | | | | | | | | | | Ignore difficult terrain | Movement | - | Self | ❌ |
| **RapidCharge** | | | Y | | | | | | | | | | | +4" charge range | Charge only | +4" | Self | ❌ |

---

## MAGIC/SPECIAL RULES

| Rule | DEP | MOV | CHG | SHT | MEL | MOR | RND | ATK | HIT | DEF | WND | ALC | RGN | Grants | Condition | Value | Target | IMP |
|------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|--------|-----------|-------|--------|-----|
| **Casting(X)** | | | | | | | Y | | | | | | | Cast X spells per round | Casting phase | X | Self | ❌ |
| **Devout** | | | | | | | | | | | | | | Faction rule | Varies | - | Self | ❌ |

---

## COMBAT RESOLUTION FLOW

For reference, here's where each rule type applies in the combat sequence:

```
ROUND START
├── Battleborn check (4+ to rally)
│
ATTACK DECLARATION
├── VersatileAttack choice (AP+1 or +1 hit)
├── Sniper target selection
├── Takedown target selection
├── Counter rule (swap initiative)
│
HIT ROLL (Quality Test)
├── Base Quality from model
├── Reliable → Quality becomes 2+
├── Precise → +1 to Quality roll
├── GoodShot → +1 (shooting only)
├── BadShot → -1 (shooting only)
├── Shaken → -1 to hit
├── Lock-On → +1 vs vehicles
├── Purge → +1 vs Tough(3+)
├── Track 6s rolled (for Furious, Rending, Rupture, etc.)
│
HIT MODIFIERS
├── Furious → +sixes when charging
├── Blast(X) → multiply by min(X, models)
├── Impact(X) → +X attacks on charge
├── Relentless → +sixes when shooting >9"
├── Surge → +1 per 6
├── PointBlankSurge → +1 per 6 at 0-9"
├── PredatorFighter → recursive attacks on 6s
│
CALCULATE AP
├── Base AP from weapon
├── Lance → +2 when charging
├── Thrust → +1 when charging
├── Rending → AP(4) on 6s to hit
├── VersatileAttack → +1 if chosen
├── Protected → 6+ to reduce AP by 1
│
DEFENSE ROLL (Save Test)
├── Base Defense from model
├── Cover → +1
├── Shielded → +1 vs non-spell
├── ShieldWall → +1 in melee
├── MeleeEvasion → +1 in melee
├── MeleeShrouding → +1 in melee
├── RangedShrouding → +1 vs shooting
├── Stealth → +1 from >12"
├── Defender Shaken → -1
├── Indirect → ignore cover
├── Poison → reroll 6s
├── Track 1s rolled (for Shred)
│
WOUND CALCULATION
├── Wounds = Hits - Saves
├── Deadly(X) → multiply by X
├── Rupture → +1 per 6 to hit
├── Shred → +1 per 1 to defend
│
RESISTANCE
├── 6+ to ignore each wound
├── 2+ vs spells
│
WOUND ALLOCATION
├── Sort by: Non-tough → Tough (most wounded) → Heroes
├── Check Tough(X) thresholds
│
REGENERATION
├── 5+ to ignore each wound
├── Bypassed by: Bane, Rupture, Unstoppable
│
MODEL DEATH
├── SelfDestruct → deal X hits to attacker
│
MORALE CHECK
├── Base Morale value
├── MoraleBoost → +1
├── At half strength → penalty
├── Lost melee → penalty
├── Fear(X) → count as +X wounds
├── Fearless → reroll failure
├── NoRetreat → take wounds instead
│
ROUND END
├── HitAndRun → retreat option
```

---

## REVIEW CHECKLIST

For each rule, verify:

- [ ] **Phase correct?** Does the Y marker match when this rule should trigger?
- [ ] **Sub-step correct?** Is the rule applying at the right point in combat resolution?
- [ ] **Grants correct?** Is the effect description accurate?
- [ ] **Condition correct?** Are the trigger conditions complete?
- [ ] **Value correct?** Are numeric values accurate?
- [ ] **Target correct?** Does it affect the right unit (self vs enemy)?
- [ ] **Interactions?** How does this rule interact with others in same phase/step?

---

## NOTES FOR REVIEW

### Rules needing clarification:
1. **Rending** - Currently tracked but not fully applied. Need to confirm: Does AP(4) replace existing AP or add to it?
2. **Impact(X)** - Handled in CombatEngine but not CombatResolver. Needs consolidation.
3. **Stealth** - Distance-based. How to handle in simplified scenarios?
4. **Counter** - Strike order change. Need to implement initiative system.

### Rules with complex interactions:
1. **Poison + Rending** - Poison rerolls 6s, but Rending triggers on 6s. Which resolves first?
2. **Deadly + Rupture** - Both multiply wounds. Order of operations?
3. **Bane + Regeneration + Resistance** - Multiple bypass/save layers.

### Rules not simulated (no movement system):
- Scout, Ambush, Fast, Slow, Agile, Flying, Strider, RapidCharge
- Stealth (distance-based)
- HitAndRun (retreat mechanics)

