# Battle Simulator - Design & Implementation Plan

## Project Overview

### Goal
Simulate 1v1 unit matchups from OPR Grimdark Future to determine which units are "best" - meaning they can control an objective while dealing damage and surviving. Results will be used for YouTube content.

### Scale
- ~1 million unique unit loadouts
- ~1 trillion matchups (every unit vs every unit, including mirrors)
- Best 2 out of 3 games per matchup
- 4 rounds per game
- Target runtime: days to weeks (not months)

### Victory Condition
The unit that **controls the objective at the end of Round 4** wins.
- Control = within 3" of objective AND no non-shaken enemy within 3"
- If contested or neither in range = Draw (resolved by tiebreaker stats)

---

## Part 1: Data Structures

### 1.1 Unit Representation

```
Input Format:
─────────────
Hive Lord [1] Q3+ D2+ | 525pts | Fear(2), Fearless, Tough(12), Regeneration
Stomp (A4, AP(1)), 2x Serrated Blade (A3, AP(4)), Heavy Serrated Blade (A6, AP(4), Reliable)

Parsed Structure:
─────────────────
Unit {
    id: u32                     // Unique identifier for this loadout
    name: string                // "Hive Lord"
    model_count: u8             // 1
    quality: u8                 // 3 (means 3+)
    defense: u8                 // 2 (means 2+)
    points: u16                 // 525
    tough: u8                   // 12 (from Tough(12), default 1)

    // Special Rules (stored as flags + values for speed)
    rules: RuleSet {
        fearless: bool
        regeneration: bool
        fear_value: u8          // Fear(2) → 2
        fast: bool
        strider: bool
        flying: bool
        relentless: bool
        scout: bool
        ambush: bool
        hero: bool
        // ... etc
    }

    // Weapons
    weapons: Vec<Weapon>

    // Computed (cached)
    ai_type: AIType             // MELEE, SHOOTING, or HYBRID
    total_melee_damage: f32     // For AI classification
    total_ranged_damage: f32    // For AI classification
    max_range: u8               // Longest weapon range
}
```

### 1.2 Weapon Representation

```
Weapon {
    name: string
    count: u8                   // How many models have this weapon
    attacks: u8                 // A value
    range: u8                   // 0 = melee, else inches
    ap: u8                      // Armor Piercing

    // Special Rules (flags)
    blast: u8                   // Blast(X) value, 0 if none
    deadly: u8                  // Deadly(X) value, 0 if none
    reliable: bool
    rending: bool
    poison: bool
    bane: bool
    indirect: bool
    // ... etc
}
```

### 1.3 Game State

```
GameState {
    // Unit states
    unit_a: UnitState
    unit_b: UnitState

    // Positions (distance from center, in inches)
    // Negative = unit A's side, Positive = unit B's side
    pos_a: i8                   // Starts at -12 (12" from center)
    pos_b: i8                   // Starts at +12

    // Game progress
    current_round: u8           // 1-4
    current_activation: u8      // Which unit is activating

    // Statistics (tracked for tiebreakers)
    stats: MatchStats
}

UnitState {
    models_remaining: u8
    wounds_taken: u16           // Total wounds on surviving models
    status: Status              // NORMAL, SHAKEN, ROUTED, DESTROYED
    has_activated: bool         // This round
    is_in_melee: bool          // Locked in combat
}

MatchStats {
    wounds_dealt_a: u16
    wounds_dealt_b: u16
    models_killed_a: u8
    models_killed_b: u8
    rounds_holding_obj_a: u8
    rounds_holding_obj_b: u8
    first_blood: Option<Side>   // Who dealt damage first
}
```

### 1.4 Match Result (Compact - 8 bytes)

```
MatchResult {
    unit_a_id: u20              // Supports up to 1M units
    unit_b_id: u20              // Supports up to 1M units
    winner: u2                  // 0=A, 1=B, 2=Draw
    games_won_a: u2             // 0-2
    games_won_b: u2             // 0-2
    // Remaining 18 bits for compressed stats
}

// For detailed analysis, also store:
DetailedResult {
    match_result: MatchResult
    avg_wounds_dealt_a: u16
    avg_wounds_dealt_b: u16
    avg_models_killed_a: u8
    avg_models_killed_b: u8
    avg_rounds: u8
}
```

---

## Part 2: Game Simulation

### 2.1 Battlefield Layout

```
    -12"        -6"         0"          +6"        +12"
     │           │           │           │           │
  [UNIT A]                [OBJECTIVE]              [UNIT B]
     │           │           │           │           │
   Start     Charge      Control       Charge      Start
             Range         Zone        Range

Movement Speeds:
- Rush: 12" (no shooting)
- Advance: 6" (can shoot)
- Hold: 0" (shoot with bonuses if Relentless)
- Charge: 12" into melee
```

### 2.2 Round Structure

```
Each Round:
┌─────────────────────────────────────────────────────────┐
│ 1. DETERMINE ACTIVATION ORDER                           │
│    - Roll off (random), or alternate starting Round 2   │
│                                                         │
│ 2. FIRST UNIT ACTIVATES                                │
│    - If Shaken: Stay Idle (remove Shaken)              │
│    - Else: Follow AI Decision Tree                      │
│      → Move (Rush/Advance/Hold)                        │
│      → Shoot (if applicable)                           │
│      → Charge (if applicable)                          │
│      → Resolve Melee (if in combat)                    │
│                                                         │
│ 3. SECOND UNIT ACTIVATES                               │
│    - Same as above                                      │
│    - If charged, Strike Back                           │
│                                                         │
│ 4. END OF ROUND                                        │
│    - Check Morale (if took casualties this round)      │
│    - Update objective control stats                    │
│    - Check if game ends early (both destroyed/routed)  │
└─────────────────────────────────────────────────────────┘
```

### 2.3 AI Decision Trees (From Solo Play Rules)

```
CLASSIFY UNIT:
─────────────
if (has no ranged weapons):
    type = MELEE
else if (avg_melee_damage >= avg_ranged_damage):
    type = HYBRID
else:
    type = SHOOTING


MELEE AI:
─────────
1. Is objective not under my control?
   ├─ YES: Is enemy in the way (within 6" of path)?
   │       ├─ YES: Can charge? → CHARGE
   │       │       └─ NO: RUSH toward objective
   │       └─ NO: RUSH toward objective
   └─ NO: Can charge enemy?
          ├─ YES: CHARGE
          └─ NO: RUSH toward enemy


SHOOTING AI:
────────────
1. Is objective not under my control?
   ├─ YES: Can I advance and still shoot?
   │       ├─ YES: ADVANCE toward objective + SHOOT
   │       └─ NO: RUSH toward objective
   └─ NO: Can I advance and still shoot?
          ├─ YES: ADVANCE toward enemy + SHOOT
          └─ NO: RUSH toward enemy


HYBRID AI:
──────────
1. Is objective not under my control?
   ├─ YES: Is enemy in the way?
   │       ├─ YES: Can charge? → CHARGE
   │       │       └─ NO: ADVANCE toward obj + SHOOT (or RUSH)
   │       └─ NO: Is objective in Rush range but not Advance?
   │              ├─ YES: RUSH toward objective
   │              └─ NO: Will advancing put enemies in range?
   │                     ├─ YES: ADVANCE + SHOOT
   │                     └─ NO: RUSH toward objective
   └─ NO: Can charge?
          ├─ YES: CHARGE
          └─ NO: Will advancing put enemies in range?
                 ├─ YES: ADVANCE + SHOOT
                 └─ NO: RUSH toward enemy
```

### 2.4 Combat Resolution

```
SHOOTING:
─────────
1. Count attacks (sum of all ranged weapons in range)
2. Roll to hit (need Quality+ on D6)
   - Apply modifiers (Reliable sets quality to 2+)
   - Track 6s for Rending
3. Apply hit modifiers
   - Blast(X): multiply hits by X (cap at target model count)
   - Relentless: extra hits on 6s (if didn't move)
4. Defender rolls saves (need Defense+ on D6)
   - AP(X) adds X to required roll
   - Poison: reroll successful 6s
5. Apply wound modifiers
   - Deadly(X): multiply wounds by X
6. Allocate wounds (standard OPR order)
   - Non-tough, non-heroes first
   - Then tough models (most wounded first)
   - Heroes last
7. Check for Regeneration (5+ to ignore each wound)


MELEE:
──────
1. Charger strikes first
   - Same resolution as shooting but with melee weapons
   - Furious: +1 hit per 6 rolled (when charging)
   - Lance: +2 AP (when charging)
2. Defender strikes back
   - No charge bonuses
   - Shaken units count as Fatigued (only hit on 6s)
3. Compare wounds dealt
   - Loser takes Morale test
4. Units remain locked in melee until one is destroyed


MORALE:
───────
Triggers:
- Reduced to half strength (models or Tough value)
- Lost melee (dealt fewer wounds)

Test:
- Roll D6, need Quality+ to pass
- Fearless: If failed, roll again, 4+ = pass anyway

Failure:
- If at half strength: ROUT (removed from game)
- Else: become SHAKEN (-1 to hit, -1 to defense, must rally next activation)
```

### 2.5 Objective Control Check

```
At end of each round, check:

distance_a = abs(pos_a)     // Distance from center
distance_b = abs(pos_b)

a_in_range = (distance_a <= 3)
b_in_range = (distance_b <= 3)

if (a_in_range AND b_in_range):
    if (unit_a.shaken AND NOT unit_b.shaken):
        b_controls = true
    else if (unit_b.shaken AND NOT unit_a.shaken):
        a_controls = true
    else:
        contested = true
else if (a_in_range):
    a_controls = true
else if (b_in_range):
    b_controls = true
else:
    no_control = true

// Track for tiebreaker
if (a_controls): stats.rounds_holding_obj_a++
if (b_controls): stats.rounds_holding_obj_b++
```

---

## Part 3: Match & Tournament Structure

### 3.1 Single Game Flow

```
play_game(unit_a, unit_b) → GameResult:

    state = initialize_game(unit_a, unit_b)

    for round in 1..=4:
        if state.is_game_over():
            break

        // Activation order (alternate, loser of last round goes first)
        first, second = determine_activation_order(state, round)

        // First activation
        if not first.is_shaken():
            execute_ai_turn(state, first)
        else:
            rally(first)

        // Second activation
        if not second.is_shaken():
            execute_ai_turn(state, second)
        else:
            rally(second)

        // End of round
        check_morale(state)
        update_objective_control(state)

    return determine_winner(state)
```

### 3.2 Best of 3 Match

```
play_match(unit_a, unit_b) → MatchResult:

    wins_a = 0
    wins_b = 0
    total_stats = empty_stats()

    for game in 1..=3:
        // Alternate who "deploys" first each game
        if game is odd:
            result = play_game(unit_a, unit_b)
        else:
            result = play_game(unit_b, unit_a)  // Swap positions
            result = flip_perspective(result)

        total_stats.merge(result.stats)

        match result.winner:
            A: wins_a++
            B: wins_b++
            Draw: pass

        // Early exit if decided
        if wins_a == 2 or wins_b == 2:
            break

    return MatchResult {
        winner: if wins_a > wins_b then A
                else if wins_b > wins_a then B
                else resolve_tiebreaker(total_stats),
        games_won_a: wins_a,
        games_won_b: wins_b,
        stats: total_stats
    }
```

### 3.3 Tiebreaker Resolution

```
When wins_a == wins_b (including 0-0 after 3 draws):

resolve_tiebreaker(stats) → Winner:

    // 1. Total wounds dealt
    if stats.wounds_dealt_a > stats.wounds_dealt_b:
        return A
    if stats.wounds_dealt_b > stats.wounds_dealt_a:
        return B

    // 2. Models killed
    if stats.models_killed_a > stats.models_killed_b:
        return A
    if stats.models_killed_b > stats.models_killed_a:
        return B

    // 3. Rounds holding objective
    if stats.rounds_holding_a > stats.rounds_holding_b:
        return A
    if stats.rounds_holding_b > stats.rounds_holding_a:
        return B

    // 4. First blood
    if stats.first_blood == Some(A):
        return A
    if stats.first_blood == Some(B):
        return B

    // 5. True draw (extremely rare)
    return Draw
```

---

## Part 4: Performance Architecture

### 4.1 Parallelization Strategy

```
┌─────────────────────────────────────────────────────────┐
│                    MAIN THREAD                          │
│  - Load all units into memory                          │
│  - Create work queue of matchups                       │
│  - Dispatch batches to worker threads                  │
│  - Collect and write results                           │
└─────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
    ┌────────────┐  ┌────────────┐  ┌────────────┐
    │  WORKER 1  │  │  WORKER 2  │  │  WORKER N  │
    │            │  │            │  │            │
    │ - Own RNG  │  │ - Own RNG  │  │ - Own RNG  │
    │ - Own unit │  │ - Own unit │  │ - Own unit │
    │   copies   │  │   copies   │  │   copies   │
    │ - Process  │  │ - Process  │  │ - Process  │
    │   batch    │  │   batch    │  │   batch    │
    └────────────┘  └────────────┘  └────────────┘

Each worker processes ~10,000 matchups at a time
No synchronization needed during batch processing
```

### 4.2 Memory Layout

```
Unit Database (read-only, shared):
┌──────────────────────────────────────┐
│ Unit 0 │ Unit 1 │ Unit 2 │ ... │ 1M │  ~200 bytes each = ~200MB
└──────────────────────────────────────┘

Results (write, partitioned by thread):
┌──────────────────────────────────────┐
│ Thread 0 results buffer              │  8 bytes × batch_size
├──────────────────────────────────────┤
│ Thread 1 results buffer              │
├──────────────────────────────────────┤
│ ...                                  │
└──────────────────────────────────────┘

Output File (append-only):
┌──────────────────────────────────────┐
│ Header: unit_count, matchup_count    │
│ Result 0                             │
│ Result 1                             │
│ ...                                  │
│ Result N                             │
└──────────────────────────────────────┘
```

### 4.3 Estimated Performance

```
Per-game operations:
- ~4 rounds
- ~2 activations per round
- ~1-3 combat resolutions per activation
- ~10-50 dice rolls per combat

Estimated: ~200-500 dice rolls per game
At 100M rolls/sec: ~200K-500K games/sec per core

With 8 cores: ~1.6M-4M games/sec
With 16 cores: ~3.2M-8M games/sec

Time for 1 trillion matchups (×3 games each = 3T games):
- 8 cores:  ~9-22 days
- 16 cores: ~4-11 days
- 32 cores: ~2-6 days
```

### 4.4 Checkpointing

```
Every N matchups (e.g., 10 million):
1. Flush results to disk
2. Save checkpoint file:
   - Last completed matchup index
   - Timestamp
   - Stats so far
3. Can resume from checkpoint if interrupted
```

---

## Part 5: Output & Analysis

### 5.1 Primary Output Files

```
1. RESULTS DATABASE (Binary, queryable):
   - Header: magic number, version, unit_count
   - Unit index: [unit_id → name, faction, points mapping]
   - Results: [MatchResult × N]
   - Indexes for fast queries by unit_id, faction, points range

2. REPLAY DATA (Compressed, selective storage):
   - Match ID
   - Per-game data:
     - Round-by-round state snapshots
     - Actions taken (movement, shooting, charges)
     - Dice rolls and outcomes
     - Casualties per round
   - Compressed with LZ4 for speed
   - ~500 bytes per replay
   - SELECTIVE STORAGE (to manage disk space):
     - Always keep: Close matches (45-55% win margin)
     - Always keep: Upsets (lower points beats higher by >10%)
     - Always keep: Perfect sweeps (2-0 with early victories)
     - Sample: 0.1% random sample of all other matches
   - Estimated: ~10-50GB of replay data

Can export to CSV/JSON for analysis tools
```

### 5.2 Analysis Queries

```
After simulation, analyze with separate tool:

1. "Best overall unit"
   → Highest win rate across all matchups

2. "Best unit at X points"
   → Filter to weight class, highest win rate

3. "Best counter to Unit X"
   → Filter matchups involving X, sort by win rate against X

4. "Most interesting matchups"
   → Closest to 50/50, most draws, most variance

5. "Loadout comparison"
   → Same base unit, different loadouts, compare win rates

6. "Faction power ranking"
   → Average win rate per faction
```

### 5.3 YouTube-Friendly Output

```
Generate summary reports:

OVERALL CHAMPION
────────────────
#1: Carnivo-Rex (Loadout 47)     - 73.2% win rate
#2: Hive Lord (Loadout 12)       - 71.8% win rate
#3: Prime Warriors (Loadout 3)   - 69.4% win rate

BEST PER WEIGHT CLASS
─────────────────────
Light (0-150pts):    Assault Grunts [10] - 67.1%
Medium (151-300pts): Hive Warriors [3]   - 64.3%
Heavy (301-500pts):  Hive Guardians [3]  - 68.9%
Super (501+pts):     Carnivo-Rex         - 73.2%

BEST MATCHUPS (for drama)
─────────────────────────
Carnivo-Rex vs Hive Lord: 51.2% - 48.8% (SO CLOSE!)
Assault Grunts vs Flamer Beast: 50.1% - 49.9% (COIN FLIP!)

BIGGEST UPSETS
──────────────
Cheap Unit X (100pts) beats Expensive Unit Y (400pts) 54% of the time!
```

---

## Part 6: Implementation Phases

### Phase 1: Foundation (Week 1)
- [ ] Unit data parser (text format → internal structure)
- [ ] Basic data structures (Unit, Weapon, GameState)
- [ ] Unit validation and AI classification
- [ ] Test with sample units from your format

### Phase 2: Game Engine (Week 2)
- [ ] Movement system (Rush/Advance/Hold/Charge)
- [ ] Shooting resolution
- [ ] Melee resolution
- [ ] Morale system
- [ ] AI decision trees (all 3 types)
- [ ] Single game loop
- [ ] Test: run 1000 games between two units, verify results make sense

### Phase 3: Match System (Week 3)
- [ ] Best of 3 logic
- [ ] Tiebreaker resolution
- [ ] Statistics tracking
- [ ] Result storage format
- [ ] Test: run 10,000 matches, analyze output

### Phase 4: Scale (Week 4)
- [ ] Thread pool implementation
- [ ] Batch processing
- [ ] Checkpointing
- [ ] Progress reporting
- [ ] Output file format
- [ ] Test: run 1 million matchups, verify speed and correctness

### Phase 5: Analysis Tools (Week 5)
- [ ] Result file reader
- [ ] Query/filter system
- [ ] Summary report generator
- [ ] CSV/JSON export
- [ ] Visualization helpers (for video)

### Phase 6: Full Run (Week 6+)
- [ ] Load complete unit database
- [ ] Run full simulation
- [ ] Monitor and checkpoint
- [ ] Generate final reports

---

## Part 7: Design Decisions (Confirmed)

1. **Terrain**: Open field (no cover) - keeps simulation clean and comparable

2. **Starting distance**: 24" apart (standard deployment)

3. **Special rules**: Implement ALL special rules from the game
   - Unit rules: Tough, Regeneration, Fear, Fearless, Fast, Strider, Flying, Relentless, Scout, Ambush, Hero, Stealth, etc.
   - Weapon rules: AP, Blast, Deadly, Reliable, Rending, Poison, Bane, Indirect, Furious, Lance, etc.

4. **Multi-model weapon distribution**: (Pending clarification from unit data format)

5. **Output**:
   - Queryable matchup database (filter by unit, faction, points, etc.)
   - Round-by-round replay data for all matches (allows deep analysis and video content)

---

## Appendix: Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Simulation too slow | Can't finish in reasonable time | Simplify AI, reduce games per match |
| Inaccurate results | YouTube claims are wrong | Validate against manual games |
| Data parsing errors | Wrong unit stats | Extensive unit validation |
| Edge cases in rules | Crashes or wrong outcomes | Comprehensive testing |
| Storage space | Can't store 1T results | Compress results, store only summaries |

---

**Please review and let me know:**
1. Does this match your vision?
2. What would you change?
3. Which open questions can you answer?
4. Ready to proceed with Phase 1?
