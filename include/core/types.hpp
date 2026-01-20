#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace battle {

// ==============================================================================
// Fundamental Types - Optimized for cache efficiency
// ==============================================================================

// Use fixed-width integers for predictable memory layout
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

// Index types for referencing into arrays
using UnitIndex   = u16;
using ModelIndex  = u16;
using WeaponIndex = u16;
using RuleIndex   = u16;

// Maximum counts (compile-time constants)
constexpr size_t MAX_MODELS_PER_UNIT   = 32;
constexpr size_t MAX_WEAPONS_PER_MODEL = 8;
constexpr size_t MAX_RULES_PER_ENTITY  = 16;
constexpr size_t MAX_UNITS_PER_ARMY    = 64;
constexpr size_t MAX_NAME_LENGTH       = 64;

// ==============================================================================
// Enumerations
// ==============================================================================

enum class ModelState : u8 {
    Healthy = 0,
    Wounded = 1,
    Dead    = 2
};

enum class UnitStatus : u8 {
    Normal  = 0,
    Shaken  = 1,
    Routed  = 2
};

enum class CombatPhase : u8 {
    Shooting = 0,
    Melee    = 1
};

enum class ScenarioType : u8 {
    ShootingOnly    = 0,
    MutualShooting  = 1,
    Charge          = 2,
    ReceiveCharge   = 3,
    ShootThenCharge = 4,
    Approach1Turn   = 5,
    Approach2Turns  = 6,
    FullEngagement  = 7,
    FightingRetreat = 8,

    COUNT // Number of scenarios
};

enum class BattleWinner : u8 {
    Attacker = 0,
    Defender = 1,
    Draw     = 2
};

enum class VictoryCondition : u8 {
    AttackerDestroyedEnemy = 0,
    DefenderDestroyedEnemy = 1,
    AttackerRoutedEnemy    = 2,
    DefenderRoutedEnemy    = 3,
    AttackerRouted         = 4,
    DefenderRouted         = 5,
    MaxRoundsAttackerAhead = 6,
    MaxRoundsDefenderAhead = 7,
    MaxRoundsDraw          = 8,
    MutualDestruction      = 9
};

// ==============================================================================
// Special Rule Identifiers (Compact representation)
// ==============================================================================

// Rules are identified by enum for fast comparison and switch statements
// IMPORTANT: Rules 1-63 are PRIMARY (hot path) rules that fit in a 64-bit mask
// Rules 64+ are EXTENDED rules stored in a separate bitset
//
// Reordering note: If you need to add a new frequently-used rule, add it before
// PRIMARY_COUNT and adjust less-frequently-used rules to EXTENDED section.
enum class RuleId : u16 {
    None = 0,

    // === PRIMARY RULES (1-63) - Hot path, frequently checked ===
    // These rules are checked most often during combat resolution

    // Combat hit modifiers (checked every attack)
    Precise,        // Precise - +1 to hit (1)
    Reliable,       // Reliable - Quality 2+ (2)
    Rending,        // Rending - 6s to hit get AP(4) (3)
    Blast,          // Blast(X) - Multiply hits (4)
    Deadly,         // Deadly(X) - Multiply wounds (5)
    Poison,         // Poison - Reroll defense 6s (6)
    Bane,           // Bane - Bypass regeneration (7)
    AP,             // AP(X) - Armor Piercing (8)
    Lance,          // Lance - +2 AP on charge (9)
    Impact,         // Impact(X) - Extra attacks on charge (10)
    Surge,          // Surge - 6s to hit deal 1 extra hit (11)
    Thrust,         // Thrust - +1 to hit and AP(+1) when charging (12)

    // Defense rules (checked every defense roll)
    Regeneration,   // Regeneration - 5+ to ignore wound (13)
    Tough,          // Tough(X) - Multiple wounds to kill (14)
    Stealth,        // Stealth - -1 to be hit from >9" (15)
    Protected,      // Protected - 6+ to reduce AP (16)
    Shielded,       // Shielded - +1 defense vs non-spell hits (17)
    ShieldWall,     // Shield Wall - +1 Defense in melee (18)
    Resistance,     // Resistance - 6+ to ignore wounds (19)

    // Hit bonus rules (checked after each hit roll)
    Relentless,     // Relentless - Extra hits on 6s shooting >9" (20)
    Furious,        // Furious - Extra hits on 6s when charging (21)
    PointBlankSurge,// Point-Blank Surge - 6s to hit deal extra hit at short range (22)
    PredatorFighter,// Predator Fighter - 6s in melee generate extra attacks (23)

    // Wound modification rules (checked during wound application)
    Rupture,        // Rupture - Ignore regen, extra wound on unmodified 6 to hit (24)
    Shred,          // Shred - Extra wound on unmodified 1 to block (25)
    Smash,          // Smash - Ignore regen, +Blast(3) vs Defense 5+/6+ (26)
    Takedown,       // Takedown - Pick target model, resolve as unit of 1 (27)
    SelfDestruct,   // Self-Destruct - Deal X hits to attacker when killed in melee (28)

    // Movement rules (checked during movement phase)
    Fast,           // Fast - 9" move instead of 6" (29)
    Slow,           // Slow - 4" move instead of 6" (30)
    Flying,         // Flying - Can fly over terrain/units (31)
    Strider,        // Strider - Ignore difficult terrain (32)
    Agile,          // Agile - +1" advance, +2" rush/charge (33)
    RapidCharge,    // Rapid Charge - +4" charge move (34)

    // Morale rules (checked during morale phase)
    Fearless,       // Fearless - Reroll failed morale (35)
    NoRetreat,      // No Retreat - Can't be shaken/routed, take wounds instead (36)
    MoraleBoost,    // Morale Boost - +1 to morale test rolls (37)
    HoldTheLine,    // Hold the Line - Reroll failed morale tests (38)
    Fear,           // Fear(X) - Counts as +X wounds in melee (39)

    // Hit modifier rules - attacker side
    GoodShot,       // Good Shot - +1 to hit when shooting (40)
    BadShot,        // Bad Shot - -1 to hit when shooting (41)
    VersatileAttack,// Versatile Attack - Choose AP+1 or +1 to hit (42)
    Purge,          // Purge - +1 to hit vs Tough(3+) (43)
    PiercingAssault,// Piercing Assault - AP(1) on melee in charge (44)

    // Hit modifier rules - defender side
    MeleeEvasion,   // Melee Evasion - -1 to be hit in melee (45)
    MeleeShrouding, // Melee Shrouding - -1 to be hit in melee (46)
    RangedShrouding,// Ranged Shrouding - -1 to be hit when shooting (47)

    // Other combat rules
    BaneInMelee,    // Bane in Melee - All melee attacks have Bane (48)
    Limited,        // Limited - Weapon may only be used once per game (49)
    Counter,        // Counter - Strikes first when charged (50)
    HitAndRun,      // Hit & Run - Can retreat after fighting (51)
    Indirect,       // Indirect - Ignore cover (52)
    Sniper,         // Sniper - Pick target model (53)
    Lock_On,        // Lock-On - +1 to hit vs vehicles (54)

    // Special unit rules
    Hero,           // Hero - Takes wounds last (55)
    Unstoppable,    // Unstoppable - Ignore regen and negative modifiers (56)

    // Deployment rules
    Scout,          // Scout - Deploy 12" ahead (57)
    Ambush,         // Ambush - Can deploy anywhere >9" from enemy (58)

    // Movement restriction rules
    Immobile,       // Immobile - Can only use Hold actions (59)

    // Reserved slots for future hot rules (60-63)
    _Reserved60,
    _Reserved61,
    _Reserved62,
    _Reserved63,

    // === BOUNDARY MARKER ===
    PRIMARY_COUNT = 64,  // First 64 rules (0-63) fit in PrimaryRuleMask

    // === EXTENDED RULES (64+) - Less frequently checked ===
    // These rules are stored in ExtendedRuleMask bitset

    // Faction-specific rules
    Devout = PRIMARY_COUNT,  // Devout - Faction rule (64)
    Battleborn,     // Battleborn - 4+ to stop being Shaken at round start (65)
    Casting,        // Casting(X) - Can cast X spells (66)

    // === Category A: Simple Modifiers ===
    Evasive,        // Evasive - Enemies get -1 to hit always (67)
    Steadfast,      // Steadfast - 4+ to stop being Shaken at round start (68)
    Swift,          // Swift - Ignore Slow rule (69)
    Ferocious,      // Ferocious - Extra hit on 6s (unit rule, same as Surge) (70)
    Lacerate,       // Lacerate - Force re-roll defense 6s (71)
    Mischievous,    // Mischievous - Force re-roll defense 6s (72)
    Scrapper,       // Scrapper - Force re-roll defense 6s (73)

    // === Category B: Weapon Conditionals ===
    Bash,           // Bash - +Blast(3) vs Defense 5+/6+ (74)
    Thrash,         // Thrash - +Blast(3) vs Defense 5+/6+ (75)
    Crack,          // Crack - AP(+2) on hit 6s (76)
    Destructive,    // Destructive - AP(+4) on hit 6s (77)
    Fracture,       // Fracture - AP(+2) on hit 6s + ignores cover (78)
    Break,          // Break - AP(+2) on hit 6s + ignores regen (79)
    Slash,          // Slash - Extra hit on 6s + ignores cover (80)
    Butcher,        // Butcher - Extra hit on 6s + ignores regen (81)
    Slam,           // Slam - Extra wound on def 1s + ignores cover (82)
    Quake,          // Quake - Extra wound on def 1s + ignores regen (83)
    Tear,           // Tear - AP(+4) vs Tough(3-9) (84)
    Scratch,        // Scratch - AP(+2) vs Tough(3-9) (85)
    Puncture,       // Puncture - AP(+4) vs Tough(3-9) + ignores regen (86)
    Shatter,        // Shatter - AP(+2) vs Tough(3-9) + ignores regen (87)
    Demolish,       // Demolish - AP(+2) vs Tough(3-9) + ignores cover (88)
    Impale,         // Impale - Deadly(+3) vs Tough(3-9) (89)
    Skewer,         // Skewer - Deadly(+3) vs Tough(3-9) + ignores cover (90)
    Reap,           // Reap - AP(+2) vs Defense 2-3+ (91)
    Disintegrate,   // Disintegrate - AP(+2) vs Defense 2-3+ + ignores regen (92)
    Decimate,       // Decimate - AP(+2) vs Defense 2-3+ + ignores cover (93)
    Fragment,       // Fragment - AP(+1) vs Defense 2-4+ + ignores cover (94)
    Wreck,          // Wreck - Re-roll def 6s + ignores cover (95)

    // === Category C: Movement Bonuses ===
    Lustbound,      // Lustbound - +1" Advance, +3" Rush/Charge (96)
    Highborn,       // Highborn - +2" Advance, +2" Rush/Charge (97)
    Scurry,         // Scurry - +2" Advance, +2" Rush/Charge (98)
    Darkborn,       // Darkborn - +3" range, +3" Charge (99)
    HitAndRunShooter, // Hit & Run Shooter - Move 3" after shooting (100)

    // === Category D: Defense Modifiers ===
    Fortified,      // Fortified - Hits count as AP(-1), min AP(0) (101)
    Guardian,       // Guardian - AP(-1) when shot/charged from >9" (102)
    GuardianBoost,  // Guardian Boost - Guardian always applies (103)
    Sturdy,         // Sturdy - +1 defense when shot/charged from >9" (104)
    Knightborn,     // Knightborn - 6+ ignore wounds (4+ vs spells) (105)
    Plaguebound,    // Plaguebound - 6+ ignore wounds (106)
    Changebound,    // Changebound - Enemies get -1 to hit from >9" (107)

    // === Category E: Extra Attack Generation & Distance-based Combat ===
    Bloodborn,      // Bloodborn - 6s to hit generate +1 attack (non-recursive) (108)
    TargetingVisor, // Targeting Visor - +1 to hit when shooting over 9" (109)
    Havocbound,     // Havocbound - AP(+1) when shooting over 9" or charging (110)
    Warbound,       // Warbound - Extra wound on defense 1s (like Shred) (111)
    BrutalFighter,  // Brutal Fighter - 6s to hit deal extra hit in melee (112)

    // === Category F: Combat Choice & Retaliation ===
    Unpredictable,        // Unpredictable - Roll D6: 1-3 = AP+1, 4-6 = +1 hit (113)
    UnpredictableFighter, // Unpredictable Fighter - Roll D6 in melee only (114)
    UnpredictableShooter, // Unpredictable Shooter - Roll D6 when shooting only (115)
    Retaliate,            // Retaliate - When wounded in melee, attacker takes X hits (116)
    Deathstrike,          // Deathstrike - If killed in melee, attacker takes X hits (117)

    // === Category G: Enhanced Combat Modifiers & Boost Rules ===
    FerociousBoost,       // Ferocious Boost - Extra hits on 5-6 to hit (118)
    ChangeboundBoost,     // Changebound Boost - -1 to hit always (119)
    WarboundBoost,        // Warbound Boost - Extra wound on defense 1-2 (120)
    PlaegueboundBoost,    // Plaguebound Boost - 5-6 to ignore wounds (121)
    LustboundBoost,       // Lustbound Boost - +2/+6" movement (122)
    MeleeSlayer,          // Melee Slayer - AP(+2) vs Tough(3+) in melee (123)
    HeavyImpact,          // Heavy Impact - Impact hits get AP(1) (124)
    Watchborn,            // Watchborn - Pick AP(+1) or +1 to hit (125)

    // === Category H: Dice-Based Special Attacks & Post-Combat Movement ===
    Crush,                // Crush(X) - Roll X dice in melee, 4+ = AP(2) hit (126)
    Ravage,               // Ravage(X) - Roll X dice in melee, 6+ = wound (127)
    Hazardous,            // Hazardous - AP(4) but take wound on hit roll of 1 (128)
    QuakeWhenShooting,    // Quake when Shooting - Shred effect when shooting (129)
    Harassing,            // Harassing - Move 3" after shooting or melee (130)
    Guerrilla,            // Guerrilla - Move 3" after shooting/melee (once/round) (131)

    // === Category I: Aura Rules (map to base rules) ===
    VersatileReachAura,       // Versatile Reach Aura (132)
    UnpredictableFighterAura, // Unpredictable Fighter Aura (133)
    PointBlankPiercingAura,   // Point-Blank Piercing Aura (134)
    RangedSlayerAura,         // Ranged Slayer Aura (135)
    TargetingVisorBoostAura,  // Targeting Visor Boost Aura (136)
    PiercingHunterAura,       // Piercing Hunter Aura (137)
    ClanWarriorBoostAura,     // Clan Warrior Boost Aura (138)
    PrecisionFighterAura,     // Precision Fighter Aura - +1 to hit melee (139)
    MischievousBoostAura,     // Mischievous Boost Aura (140)
    QuickShotAura,            // Quick Shot Aura (141)
    HavocboundBoostAura,      // Havocbound Boost Aura (142)
    WarboundBoostAura,        // Warbound Boost Aura (143)
    InfectedBoostAura,        // Infected Boost Aura (144)
    UnpredictableShooterAura, // Unpredictable Shooter Aura (145)
    ScrapperBoostAura,        // Scrapper Boost Aura (146)
    FerociousBoostAura,       // Ferocious Boost Aura (147)
    PiercingFighterAura,      // Piercing Fighter Aura - AP+1 melee (148)
    PrecisionChargeAura,      // Precision Charge Aura - +1 hit charging (149)
    GroundedPrecisionAura,    // Grounded Precision Aura (150)
    MeleeSlayerAura,          // Melee Slayer Aura (151)
    PiercingShooterAura,      // Piercing Shooter Aura - AP+1 shooting (152)

    // === Category J: Buff Rules (grant abilities to friendlies) ===
    RighteousFury,            // Righteous Fury - Grant Piercing Assault (153)
    PrecisionFighterBuff,     // Precision Fighter Buff - +1 hit melee (154)
    PrecisionShooterBuff,     // Precision Shooter Buff - +1 hit shooting (155)
    BaneInMeleeBuff,          // Bane in Melee Buff (156)
    PrimalBoostBuff,          // Primal Boost Buff (157)
    PrecisionAttacksBuff,     // Precision Attacks Buff - +1 hit (158)

    // === Category K: Mark Rules (grant abilities vs targets) ===
    UnpredictableFighterMark, // Unpredictable Fighter Mark (159)
    UnstoppableShootingMark,  // Unstoppable Shooting Mark (160)
    ShredMark,                // Shred Mark (161)
    PiercingShootingMark,     // Piercing Shooting Mark - AP+1 (162)
    PrecisionFightingMark,    // Precision Fighting Mark - +1 hit melee (163)
    SlayerMark,               // Slayer Mark (164)
    RelentlessMark,           // Relentless Mark (165)
    RendingInMeleeMark,       // Rending in Melee Mark (166)
    FuriousMark,              // Furious Mark (167)
    IndirectMark,             // Indirect Mark (168)
    BaneMark,                 // Bane Mark (169)
    PrecisionShootingMark,    // Precision Shooting Mark - +1 hit shooting (170)
    QuickShotMark,            // Quick Shot Mark (171)

    // === Category L: Debuff Rules ===
    UnwieldyDebuff,           // Unwieldy Debuff - grant Unwieldy (172)
    PrecisionDebuff,          // Precision Debuff - -1 hit (173)
    PiercingShootingDebuff,   // Piercing Shooting Debuff - remove AP+1 (174)

    // === Category M: Scaling/Frenzy/Growth Rules ===
    PiercingGrowth,           // Piercing Growth - marker AP scaling (175)
    RuinousFrenzy,            // Ruinous Frenzy - marker hit/def bonus (176)
    DevastatingFrenzy,        // Devastating Frenzy - marker AP/def bonus (177)
    PrecisionGrowth,          // Precision Growth - marker hit bonus (178)
    DestructiveFrenzy,        // Destructive Frenzy - marker hit/AP bonus (179)

    // === Category N: Storm AoE Rules ===
    StormOfChange,            // Storm of Change - 3 dice AoE Shred (180)
    StormOfLust,              // Storm of Lust - 3 dice AoE Surge (181)
    StormOfPlague,            // Storm of Plague - 3 dice AoE Bane (182)
    StormOfWar,               // Storm of War - 3 dice AoE AP(1) (183)

    // === Category O: Special Combat Rules ===
    BreathAttack,             // Breath Attack - D6 area damage (184)
    IncreasedShootingRange,   // Increased Shooting Range - +6" (185)
    RegenerativeStrength,     // Regenerative Strength - attacks on ignore (186)
    Strafing,                 // Strafing - attack through movement (187)
    SurpriseAttack,           // Surprise Attack - infiltrate + damage (188)
    TakedownStrike,           // Takedown Strike - once/game Quality 2+ (189)
    Instinctive,              // Instinctive - attack closest +1 hit (190)
    CrossingAttack,           // Crossing Attack - dice through enemies (191)
    QuickReadjustment,        // Quick Readjustment - ignore Indirect penalty (192)
    CrossingStrike,           // Crossing Strike - once/game move attack (193)
    SurprisePiercingShot,     // Surprise Piercing Shot - AP+2 on Ambush (194)
    MindWound,                // Mind Wound - 2 Demolish hits (195)
    MobileArtillery,          // Mobile Artillery - +1 hit Hold, -2 hit (196)
    Vengeance,                // Vengeance - markers on destroy (197)

    // === Category P: Activation Rules ===
    MartialProwess,           // Activate twice per round (once per game) (198)
    Coordinate,               // Activate another friendly unit after this (199)
    InquisitorialAgent,       // Activate twice per round (once per game) (200)
    DelayedAction,            // Pass turn if opponent has more units (201)

    // === Category Q: Remaining Boost Auras ===
    SturdyBoostAura,          // Unit gets Sturdy Boost (202)
    VersatileDefenseAura,     // Unit gets Versatile Defense (203)
    ChangeboundBoostAura,     // Unit gets Changebound Boost (204)
    PlageboundBoostAura,      // Unit gets Plaguebound Boost (205)
    DefensiveGrowthAura,      // Unit gets Defensive Growth (206)
    MachineFogBoostAura,      // Unit gets Machine-Fog Boost (207)
    ReanimationAura,          // Unit gets Reanimation (208)
    GroundedReinforcementAura,// Unit gets Grounded Reinforcement (209)
    DevoutBoostAura,          // Unit gets Devout Boost (210)
    CourageAura,              // +1 to morale test rolls for unit (211)
    HoldTheLineBoostAura,     // Unit gets Hold the Line Boost (212)
    RapidRushAura,            // Unit gets Rapid Rush (213)
    HarassingBoostAura,       // Unit gets Harassing Boost (214)
    SwiftAura,                // Unit gets Swift (215)
    RapidBlinkBoostAura,      // Unit gets Rapid Blink Boost (216)
    RapidAdvanceAura,         // Unit gets Rapid Advance (217)
    LustboundBoostAura,       // Unit gets Lustbound Boost (218)
    BoundingAura,             // Unit gets Bounding (219)
    HighbornBoostAura,        // Unit gets Highborn Boost (220)
    SpeedFeatAura,            // Unit gets Speed Feat (221)
    ScurryBoostAura,          // Unit gets Scurry Boost (222)
    GuerrillaBoostAura,       // Unit gets Guerrilla Boost (223)

    // === Category R: Remaining Buffs ===
    StealthBuff,              // Grant Stealth to friendly unit (224)
    ProtectiveDome,           // Grant Evasive to up to three friendly units (225)
    GuardedBuff,              // Grant Guarded to friendly unit (226)
    RegenerationBuff,         // Grant Regeneration to friendly (227)
    EntrenchedBuff,           // Grant Entrenched to friendly (228)
    SelfRepairBoostBuff,      // Grant Self-Repair Boost to friendly (229)
    CourageBuff,              // Grant +1 morale to friendly unit (230)
    SteadfastBuff,            // Grant Steadfast to friendly (231)
    NoRetreatBuff,            // Grant No Retreat to friendly (232)
    RapidAdvanceBuff,         // Grant Rapid Advance to friendly (233)
    SwiftBuff,                // Grant Swift to friendly (234)
    SpeedBuff,                // Grant +2/+4" movement to friendly (235)
    IncreasedShootingRangeMark,// Grant +6" range vs target (236)
    ExtendedBuffRange,        // Extend buff ranges to 24" (237)
    IncreasedShootingRangeBuff,// Grant +6" range to friendly (238)
    CastingBuff,              // +1 to casting rolls for friendly caster (239)

    // === Category S: Remaining Debuffs ===
    FatigueDebuff,            // Force enemy to become fatigued (240)
    DefenseDebuff,            // -1 to defense for enemy unit (241)
    MoraleDebuff,             // -1 to enemy morale tests (242)
    SpeedDebuff,              // Reduce enemy movement (243)
    DangerousTerrainDebuff,   // Force Dangerous Terrain test (244)
    MindControl,              // Force enemy move on failed morale (245)
    DifficultTerrainDebuff,   // Force Difficult Terrain on enemy (246)
    CastingDebuff,            // -1 to casting rolls for enemy caster (247)

    // === Category T: Defense/Growth Rules ===
    FortifiedGrowth,          // Marker-based AP reduction over rounds (248)
    GroundedStealth,          // -1 to be hit when near terrain (249)
    ProtectionFeat,           // Once per game 4+ to ignore wounds (250)

    // === Category U: Deployment Rules ===
    Infiltrate,               // Ambush variant - deploy up to 1" from enemy (251)
    Spawn,                    // Once per game place new unit of X within 6" (252)
    ReDeployment,             // Remove and redeploy up to two units (253)
    RapidAmbush,              // Ambush variant - can deploy round 1 (254)
    AmbushBeacon,             // Friendly Ambushers can ignore distance (255)
    AmbushReDeployment,       // Remove and redeploy via Ambush (256)
    RepelAmbushers,           // Enemies must Ambush over 12" away (257)
    Reinforcement,            // Respawn when shaken/destroyed (258)
    Fanatic,                  // Place model within 9" after deployment (259)
    Split,                    // Spawn new unit when destroyed (260)

    // === Category V: Movement Rules ===
    Teleport,                 // Place model anywhere within 6" before attacking (261)
    Wolfborn,                 // Place models anywhere within 3" when activated (262)
    // Note: HitAndRunShooter already defined at line 249
    RapidBlink,               // Place models within 3" when activated (263)
    Bounding,                 // Place models within D3+1" when activated (264)

    // === Category W: Faction/Morale/Misc ===
    DevoutBoost,              // Extra hits on 5-6 instead of just 6 (266)
    Mend,                     // Remove D3 wounds from Tough model (267)
    HiveBond,                 // Units with this rule get +1 morale (268)
    RePositionArtillery,      // Move Artillery model up to 9" (269)

    // === Category X: Spell & Targeting Rules ===
    CasterGroup,              // Complex caster pooling mechanic (270)
    SpellConduit,             // Casters within 12" can cast from this position (271)
    SpellAccumulator,         // Store and release spell energy (272)
    PrecisionTarget,          // Place markers for hit bonus vs target (273)
    PiercingTarget,           // Place markers for AP bonus vs target (274)
    PrecisionSpotter,         // Spotter that grants hit bonus to allies (275)
    PiercingSpotter,          // Spotter that grants AP bonus to allies (276)
    PrecisionTag,             // Mark target for precision bonus (277)
    PiercingTag,              // Mark target for piercing bonus (278)

    // === Category Y: Unit Combat Order Rules ===
    CounterAttack,            // Counter-Attack - Unit strikes first when charged (no Impact reduction) (279)

    COUNT // Total number of rules
};

// Compile-time validation
static_assert(static_cast<size_t>(RuleId::PRIMARY_COUNT) == 64,
              "PRIMARY_COUNT must be exactly 64 for PrimaryRuleMask");
static_assert(static_cast<size_t>(RuleId::COUNT) <= 320,
              "RuleId::COUNT exceeds ExtendedRuleMask capacity");

// Primary rule mask - 64-bit for cache-friendly O(1) lookup of hot rules
using PrimaryRuleMask = u64;
constexpr size_t PRIMARY_RULE_LIMIT = 64;

// Legacy alias for backwards compatibility (will be removed after full migration)
using RuleMask = PrimaryRuleMask;

// Helper to get the bit position for a primary rule
// Returns 0 for None or extended rules (caller must handle extended rules separately)
inline constexpr PrimaryRuleMask rule_bit(RuleId id) {
    auto idx = static_cast<u16>(id);
    if (id == RuleId::None || idx >= PRIMARY_RULE_LIMIT) {
        return 0;
    }
    return PrimaryRuleMask(1) << idx;
}

// Check if a rule is in the primary range (hot path)
inline constexpr bool is_primary_rule(RuleId id) {
    auto idx = static_cast<u16>(id);
    return idx > 0 && idx < PRIMARY_RULE_LIMIT;
}

// Check if a rule is in the extended range (cold path)
inline constexpr bool is_extended_rule(RuleId id) {
    auto idx = static_cast<u16>(id);
    return idx >= PRIMARY_RULE_LIMIT && idx < static_cast<u16>(RuleId::COUNT);
}

// AI behavior type for Solo Play rules
enum class AIType : u8 {
    Melee = 0,      // No ranged weapons - charges aggressively
    Shooting = 1,   // Ranged > melee - maintains distance
    Hybrid = 2      // Melee >= ranged - opportunistic
};

// ==============================================================================
// Compact Rule Representation
// ==============================================================================

// A special rule with its value, packed into 4 bytes
// Note: RuleId expanded from u8 to u16 to support >64 rules
struct alignas(4) CompactRule {
    RuleId id;         // Rule identifier (16 bits)
    u8     value;      // Rule value (e.g., 3 for Blast(3), 0 if no value)
    u8     _padding;   // Padding for alignment

    constexpr CompactRule() : id(RuleId::None), value(0), _padding(0) {}
    constexpr CompactRule(RuleId r) : id(r), value(0), _padding(0) {}
    constexpr CompactRule(RuleId r, u8 v) : id(r), value(v), _padding(0) {}

    constexpr bool is_valid() const { return id != RuleId::None; }
    constexpr bool operator==(const CompactRule& other) const {
        return id == other.id && value == other.value;
    }
};

static_assert(sizeof(CompactRule) == 4, "CompactRule must be 4 bytes");

// ==============================================================================
// Fixed-Size String (avoids heap allocation)
// ==============================================================================

template<size_t N>
struct FixedString {
    std::array<char, N> data{};
    u8 length = 0;

    FixedString() = default;

    FixedString(std::string_view sv) {
        length = static_cast<u8>(std::min(sv.size(), N - 1));
        std::copy_n(sv.begin(), length, data.begin());
        data[length] = '\0';
    }

    std::string_view view() const { return {data.data(), length}; }
    const char* c_str() const { return data.data(); }
    bool empty() const { return length == 0; }
};

using Name = FixedString<MAX_NAME_LENGTH>;

} // namespace battle
