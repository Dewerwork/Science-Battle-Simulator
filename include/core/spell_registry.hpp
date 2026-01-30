#pragma once

#include "core/spell.hpp"
#include "core/types.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

namespace battle {

// ==============================================================================
// Spell Registry - Stores all faction spells
// ==============================================================================

class SpellRegistry {
public:
    static SpellRegistry& instance() {
        static SpellRegistry inst;
        return inst;
    }

    // Initialize with all faction spells
    void initialize();

    // Get spell list for a faction
    const FactionSpellList* get_faction_spells(std::string_view faction) const;

    // Check if a faction has spells
    bool faction_has_spells(std::string_view faction) const;

    // Get all registered factions with spells
    const std::vector<std::string>& get_factions_with_spells() const { return factions_with_spells_; }

private:
    SpellRegistry() = default;
    SpellRegistry(const SpellRegistry&) = delete;
    SpellRegistry& operator=(const SpellRegistry&) = delete;

    std::unordered_map<std::string, FactionSpellList> faction_spells_;
    std::vector<std::string> factions_with_spells_;
    std::once_flag init_flag_;  // Thread-safe one-time initialization

    // Helper to register a spell
    void register_spell(const char* faction, const SpellDef& spell);

    // Register all faction spells
    void register_alien_hives_spells();
    void register_battle_brothers_spells();
    void register_blood_brothers_spells();
    void register_dark_brothers_spells();
    void register_knight_brothers_spells();
    void register_watch_brothers_spells();
    void register_wolf_brothers_spells();
    void register_blessed_sisters_spells();
    void register_custodian_brothers_spells();
    void register_dao_union_spells();
    void register_dark_elf_raiders_spells();
    void register_dwarf_guilds_spells();
    void register_elven_jesters_spells();
    void register_eternal_dynasty_spells();
    void register_goblin_reclaimers_spells();
    void register_havoc_brothers_spells();
    void register_change_disciples_spells();
    void register_lust_disciples_spells();
    void register_plague_disciples_spells();
    void register_war_disciples_spells();
    void register_high_elf_fleets_spells();
    void register_human_defense_force_spells();
    void register_human_inquisition_spells();
    void register_infected_colonies_spells();
    void register_jackals_spells();
    void register_machine_cult_spells();
    void register_orc_marauders_spells();
    void register_prime_brothers_spells();
    void register_blood_prime_brothers_spells();
    void register_dark_prime_brothers_spells();
    void register_knight_prime_brothers_spells();
    void register_watch_prime_brothers_spells();
    void register_wolf_prime_brothers_spells();
    void register_ratmen_clans_spells();
    void register_rebel_guerrillas_spells();
    void register_robot_legions_spells();
    void register_saurian_starhost_spells();
    void register_soul_snatcher_cults_spells();
    void register_wormhole_daemons_spells();
};

// Helper function to create a direct damage spell
inline SpellDef make_direct_damage_spell(
    const char* name,
    const char* faction,
    u8 cost,
    u8 range,
    u8 hits,
    u8 ap = 0,
    bool rupture = false,
    bool bane = false,
    bool shred = false,
    bool surge = false,
    u8 blast = 0,
    u8 deadly = 0,
    bool smash = false
) {
    SpellDef spell;
    spell.name = FixedString<48>(name);
    spell.faction = FixedString<64>(faction);
    spell.cost = cost;
    spell.range = range;
    spell.hits = hits;
    spell.target_type = SpellTargetType::EnemyDirect;
    spell.effect_type = SpellEffectType::DirectDamage;
    spell.hit_rules.ap = ap;
    spell.hit_rules.has_rupture = rupture;
    spell.hit_rules.has_bane = bane;
    spell.hit_rules.has_shred = shred;
    spell.hit_rules.has_surge = surge;
    spell.hit_rules.has_blast = blast > 0;
    spell.hit_rules.blast_value = blast;
    spell.hit_rules.has_deadly = deadly > 0;
    spell.hit_rules.deadly_value = deadly;
    spell.hit_rules.has_smash = smash;
    return spell;
}

// Helper function to create a buff spell (grants rule to self)
inline SpellDef make_buff_spell(
    const char* name,
    const char* faction,
    u8 cost,
    u8 range,
    RuleId grants_rule = RuleId::None,
    i8 hit_mod = 0,
    i8 def_mod = 0
) {
    SpellDef spell;
    spell.name = FixedString<48>(name);
    spell.faction = FixedString<64>(faction);
    spell.cost = cost;
    spell.range = range;
    spell.target_type = SpellTargetType::Self;
    spell.effect_type = SpellEffectType::Buff;
    spell.grants_rule = grants_rule;
    spell.hit_modifier = hit_mod;
    spell.defense_modifier = def_mod;
    return spell;
}

// Helper function to create an enemy debuff spell
inline SpellDef make_debuff_spell(
    const char* name,
    const char* faction,
    u8 cost,
    u8 range,
    i8 hit_mod = 0,
    i8 def_mod = 0,
    i8 ap_mod = 0
) {
    SpellDef spell;
    spell.name = FixedString<48>(name);
    spell.faction = FixedString<64>(faction);
    spell.cost = cost;
    spell.range = range;
    spell.target_type = SpellTargetType::EnemyDebuff;
    spell.effect_type = SpellEffectType::Debuff;
    spell.hit_modifier = hit_mod;
    spell.defense_modifier = def_mod;
    spell.ap_modifier = ap_mod;
    return spell;
}

// Helper function to create a morale spell
inline SpellDef make_morale_spell(
    const char* name,
    const char* faction,
    u8 cost,
    u8 range
) {
    SpellDef spell;
    spell.name = FixedString<48>(name);
    spell.faction = FixedString<64>(faction);
    spell.cost = cost;
    spell.range = range;
    spell.target_type = SpellTargetType::EnemyDebuff;
    spell.effect_type = SpellEffectType::Morale;
    return spell;
}

} // namespace battle
