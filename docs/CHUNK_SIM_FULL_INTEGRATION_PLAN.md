# Full Rule Architecture Integration Plan for chunk_sim

## Executive Summary

This document outlines the work needed to make chunk_sim fully utilize the rule architecture defined in `RULE_ARCHITECTURE_IMPLEMENTATION_PLAN.md`. The goal is to migrate all hardcoded rule logic to the registry-based system, unify the two separate rule registries, integrate all 185+ faction rules, add deployment simulation, and enable rule-aware AI.

---

## Scope Decisions

| Decision | Answer | Impact |
|----------|--------|--------|
| Deployment Phase | ✅ Yes | Full deployment simulation required |
| Rule-Aware AI | ✅ Yes | AI uses rule hints for decisions |
| Faction Rules | ✅ All 185+ | Complete faction rule integration |
| Backwards Compatible | ❌ No | Can refactor freely for correctness |
| Timeline | None | Quality over speed |

---

## Current State Assessment

### What's Working

- Foundation data structures (Phase 1) - fully implemented
- Rule Registry (Phase 2) - fully implemented
- Combat resolution uses `RegistryCombatResolver` with phase-based dispatch

### Gaps Identified

| Gap | Location | Impact |
|-----|----------|--------|
| Two separate rule registries | `FactionRulesRegistry` vs `RuleRegistry` | Faction rules don't integrate with combat |
| Hardcoded movement logic | `GameRunner::execute_advance/rush/charge` | Movement rules not registry-based |
| Hardcoded end-round effects | `GameRunner::check_battleborn`, morale in `execute_melee_round` | End-round rules not dispatched via registry |
| Missing rule effect implementations | `src/rules/combat_rules.cpp` | ~40% of rules may have incomplete effects |
| Basic AI | `AIController::decide_action` | Ignores AI hints from rule definitions |
| No deployment simulation | N/A | Scout, Ambush rules have no effect |

---

## Implementation Order

Given the full scope, the order builds dependencies correctly:

```
Phase A: Complete Combat Rule Effects
    │
    ▼
Phase B: Unify Registries + All 185+ Faction Rules  ←── Largest phase
    │
    ├─────────────┬─────────────┐
    ▼             ▼             ▼
Phase C       Phase D       Phase F
(Movement)    (End-Round)   (Deployment)
    │             │             │
    └─────────────┴──────┬──────┘
                         ▼
                    Phase E: Rule-Aware AI  ←── Needs all phases
                         │
                         ▼
                    Phase G: Cleanup
```

---

## Phase A: Complete Combat Rule Effects

**Goal:** All 58+ base rules have working, tested effect implementations.

### A.1 Audit Current State

- Catalog all rules in `RuleId` enum
- For each rule, check if effect exists in `src/rules/combat_rules.cpp`
- Mark as: ✅ Implemented, ⚠️ Partial, ❌ Missing

### A.2 Implement Missing Effects

**Rules likely needing work:**

| Rule | Phase | Expected Behavior |
|------|-------|-------------------|
| Impact | HIT_BONUSES | +1 hit on charge per model |
| Indirect | PRE_ATTACK | Ignore cover, can shoot without LOS |
| Sniper | WOUND_ALLOCATION | Choose target model |
| Lock_On | HIT_MODIFIERS | +1 to hit vs target |
| Unstoppable | DEFENSE_RESOLUTION | Ignore AP |
| Limited | PRE_ATTACK | Track ammo, disable after X uses |
| Fear | HIT_MODIFIERS | -1 to hit in melee (aura) |

### A.3 Create Effect Test Suite

- Build `DeterministicDice` test harness
- Test each rule in isolation with known dice sequences
- Test rule interactions (Rending+Blast, Deadly+Takedown, etc.)

### A.4 Verify Against Game Rules

- Cross-reference each effect with official game rules
- Document any deviations or interpretations

**Deliverables:**
- All rules have effect functions in `combat_rules.cpp`
- Unit tests for each rule
- Rule effect documentation

---

## Phase B: Unify Registries + All Faction Rules

**Goal:** Single `RuleRegistry` handles base rules AND all 185+ faction-specific rules.

This is the largest phase due to faction rule volume.

### B.1 Analyze Faction Rule Categories

From the Excel file and existing code, faction rules fall into categories:

| Category | Count (est.) | Integration Approach |
|----------|--------------|---------------------|
| Grants base rule | ~60 | Map to existing RuleId |
| Stat modifier | ~40 | New modifier effects |
| Aura effects | ~30 | Modifier layer with range |
| Triggered abilities | ~25 | Trigger system |
| Spells/Casting | ~20 | Casting phase (new) |
| Unique mechanics | ~10 | Custom implementations |

### B.2 Extend RuleId Enum

Current enum has 64 primary + ~10 extended rules. Need to expand:

```
Primary Rules (0-63):     Existing base rules
Extended Rules (64-319):  Faction-specific rules
```

- Add faction rule IDs to extended section
- Organize by faction for maintainability
- Update `RulePresence` if needed for capacity

### B.3 Implement Modifier Layer

From Addendum Issue 5 in the architecture plan, create temporal modifier system:

**New structures needed:**
- `ActiveModifier` - tracks granted rules with duration
- `ModifierSource` - PERMANENT, AURA, BUFF, DEBUFF
- `ModifierDuration` - PERMANENT, UNTIL_END_OF_ROUND, etc.
- `UnitModifiers` - per-unit modifier tracking

**Modifier lifecycle:**
1. Permanent rules from unit definition
2. Auras applied at phase start
3. Buffs applied by abilities
4. Expiration checked at phase end

### B.4 Implement Aura Processing

For rules that affect nearby units:

- `AuraProcessor` class to handle aura application
- Run at start of each phase
- Track source unit for aura removal when source dies
- Range-based unit filtering

### B.5 Create Faction Rule Definitions

For each faction, create rule definition file:

```
include/rules/factions/
├── battle_brothers.hpp
├── alien_hives.hpp
├── robot_legions.hpp
├── daemon_hordes.hpp
├── ... (one per faction)
└── faction_rules_all.hpp  (aggregates all)
```

Each file defines:
- Faction-specific RuleIds
- Effect functions
- AI hints for faction rules

### B.6 Parse Faction Rules from Excel

- Create parser for `Faction Specific Army Rules.xlsx`
- Extract rule definitions programmatically
- Generate initial rule definition code

### B.7 Integrate with Unit Parser

- `UnitParser` resolves faction rules via main registry
- Apply granted rules during unit construction
- Store faction context on Unit for aura processing

### B.8 Remove FactionRulesRegistry

- Delete `include/core/faction_rules.hpp`
- Remove `initialize_faction_rules()` call
- Replace with unified registry initialization

**Deliverables:**
- Extended RuleId enum with all faction rules
- Modifier layer implementation
- Aura processor
- Per-faction rule definition files
- Updated UnitParser
- FactionRulesRegistry deleted

---

## Phase C: Registry-Based Movement

**Goal:** Movement rules dispatch through registry.

### C.1 Create Movement Rule Definitions

File: `include/rules/movement_rules.hpp` + `src/rules/movement_rules.cpp`

| Rule | SubPhase | Effect |
|------|----------|--------|
| Fast | CALCULATE_DISTANCE | +4" movement |
| Slow | CALCULATE_DISTANCE | -4" movement |
| Flying | CALCULATE_DISTANCE | Ignore terrain, ignore engagement |
| Strider | CALCULATE_DISTANCE | Ignore difficult terrain |
| Agile | CHARGE_DECLARE | +2" charge range |
| RapidCharge | CHARGE_DECLARE | Charge after advance |
| HitAndRun | POST_MOVE | Disengage move after melee |
| Scout | (Deployment) | Forward deploy |
| Ambush | (Deployment) | Flexible deploy |

### C.2 Build MovementContext in GameRunner

Before each movement action:

1. Create MovementContext with unit, move type, current position
2. Collect applicable movement rules from registry
3. Apply CALCULATE_DISTANCE phase effects
4. Apply CHARGE_DECLARE effects if charging
5. Execute movement with modified values
6. Apply POST_MOVE effects

### C.3 Refactor Movement Methods

Update `GameRunner` methods:
- `execute_advance()` - use registry for distance
- `execute_rush()` - use registry for distance
- `execute_charge()` - use registry for charge range
- Add `process_movement_phase()` dispatcher

### C.4 Handle Terrain (If Modeled)

If terrain is added later:
- Flying ignores all terrain
- Strider ignores difficult terrain
- Terrain modifiers apply via registry

**Deliverables:**
- Movement rule definitions
- MovementContext integration
- Refactored GameRunner movement
- Movement rule tests

---

## Phase D: Registry-Based End-Round Effects

**Goal:** End-of-round processing dispatches through registry.

### D.1 Create End-Round Rule Definitions

File: `include/rules/endround_rules.hpp` + `src/rules/endround_rules.cpp`

| Rule | SubPhase | Effect |
|------|----------|--------|
| Battleborn | MORALE | 4+ to rally at round start |
| Fearless | MORALE | Reroll failed morale |
| MoraleBoost | MORALE | +X to morale tests nearby |
| HoldTheLine | MORALE | Auto-pass if near objective |
| NoRetreat | MORALE | Cannot retreat, fight to death |
| Regeneration | REGENERATION | Heal wounds on 5+ |
| Fear | MORALE | Enemy -1 to morale nearby |

### D.2 Build EndRoundContext

At end of each round:

1. Create EndRoundContext with round number, all units, battlefield
2. Execute MORALE sub-phase
   - Collect morale modifiers from rules
   - Apply Fearless rerolls
   - Process morale test results
3. Execute REGENERATION sub-phase
   - Collect regeneration rules
   - Roll healing for wounded models
4. Execute CLEANUP sub-phase
   - Expire temporary modifiers
   - Reset per-round flags

### D.3 Refactor GameRunner Round Processing

- Remove hardcoded `check_battleborn()`
- Add `process_end_round_phase()` method
- Integrate morale with registry effects
- Move regeneration to registry dispatch

### D.4 Handle Morale Integration

Currently morale is in `RegistryCombatResolver::check_morale()`:
- Keep morale test logic there
- Add registry lookup for morale modifiers
- Apply Fearless, MoraleBoost, Fear via registry

**Deliverables:**
- End-round rule definitions
- EndRoundContext integration
- Refactored round processing
- End-round rule tests

---

## Phase E: Rule-Aware AI

**Goal:** AI decisions consider unit rules and capabilities.

### E.1 Add AI Hints to All Rules

For each rule in registry, add appropriate hints:

| Rule | AI Hint |
|------|---------|
| Impact | `action_preference: CHARGE, weight: 0.8` |
| Stealth | `preferred_range: LONG_RANGE` |
| PointBlankSurge | `preferred_range: POINT_BLANK` |
| Counter | `threat_modifier: charge_penalty: 0.7` |
| SelfDestruct | `threat_modifier: melee_penalty: 0.5` |
| Relentless | `preferred_range: MID_RANGE` |
| Sniper | `target_modifier: priority heroes` |
| Regeneration | `threat_modifier: priority: 0.8` (lower priority target) |

### E.2 Implement RuleAwareAIController

New class replacing basic `AIController`:

```cpp
class RuleAwareAIController {
    // Aggregate hints from unit's rules
    AIPreferences aggregate_hints(unit)

    // Evaluate actions with rule awareness
    float evaluate_shooting(unit, target, prefs)
    float evaluate_charge(unit, target, prefs)
    float evaluate_position(unit, pos, prefs)

    // Make decision
    ActionDecision decide_action(unit, game_state)
}
```

### E.3 Implement Strategic Choices

For rules with choices (VersatileAttack, Takedown):

**VersatileAttack:**
- Calculate AP improvement value
- Calculate hit improvement value
- Choose better option

**Takedown:**
- Prioritize heroes
- Then nearly-dead models
- Then special weapons
- Then first model

### E.4 Add AI Personalities

Implement personality presets:

| Personality | Aggression | Risk | Objective | Target |
|-------------|------------|------|-----------|--------|
| Cautious | 0.3 | 0.2 | 0.7 | 0.5 |
| Balanced | 0.5 | 0.5 | 0.5 | 0.5 |
| Aggressive | 0.8 | 0.7 | 0.3 | 0.8 |
| Berserker | 1.0 | 1.0 | 0.0 | 1.0 |

- Map unit AI types to personalities
- Melee units → Aggressive
- Shooting units → Cautious
- Hybrid units → Balanced

### E.5 Integrate with GameRunner

- Replace `AIController::decide_action` calls
- Pass registry to AI controller
- Use rule-aware decisions for all units

### E.6 Add Faction-Specific AI Behaviors

Some factions have unique AI considerations:
- Daemon units may be more reckless
- Robot units may be more calculating
- Swarm units may prioritize numbers

**Deliverables:**
- AI hints on all rule definitions
- RuleAwareAIController implementation
- Strategic choice functions
- AI personality system
- Faction AI considerations

---

## Phase F: Deployment Phase

**Goal:** Simulate deployment with Scout, Ambush, and positioning rules.

### F.1 Design Deployment Model

**Deployment Zones:**

```
        [-24" to -12"]  [-12" to 0]  [0 to 12"]  [12" to 24"]
Unit A:  Deploy Zone    No-man's    No-man's    Enemy Zone
Unit B:  Enemy Zone     No-man's    No-man's    Deploy Zone
```

**Deployment Order:**
1. Standard units deploy in zones
2. Scout units make 12" forward move
3. Ambush units deploy anywhere >9" from enemies

### F.2 Create Deployment Rule Definitions

File: `include/rules/deployment_rules.hpp` + `src/rules/deployment_rules.cpp`

| Rule | SubPhase | Effect |
|------|----------|--------|
| Scout | SCOUT_MOVE | 12" forward move after deployment |
| Ambush | INFILTRATE | Deploy anywhere >9" from enemies |

### F.3 Build DeploymentContext

```cpp
struct DeploymentContext {
    Unit& unit;
    Player& player;  // Which side
    Zone deployment_zone;
    Battlefield& battlefield;

    PositionSet allowed_positions;
    DeployTiming deploy_timing;
    u32 forward_distance;
}
```

### F.4 Add Deployment Phase to GameRunner

New method `run_deployment()`:

1. Determine deployment order (alternating or simultaneous)
2. For each unit:
   a. Build DeploymentContext
   b. Collect deployment rules from registry
   c. Apply SETUP phase (determine allowed positions)
   d. AI chooses position from allowed set
   e. Apply SCOUT_MOVE phase
   f. Apply INFILTRATE phase
3. Validate no units within 1" of enemies
4. Proceed to first round

### F.5 AI Deployment Decisions

Add deployment preferences to AI hints:
- Melee units deploy forward
- Shooting units deploy back
- Scout units use forward move aggressively
- Ambush units flank or threaten objectives

### F.6 Update GameState Initialization

- Remove fixed starting positions
- Initialize via deployment phase
- Track deployment zone info

**Deliverables:**
- Deployment rule definitions
- DeploymentContext implementation
- Deployment phase in GameRunner
- AI deployment decisions
- Updated game initialization

---

## Phase G: Cleanup and Optimization

**Goal:** Remove legacy code, optimize, document.

### G.1 Remove Legacy Code

- Delete `FactionRulesRegistry` and related code
- Remove any remaining hardcoded rule checks in GameRunner
- Delete old `AIController` basic implementation
- Remove dead code paths

### G.2 Consolidate Rule Files

Organize rule definitions:

```
include/rules/
├── combat_rules.hpp      (base combat rules)
├── movement_rules.hpp    (movement rules)
├── deployment_rules.hpp  (deployment rules)
├── endround_rules.hpp    (end-round rules)
└── factions/
    ├── index.hpp         (includes all factions)
    ├── battle_brothers.hpp
    ├── alien_hives.hpp
    └── ... (all factions)

src/rules/
├── combat_rules.cpp
├── movement_rules.cpp
├── deployment_rules.cpp
├── endround_rules.cpp
└── factions/
    └── all_factions.cpp  (registers all faction rules)
```

### G.3 Performance Optimization

- Profile full simulation run
- Optimize hot paths:
  - Rule collection caching
  - Effect lookup optimization
  - Reduce allocations in combat loop
- Verify no regression from current performance

### G.4 Documentation

- Update `RULE_ARCHITECTURE_IMPLEMENTATION_PLAN.md` with completion status
- Add "Adding a New Rule" guide
- Document faction rule format
- Add architecture diagram

### G.5 Final Validation

- Run large-scale simulation
- Verify all rules are exercised
- Check for any silent failures
- Performance benchmarks

**Deliverables:**
- Clean codebase with no legacy systems
- Organized rule file structure
- Performance validation
- Complete documentation

---

## File Change Summary

### New Files to Create

| File | Phase | Purpose |
|------|-------|---------|
| `src/rules/combat_rules.cpp` | A | Complete combat effects (extend existing) |
| `include/rules/movement_rules.hpp` | C | Movement rule declarations |
| `src/rules/movement_rules.cpp` | C | Movement rule implementations |
| `include/rules/deployment_rules.hpp` | F | Deployment rule declarations |
| `src/rules/deployment_rules.cpp` | F | Deployment rule implementations |
| `include/rules/endround_rules.hpp` | D | End-round rule declarations |
| `src/rules/endround_rules.cpp` | D | End-round rule implementations |
| `include/rules/factions/*.hpp` | B | Per-faction rule definitions |
| `src/rules/factions/all_factions.cpp` | B | Faction rule registration |
| `include/core/modifiers.hpp` | B | Modifier layer |
| `include/core/aura_processor.hpp` | B | Aura handling |
| `include/ai/rule_aware_ai.hpp` | E | New AI controller (extend existing) |
| `src/ai/rule_aware_ai.cpp` | E | AI implementation |
| `tests/rule_effects_test.cpp` | A | Rule effect tests |
| `tests/faction_rules_test.cpp` | B | Faction rule tests |

### Files to Modify

| File | Phase | Changes |
|------|-------|---------|
| `include/core/types.hpp` | B | Extend RuleId enum |
| `include/core/rule_registry.hpp` | B | Add modifier support |
| `include/core/unit.hpp` | B | Add UnitModifiers |
| `include/parser/unit_parser.hpp` | B | Faction rule resolution |
| `include/engine/game_runner.hpp` | C,D,E,F | Major refactor |
| `src/chunk_sim.cpp` | B | Remove faction init, add deployment |
| `CMakeLists.txt` | All | Add new source files |

### Files to Delete

| File | Phase | Reason |
|------|-------|--------|
| `include/core/faction_rules.hpp` | B | Replaced by unified registry |
| `src/core/faction_rules.cpp` | B | If exists |
| `include/engine/ai_controller.hpp` | E | Replaced by rule-aware AI |

---

## Testing Strategy

### Unit Tests (Per Phase)

| Phase | Test Focus |
|-------|------------|
| A | Each combat rule in isolation |
| B | Faction rules grant correctly, auras apply |
| C | Movement modifiers calculated correctly |
| D | End-round effects trigger properly |
| E | AI makes rule-aware decisions |
| F | Deployment positions valid |

### Integration Tests

- Full game with multiple rules interacting
- Faction vs faction matchups
- Edge cases (all models dead, max rules, etc.)

### Regression Tests

- Compare simulation distributions before/after
- Verify no rules silently broken
- Check performance benchmarks

### Deterministic Testing

- Use `DeterministicDice` for reproducible tests
- Golden file tests for complex scenarios
- A/B comparison capability

---

## Milestone Checkpoints

| Milestone | Phases | Validation |
|-----------|--------|------------|
| M1: Combat Complete | A | All base rules have effects, tests pass |
| M2: Unified Registry | B | Single registry, faction rules work |
| M3: Full Game Phases | C, D, F | Movement, deployment, end-round via registry |
| M4: Smart AI | E | AI uses rule hints for decisions |
| M5: Production Ready | G | Clean, optimized, documented |

---

## Risk Mitigation

### Risk 1: Breaking existing simulation results

**Mitigation:**
- Use deterministic dice for A/B testing
- Compare old vs new results for large sample
- Keep old code paths until validated

### Risk 2: Performance regression

**Mitigation:**
- Benchmark before and after each phase
- Profile hot paths (combat resolution called billions of times)
- Use function pointers not std::function (already done)

### Risk 3: Rule interaction bugs

**Mitigation:**
- Implement rule interaction table (from Addendum Issue 13)
- Test known complex interactions (Rending+Rupture, Blast+Takedown)
- Add integration tests for rule combinations

---

## Success Criteria

1. **Single Rule System:** Only `RuleRegistry` exists; `FactionRulesRegistry` removed
2. **All Rules Implemented:** Every `RuleId` has tested effect in registry
3. **No Hardcoded Logic:** `GameRunner` dispatches all rule effects via registry
4. **Movement via Registry:** Fast, Slow, Flying affect movement through registry
5. **End-Round via Registry:** Battleborn, Regeneration use registry dispatch
6. **Deployment Simulated:** Scout, Ambush affect starting positions
7. **AI Rule-Aware:** AI decisions consider unit rules and capabilities
8. **All Faction Rules:** 185+ faction rules integrated and functional
9. **Tests Pass:** All existing tests pass; new tests cover rule effects
10. **Performance Maintained:** No more than 10% slowdown vs current

---

## Appendix: Rule Categories Quick Reference

### Base Combat Rules (Phase A)

**Hit Modifiers:** Precise, GoodShot, BadShot, Stealth, RangedShrouding, MeleeEvasion, MeleeShrouding, Purge, Thrust

**Quality Override:** Reliable

**Hit Separation:** Rending, Rupture

**Hit Bonuses:** Relentless, Surge, PointBlankSurge, PredatorFighter, Impact

**Hit Multiplication:** Blast, Deadly

**Defense Resolution:** AP, Lance, Poison, Bane, BaneInMelee, Shred, Smash, PiercingAssault, Shielded, Protected, ShieldWall

**Wound Allocation:** Regeneration, Resistance, Tough, Takedown, SelfDestruct, Sniper, Hero

### Movement Rules (Phase C)

Fast, Slow, Flying, Strider, Agile, RapidCharge, HitAndRun

### Deployment Rules (Phase F)

Scout, Ambush

### End-Round Rules (Phase D)

Fearless, Battleborn, MoraleBoost, HoldTheLine, NoRetreat, Fear, Regeneration (healing)

### Faction Rule Categories (Phase B)

- Army-wide rules (affect all units)
- Unit-specific rules (affect single unit type)
- Aura rules (affect units within range)
- Spell/psychic rules (casting phase)
- Triggered abilities (on specific events)
- Stat modifiers (change base stats)
