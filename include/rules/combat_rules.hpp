#pragma once

#include "core/types.hpp"
#include "core/phases.hpp"
#include "core/traits.hpp"
#include "core/rule_data.hpp"
#include "core/rule_effect.hpp"
#include "core/contexts.hpp"
#include "core/rule_registry.hpp"

namespace battle {
namespace rules {

// ==============================================================================
// Combat Rule Effect Function Declarations
// ==============================================================================
// These are free functions (not lambdas) for maximum performance.
// They are used as function pointers in the effect table.

// === HIT_MODIFIERS Phase ===

// Precise: +1 to hit
void precise_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// Stealth: -1 to hit when distance > 9"
void stealth_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);
bool stealth_condition(const CombatContextCore& ctx);

// RangedShrouding: -1 to be hit when shot at
void ranged_shrouding_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// MeleeEvasion: -1 to be hit in melee
void melee_evasion_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// MeleeShrouding: -1 to be hit in melee
void melee_shrouding_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// GoodShot: +1 to hit when shooting
void good_shot_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// BadShot: -1 to hit when shooting
void bad_shot_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// Purge: +1 to hit vs Tough(3+)
void purge_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);
bool purge_condition(const CombatContextCore& ctx);

// Thrust: +1 to hit when charging (also affects AP in DEFENSE_RESOLUTION)
void thrust_hit_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);
bool thrust_condition(const CombatContextCore& ctx);

// === ROLL_HITS Phase ===

// Reliable: Quality becomes 2+
void reliable_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// === HIT_SEPARATION Phase ===

// Rending: Natural 6s get AP+4 (separated for different defense roll)
void rending_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// Rupture: Natural 6s bypass regen and deal +1 wound per wound
void rupture_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// === HIT_BONUSES Phase ===

// Relentless: Extra hits on 6s when shooting >9"
void relentless_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);
bool relentless_condition(const CombatContextCore& ctx);

// Surge: Extra hits on 6s
void surge_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// PointBlankSurge: Extra hits on 6s at short range
void point_blank_surge_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);
bool point_blank_surge_condition(const CombatContextCore& ctx);

// Furious: Extra hits on 6s when charging
void furious_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);
bool furious_condition(const CombatContextCore& ctx);

// === HIT_MULTIPLICATION Phase ===

// Blast: Multiply hits by value (capped at defender model count)
void blast_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value);

// ==============================================================================
// Combat Rule Hot Data Declarations
// ==============================================================================
// These are extern declarations - definitions are in combat_rules.cpp

extern const RuleHotData Precise_HotData;
extern const RuleHotData Reliable_HotData;
extern const RuleHotData Rending_HotData;
extern const RuleHotData Blast_HotData;
extern const RuleHotData Deadly_HotData;
extern const RuleHotData Poison_HotData;
extern const RuleHotData Bane_HotData;
extern const RuleHotData Stealth_HotData;
extern const RuleHotData Relentless_HotData;
extern const RuleHotData Furious_HotData;
extern const RuleHotData Surge_HotData;
extern const RuleHotData PointBlankSurge_HotData;
extern const RuleHotData Rupture_HotData;
extern const RuleHotData GoodShot_HotData;
extern const RuleHotData BadShot_HotData;
extern const RuleHotData RangedShrouding_HotData;
extern const RuleHotData MeleeEvasion_HotData;
extern const RuleHotData MeleeShrouding_HotData;
extern const RuleHotData Purge_HotData;
extern const RuleHotData Thrust_HotData;

// ==============================================================================
// Combat Rule Cold Data Declarations
// ==============================================================================

extern const RuleColdData Precise_ColdData;
extern const RuleColdData Reliable_ColdData;
extern const RuleColdData Rending_ColdData;
extern const RuleColdData Blast_ColdData;
extern const RuleColdData Stealth_ColdData;

// ==============================================================================
// Effect Entry Declarations
// ==============================================================================

extern const RuleEffectEntry Precise_Effects;
extern const RuleEffectEntry Reliable_Effects;
extern const RuleEffectEntry Rending_Effects;
extern const RuleEffectEntry Stealth_Effects;
extern const RuleEffectEntry Relentless_Effects;
extern const RuleEffectEntry Surge_Effects;
extern const RuleEffectEntry Blast_Effects;
extern const RuleEffectEntry GoodShot_Effects;
extern const RuleEffectEntry BadShot_Effects;
extern const RuleEffectEntry RangedShrouding_Effects;
extern const RuleEffectEntry MeleeEvasion_Effects;
extern const RuleEffectEntry MeleeShrouding_Effects;
extern const RuleEffectEntry Purge_Effects;
extern const RuleEffectEntry Thrust_Effects;
extern const RuleEffectEntry PointBlankSurge_Effects;
extern const RuleEffectEntry Furious_Effects;
extern const RuleEffectEntry Rupture_Effects;

// ==============================================================================
// Cold Data Declarations
// ==============================================================================

extern const RuleColdData GoodShot_ColdData;
extern const RuleColdData BadShot_ColdData;
extern const RuleColdData RangedShrouding_ColdData;
extern const RuleColdData MeleeEvasion_ColdData;
extern const RuleColdData MeleeShrouding_ColdData;
extern const RuleColdData Purge_ColdData;
extern const RuleColdData Thrust_ColdData;

// ==============================================================================
// Registration Function
// ==============================================================================

// Register all combat rules with a registry
void register_combat_rules(RuleRegistry& registry);

} // namespace rules
} // namespace battle
