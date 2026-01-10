#include "core/spell_registry.hpp"

namespace battle {

void SpellRegistry::register_spell(const char* faction, const SpellDef& spell) {
    auto it = faction_spells_.find(faction);
    if (it == faction_spells_.end()) {
        FactionSpellList list;
        list.faction_name = FixedString<64>(faction);
        faction_spells_[faction] = list;
        factions_with_spells_.push_back(faction);
        it = faction_spells_.find(faction);
    }
    it->second.add_spell(spell);
}

const FactionSpellList* SpellRegistry::get_faction_spells(std::string_view faction) const {
    auto it = faction_spells_.find(std::string(faction));
    if (it != faction_spells_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SpellRegistry::faction_has_spells(std::string_view faction) const {
    return faction_spells_.find(std::string(faction)) != faction_spells_.end();
}

void SpellRegistry::initialize() {
    if (initialized_) return;

    register_alien_hives_spells();
    register_battle_brothers_spells();
    register_blood_brothers_spells();
    register_dark_brothers_spells();
    register_knight_brothers_spells();
    register_watch_brothers_spells();
    register_wolf_brothers_spells();
    register_blessed_sisters_spells();
    register_custodian_brothers_spells();
    register_dao_union_spells();
    register_dark_elf_raiders_spells();
    register_dwarf_guilds_spells();
    register_elven_jesters_spells();
    register_eternal_dynasty_spells();
    register_goblin_reclaimers_spells();
    register_havoc_brothers_spells();
    register_change_disciples_spells();
    register_lust_disciples_spells();
    register_plague_disciples_spells();
    register_war_disciples_spells();
    register_high_elf_fleets_spells();
    register_human_defense_force_spells();
    register_human_inquisition_spells();
    register_infected_colonies_spells();
    register_jackals_spells();
    register_machine_cult_spells();
    register_orc_marauders_spells();
    register_prime_brothers_spells();
    register_blood_prime_brothers_spells();
    register_dark_prime_brothers_spells();
    register_knight_prime_brothers_spells();
    register_watch_prime_brothers_spells();
    register_wolf_prime_brothers_spells();
    register_ratmen_clans_spells();
    register_rebel_guerrillas_spells();
    register_robot_legions_spells();
    register_saurian_starhost_spells();
    register_soul_snatcher_cults_spells();
    register_wormhole_daemons_spells();

    initialized_ = true;
}

// ==============================================================================
// Alien Hives Spells
// ==============================================================================
void SpellRegistry::register_alien_hives_spells() {
    const char* faction = "Alien Hives";
    // Cost 1
    register_spell(faction, make_buff_spell("Animate Spirit", faction, 1, 12, RuleId::HitAndRun));
    register_spell(faction, make_direct_damage_spell("Overwhelming Strike", faction, 1, 12, 2, 1, true)); // AP(1), Rupture
    // Cost 2
    register_spell(faction, make_buff_spell("Infuse Bloodthirst", faction, 2, 12)); // Hive Bond Boost
    register_spell(faction, make_direct_damage_spell("Psychic Blast", faction, 2, 12, 4, 2)); // AP(2)
    // Cost 3
    register_spell(faction, make_debuff_spell("Terror Seeker", faction, 3, 18)); // Unpredictable Fighter mark
    register_spell(faction, make_direct_damage_spell("Hive Shriek", faction, 3, 18, 6, 1)); // AP(1)
}

// ==============================================================================
// Battle Brothers Spells
// ==============================================================================
void SpellRegistry::register_battle_brothers_spells() {
    const char* faction = "Battle Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Advanced Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Cerebral Trauma", faction, 1, 18, 1, 0, false, false, false, false, 0, 0, true)); // Smash
    // Cost 2
    register_spell(faction, make_buff_spell("Blessed Ammo", faction, 2, 12)); // Shred when shooting
    register_spell(faction, make_direct_damage_spell("Lightning Fog", faction, 2, 9, 4)); // 4 hits each to up to 2 units (simplified to 4 hits)
    // Cost 3
    register_spell(faction, make_buff_spell("Protective Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Psychic Terror", faction, 3, 6, 9, 0, false, true)); // Bane
}

// ==============================================================================
// Blood Brothers Spells
// ==============================================================================
void SpellRegistry::register_blood_brothers_spells() {
    const char* faction = "Blood Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Blood Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Blood Trauma", faction, 1, 18, 1, 0, false, false, false, false, 0, 0, true)); // Smash
    // Cost 2
    register_spell(faction, make_buff_spell("Burst of Rage", faction, 2, 12, RuleId::Furious));
    register_spell(faction, make_direct_damage_spell("Heavenly Lance", faction, 2, 18, 4, 1)); // AP(1)
    // Cost 3
    register_spell(faction, make_buff_spell("Blood Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Shield Breaker", faction, 3, 12, 6, 1, false, false, true)); // AP(1), Shred
}

// ==============================================================================
// Dark Brothers Spells
// ==============================================================================
void SpellRegistry::register_dark_brothers_spells() {
    const char* faction = "Dark Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Dark Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Dark Trauma", faction, 1, 18, 1, 0, false, false, false, false, 0, 0, true)); // Smash
    // Cost 2
    register_spell(faction, make_buff_spell("Blessed Ammo", faction, 2, 12)); // Shred when shooting
    register_spell(faction, make_direct_damage_spell("Lightning Fog", faction, 2, 9, 4));
    // Cost 3
    register_spell(faction, make_buff_spell("Dark Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Psychic Terror", faction, 3, 6, 9, 0, false, true)); // Bane
}

// ==============================================================================
// Knight Brothers Spells
// ==============================================================================
void SpellRegistry::register_knight_brothers_spells() {
    const char* faction = "Knight Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Knight Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Knight Trauma", faction, 1, 18, 1, 0, false, false, false, false, 0, 0, true)); // Smash
    // Cost 2
    register_spell(faction, make_debuff_spell("Banishing Sigil", faction, 2, 18, 0, -1)); // -1 to defense rolls
    register_spell(faction, make_direct_damage_spell("Doom Strike", faction, 2, 6, 2, 2, false, false, false, false, 0, 3)); // AP(2), Deadly(3)
    // Cost 3
    register_spell(faction, make_buff_spell("Knight Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Purge the Impure", faction, 3, 12, 3, 2, false, false, false, false, 0, 0)); // AP(2), hits 2 units
}

// ==============================================================================
// Watch Brothers Spells
// ==============================================================================
void SpellRegistry::register_watch_brothers_spells() {
    const char* faction = "Watch Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Watch Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Watch Trauma", faction, 1, 18, 1, 0, false, false, false, false, 0, 0, true)); // Smash
    // Cost 2
    register_spell(faction, make_buff_spell("Blessed Ammo", faction, 2, 12)); // Shred when shooting
    register_spell(faction, make_direct_damage_spell("Lightning Fog", faction, 2, 9, 4));
    // Cost 3
    register_spell(faction, make_buff_spell("Watch Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Psychic Terror", faction, 3, 6, 9, 0, false, true)); // Bane
}

// ==============================================================================
// Wolf Brothers Spells
// ==============================================================================
void SpellRegistry::register_wolf_brothers_spells() {
    const char* faction = "Wolf Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Wolf Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Wolf Trauma", faction, 1, 18, 1, 0, false, false, false, false, 0, 0, true)); // Smash
    // Cost 2
    register_spell(faction, make_buff_spell("Righteous Fury", faction, 2, 12)); // Piercing Assault
    register_spell(faction, make_direct_damage_spell("Godly Thunder", faction, 2, 24, 2, 4)); // AP(4)
    // Cost 3
    register_spell(faction, make_buff_spell("Wolf Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Storm of Power", faction, 3, 12, 6, 1, false, false, false, true)); // AP(1), Surge
}

// ==============================================================================
// Blessed Sisters Spells
// ==============================================================================
void SpellRegistry::register_blessed_sisters_spells() {
    const char* faction = "Blessed Sisters";
    // Cost 1
    register_spell(faction, make_debuff_spell("Burn the Heretic", faction, 1, 18)); // -3 to casting rolls
    register_spell(faction, make_direct_damage_spell("Righteous Wrath", faction, 1, 24, 1, 4)); // AP(4)
    // Cost 2
    register_spell(faction, make_buff_spell("Holy Rage", faction, 2, 12)); // Piercing Hunter
    register_spell(faction, make_direct_damage_spell("Eternal Flame", faction, 2, 6, 6)); // Purge
    // Cost 3
    register_spell(faction, make_buff_spell("Litanies of War", faction, 3, 12)); // Devout Boost
    register_spell(faction, make_direct_damage_spell("Searing Admonition", faction, 3, 18, 3, 0, false, false, false, false, 3)); // Blast(3)
}

// ==============================================================================
// Custodian Brothers Spells
// ==============================================================================
void SpellRegistry::register_custodian_brothers_spells() {
    const char* faction = "Custodian Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("The Founder's Curse", faction, 1, 18)); // Shred when attacking
    register_spell(faction, make_direct_damage_spell("Thunderous Mist", faction, 1, 18, 2));
    // Cost 2
    register_spell(faction, make_buff_spell("Focused Defender", faction, 2, 12)); // Unpredictable Fighter
    register_spell(faction, make_direct_damage_spell("Dread Strike", faction, 2, 24, 2)); // Tear (simplified)
    // Cost 3
    register_spell(faction, make_buff_spell("Guardian Protection", faction, 3, 12)); // Guardian Boost
    register_spell(faction, make_direct_damage_spell("Mind Gash", faction, 3, 12, 6, 1, false, false, true)); // AP(1), Shred
}

// ==============================================================================
// DAO Union Spells
// ==============================================================================
void SpellRegistry::register_dao_union_spells() {
    const char* faction = "DAO Union";
    // Cost 1
    register_spell(faction, make_buff_spell("Aura of Peace", faction, 1, 12, RuleId::Fearless));
    register_spell(faction, make_direct_damage_spell("Killing Blow", faction, 1, 18, 2, 1)); // AP(1)
    // Cost 2
    register_spell(faction, make_buff_spell("Psychic Stabilization", faction, 2, 12)); // Targeting Visor Boost
    register_spell(faction, make_direct_damage_spell("Deadly Surge", faction, 2, 6, 6, 0, false, true)); // Bane
    // Cost 3
    register_spell(faction, make_debuff_spell("Coordinated Aggression", faction, 3, 18, 0, 0, 1)); // AP(1) when shooting
    register_spell(faction, make_direct_damage_spell("Devastating Strike", faction, 3, 12, 6)); // Decimate
}

// ==============================================================================
// Dark Elf Raiders Spells
// ==============================================================================
void SpellRegistry::register_dark_elf_raiders_spells() {
    const char* faction = "Dark Elf Raiders";
    // Cost 1
    register_spell(faction, make_buff_spell("Psy-Adrenaline", faction, 1, 12)); // Harassing Boost
    register_spell(faction, make_direct_damage_spell("Snake Bite", faction, 1, 12, 2, 1)); // AP(1), Lacerate
    // Cost 2
    register_spell(faction, make_debuff_spell("Raiding Drugs", faction, 2, 18, 1)); // +1 to hit in melee
    register_spell(faction, make_direct_damage_spell("Art of Pain", faction, 2, 6, 2, 2, false, false, false, false, 0, 3)); // AP(2), Deadly(3)
    // Cost 3
    register_spell(faction, make_buff_spell("Fade in the Dark", faction, 3, 12, RuleId::Stealth));
    register_spell(faction, make_direct_damage_spell("Holistic Suffering", faction, 3, 12, 9));
}

// ==============================================================================
// Dwarf Guilds Spells
// ==============================================================================
void SpellRegistry::register_dwarf_guilds_spells() {
    const char* faction = "Dwarf Guilds";
    // Cost 1
    register_spell(faction, make_buff_spell("Battle Rune", faction, 1, 12)); // +6" range when shooting
    register_spell(faction, make_direct_damage_spell("Breaking Rune", faction, 1, 24, 1, 4)); // AP(4)
    // Cost 2
    register_spell(faction, make_buff_spell("Armor Rune", faction, 2, 12)); // Sturdy Boost
    register_spell(faction, make_direct_damage_spell("Smiting Rune", faction, 2, 12, 4, 1)); // AP(1), Quake
    // Cost 3
    register_spell(faction, make_debuff_spell("Deceleration Rune", faction, 3, 18)); // Movement debuff
    register_spell(faction, make_direct_damage_spell("Cleaving Rune", faction, 3, 18, 3, 0, false, false, false, false, 3)); // Blast(3)
}

// ==============================================================================
// Elven Jesters Spells
// ==============================================================================
void SpellRegistry::register_elven_jesters_spells() {
    const char* faction = "Elven Jesters";
    // Cost 1
    register_spell(faction, make_buff_spell("Asphyxiating Fog", faction, 1, 12, RuleId::Counter));
    register_spell(faction, make_direct_damage_spell("Blades of Discord", faction, 1, 6, 1, 2, false, false, false, false, 0, 3)); // AP(2), Deadly(3)
    // Cost 2
    register_spell(faction, make_buff_spell("Shadow Dance", faction, 2, 12)); // Rapid Blink Boost
    register_spell(faction, make_direct_damage_spell("Light Fragments", faction, 2, 12, 4, 1)); // AP(1), Fragment
    // Cost 3
    register_spell(faction, make_debuff_spell("Veil of Madness", faction, 3, 18)); // Slayer against
    register_spell(faction, make_direct_damage_spell("Fatal Sorrow", faction, 3, 18, 6));
}

// ==============================================================================
// Eternal Dynasty Spells
// ==============================================================================
void SpellRegistry::register_eternal_dynasty_spells() {
    const char* faction = "Eternal Dynasty";
    // Cost 1
    register_spell(faction, make_buff_spell("Spirit Power", faction, 1, 12, RuleId::Flying));
    register_spell(faction, make_direct_damage_spell("Soul Spear", faction, 1, 24, 1)); // Puncture
    // Cost 2
    register_spell(faction, make_buff_spell("Spirit Resolve", faction, 2, 12)); // Clan Warrior Boost
    register_spell(faction, make_direct_damage_spell("Mind Vortex", faction, 2, 12, 2, 2)); // AP(2), hits 2 units
    // Cost 3
    register_spell(faction, make_debuff_spell("Eternal Guidance", faction, 3, 18)); // +6" range when shooting
    register_spell(faction, make_direct_damage_spell("Dragon Breath", faction, 3, 12, 9));
}

// ==============================================================================
// Goblin Reclaimers Spells
// ==============================================================================
void SpellRegistry::register_goblin_reclaimers_spells() {
    const char* faction = "Goblin Reclaimers";
    // Cost 1
    register_spell(faction, make_buff_spell("Ammo Boost", faction, 1, 12)); // Mischievous Boost
    register_spell(faction, make_direct_damage_spell("Zap!", faction, 1, 12, 2, 1, false, false, false, true)); // AP(1), Surge
    // Cost 2
    register_spell(faction, make_debuff_spell("Mob Frenzy", faction, 2, 18, 0, 0, 1)); // AP(1) when shooting
    register_spell(faction, make_direct_damage_spell("Boom!", faction, 2, 18, 2, 0, false, false, false, false, 3)); // Blast(3)
    // Cost 3
    register_spell(faction, make_buff_spell("Shroud Field", faction, 3, 12, RuleId::None, 0, 1)); // +1 to defense rolls
    register_spell(faction, make_direct_damage_spell("Pow!", faction, 3, 6, 3, 2)); // AP(2), Skewer
}

// ==============================================================================
// Havoc Brothers Spells
// ==============================================================================
void SpellRegistry::register_havoc_brothers_spells() {
    const char* faction = "Havoc Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Cursed Stride", faction, 1, 18)); // Dangerous Terrain
    register_spell(faction, make_direct_damage_spell("Havoc Trauma", faction, 1, 12, 2, 2)); // AP(2)
    // Cost 2
    register_spell(faction, make_buff_spell("Dark Assault", faction, 2, 12)); // Shred in melee
    register_spell(faction, make_direct_damage_spell("Havoc Terror", faction, 2, 9, 4)); // 4 hits each to up to 2 units
    // Cost 3
    register_spell(faction, make_buff_spell("Havoc Boon", faction, 3, 12)); // Havocbound Boost
    register_spell(faction, make_direct_damage_spell("Havoc Fog", faction, 3, 12, 6, 1)); // AP(1), Slam
}

// ==============================================================================
// Change Disciples Spells
// ==============================================================================
void SpellRegistry::register_change_disciples_spells() {
    const char* faction = "Change Disciples";
    // Cost 1
    register_spell(faction, make_debuff_spell("Shifting Form", faction, 1, 18)); // -3 to casting rolls
    register_spell(faction, make_direct_damage_spell("Sky Blaze", faction, 1, 12, 2, 1)); // AP(1), Slash
    // Cost 2
    register_spell(faction, make_buff_spell("Breath of Change", faction, 2, 12, RuleId::Bane)); // Bane when Shooting
    register_spell(faction, make_direct_damage_spell("Mutating Inferno", faction, 2, 9, 4)); // 4 hits each to up to 2 units
    // Cost 3
    register_spell(faction, make_buff_spell("Change Boon", faction, 3, 12)); // Changebound Boost
    register_spell(faction, make_direct_damage_spell("Power Bolt", faction, 3, 12, 6, 2)); // AP(2)
}

// ==============================================================================
// Lust Disciples Spells
// ==============================================================================
void SpellRegistry::register_lust_disciples_spells() {
    const char* faction = "Lust Disciples";
    // Cost 1
    register_spell(faction, make_debuff_spell("Combat Ecstasy", faction, 1, 18)); // Quick Shot
    register_spell(faction, make_direct_damage_spell("Beautiful Pain", faction, 1, 12, 2)); // Shatter
    // Cost 2
    register_spell(faction, make_buff_spell("Blissful Dance", faction, 2, 12)); // Melee Evasion
    register_spell(faction, make_direct_damage_spell("Total Seizure", faction, 2, 18, 4, 1)); // AP(1)
    // Cost 3
    register_spell(faction, make_buff_spell("Lust Boon", faction, 3, 12)); // Lustbound Boost
    register_spell(faction, make_direct_damage_spell("Overpowering Lash", faction, 3, 12, 6, 1, false, false, true)); // AP(1), Shred
}

// ==============================================================================
// Plague Disciples Spells
// ==============================================================================
void SpellRegistry::register_plague_disciples_spells() {
    const char* faction = "Plague Disciples";
    // Cost 1
    register_spell(faction, make_debuff_spell("Aura of Pestilence", faction, 1, 18)); // Difficult Terrain
    register_spell(faction, make_direct_damage_spell("Rapid Putrefaction", faction, 1, 12, 2, 1)); // AP(1), Butcher
    // Cost 2
    register_spell(faction, make_buff_spell("Blessed Virus", faction, 2, 12)); // Rapid Rush
    register_spell(faction, make_direct_damage_spell("Plague Malediction", faction, 2, 24, 2, 4)); // AP(4)
    // Cost 3
    register_spell(faction, make_buff_spell("Plague Boon", faction, 3, 12)); // Plaguebound Boost
    register_spell(faction, make_direct_damage_spell("Rot Wave", faction, 3, 18, 6));
}

// ==============================================================================
// War Disciples Spells
// ==============================================================================
void SpellRegistry::register_war_disciples_spells() {
    const char* faction = "War Disciples";
    // Cost 1
    register_spell(faction, make_morale_spell("Terrifying Fury", faction, 1, 18)); // Morale test
    register_spell(faction, make_direct_damage_spell("Flame of Destruction", faction, 1, 18, 1, 0, false, false, false, false, 3)); // Blast(3)
    // Cost 2
    register_spell(faction, make_buff_spell("Fiery Protection", faction, 2, 12)); // Melee Evasion
    register_spell(faction, make_direct_damage_spell("Brutal Massacre", faction, 2, 6, 6)); // Break
    // Cost 3
    register_spell(faction, make_buff_spell("War Boon", faction, 3, 12)); // Warbound Boost
    register_spell(faction, make_direct_damage_spell("Headtaker Strike", faction, 3, 12, 3, 2)); // AP(2), hits 2 units
}

// ==============================================================================
// High Elf Fleets Spells
// ==============================================================================
void SpellRegistry::register_high_elf_fleets_spells() {
    const char* faction = "High Elf Fleets";
    // Cost 1
    register_spell(faction, make_debuff_spell("Creator of Illusions", faction, 1, 18)); // Unwieldy in melee
    register_spell(faction, make_direct_damage_spell("Elemental Seeker", faction, 1, 18, 1, 0, false, false, false, false, 3)); // Blast(3)
    // Cost 2
    register_spell(faction, make_buff_spell("Hidden Spirits", faction, 2, 12)); // Unpredictable Shooter
    register_spell(faction, make_direct_damage_spell("Psy-Destruction", faction, 2, 24, 2, 4)); // AP(4)
    // Cost 3
    register_spell(faction, make_buff_spell("Blessing of Souls", faction, 3, 12)); // Highborn Boost
    register_spell(faction, make_direct_damage_spell("Shattering Curse", faction, 3, 12, 6, 1)); // AP(1), Crack
}

// ==============================================================================
// Human Defense Force Spells
// ==============================================================================
void SpellRegistry::register_human_defense_force_spells() {
    const char* faction = "Human Defense Force";
    // Cost 1
    register_spell(faction, make_buff_spell("Psy-Injected Courage", faction, 1, 12, RuleId::HoldTheLine));
    register_spell(faction, make_direct_damage_spell("Electric Tempest", faction, 1, 12, 2, 1)); // AP(1), Fracture
    // Cost 2
    register_spell(faction, make_debuff_spell("Calculated Foresight", faction, 2, 18)); // Relentless against
    register_spell(faction, make_direct_damage_spell("Searing Burst", faction, 2, 12, 6));
    // Cost 3
    register_spell(faction, make_buff_spell("Shock Speed", faction, 3, 12, RuleId::Fast)); // +2" advance, +4" rush/charge
    register_spell(faction, make_direct_damage_spell("Expel Threat", faction, 3, 18, 6, 1)); // AP(1)
}

// ==============================================================================
// Human Inquisition Spells
// ==============================================================================
void SpellRegistry::register_human_inquisition_spells() {
    const char* faction = "Human Inquisition";
    // Cost 1
    register_spell(faction, make_buff_spell("Psy-Injected Courage", faction, 1, 12, RuleId::None, 0, 0)); // +1 to morale test
    register_spell(faction, make_direct_damage_spell("Electric Tempest", faction, 1, 12, 2, 1, false, false, false, true)); // AP(1), Surge
    // Cost 2
    register_spell(faction, make_debuff_spell("Calculated Foresight", faction, 2, 18)); // Relentless against
    register_spell(faction, make_direct_damage_spell("Searing Burst", faction, 2, 12, 6));
    // Cost 3
    register_spell(faction, make_buff_spell("Shock Speed", faction, 3, 12, RuleId::Fast)); // +2" advance, +4" rush/charge
    register_spell(faction, make_direct_damage_spell("Expel Threat", faction, 3, 18, 6, 1)); // AP(1)
}

// ==============================================================================
// Infected Colonies Spells
// ==============================================================================
void SpellRegistry::register_infected_colonies_spells() {
    const char* faction = "Infected Colonies";
    // Cost 1
    register_spell(faction, make_buff_spell("Violent Onslaught", faction, 1, 12)); // Infected Boost
    register_spell(faction, make_direct_damage_spell("Bio-Horror", faction, 1, 12, 2, 1, false, false, false, true)); // AP(1), Surge
    // Cost 2
    register_spell(faction, make_debuff_spell("Brain Infestation", faction, 2, 18, -1)); // -1 to hit rolls
    register_spell(faction, make_direct_damage_spell("Spread Plague", faction, 2, 18, 2)); // Bash
    // Cost 3
    register_spell(faction, make_buff_spell("Rapid Mutation", faction, 3, 12, RuleId::Regeneration));
    register_spell(faction, make_direct_damage_spell("Volatile Infection", faction, 3, 6, 3, 2, false, false, false, false, 0, 3)); // AP(2), Deadly(3)
}

// ==============================================================================
// Jackals Spells
// ==============================================================================
void SpellRegistry::register_jackals_spells() {
    const char* faction = "Jackals";
    // Cost 1
    register_spell(faction, make_buff_spell("Psy-Hunter", faction, 1, 12)); // Scrapper Boost
    register_spell(faction, make_direct_damage_spell("Power Maw", faction, 1, 12, 1, 2)); // AP(2), hits 2 units
    // Cost 2
    register_spell(faction, make_debuff_spell("Mind Shaper", faction, 2, 18)); // -1 to morale test
    register_spell(faction, make_direct_damage_spell("Quill Blast", faction, 2, 12, 4)); // Scratch
    // Cost 3
    register_spell(faction, make_buff_spell("Power Field", faction, 3, 12, RuleId::Shielded));
    register_spell(faction, make_direct_damage_spell("Feral Strike", faction, 3, 6, 3, 2, false, false, false, false, 0, 3)); // AP(2), Deadly(3)
}

// ==============================================================================
// Machine Cult Spells
// ==============================================================================
void SpellRegistry::register_machine_cult_spells() {
    const char* faction = "Machine Cult";
    // Cost 1
    register_spell(faction, make_buff_spell("Cyborg Assault", faction, 1, 12, RuleId::HitAndRun)); // Hit & Run Shooter
    register_spell(faction, make_direct_damage_spell("Power Beam", faction, 1, 18, 2, 1)); // AP(1)
    // Cost 2
    register_spell(faction, make_buff_spell("Shrouding Incense", faction, 2, 12)); // Machine-Fog Boost
    register_spell(faction, make_direct_damage_spell("Searing Shrapnel", faction, 2, 12, 4, 1)); // AP(1), Wreck
    // Cost 3
    register_spell(faction, make_debuff_spell("Corrode Weapons", faction, 3, 18, 0, 0, -1)); // Loses AP(1)
    register_spell(faction, make_direct_damage_spell("Crushing Force", faction, 3, 12, 6, 2)); // AP(2)
}

// ==============================================================================
// Orc Marauders Spells
// ==============================================================================
void SpellRegistry::register_orc_marauders_spells() {
    const char* faction = "Orc Marauders";
    // Cost 1
    register_spell(faction, make_buff_spell("Elder Protection", faction, 1, 12, RuleId::Resistance));
    register_spell(faction, make_direct_damage_spell("Death Bolt", faction, 1, 6, 1, 2)); // AP(2), Impale
    // Cost 2
    register_spell(faction, make_buff_spell("Path of War", faction, 2, 12)); // Ferocious Boost
    register_spell(faction, make_direct_damage_spell("Psychic Vomit", faction, 2, 6, 6, 0, false, true)); // Bane
    // Cost 3
    register_spell(faction, make_debuff_spell("Head Bang", faction, 3, 18)); // Rending in melee against
    register_spell(faction, make_direct_damage_spell("Crackling Bolt", faction, 3, 18, 3, 0, false, false, false, false, 3)); // Blast(3)
}

// ==============================================================================
// Prime Brothers Spells
// ==============================================================================
void SpellRegistry::register_prime_brothers_spells() {
    const char* faction = "Prime Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Advanced Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Mind Wound", faction, 1, 12, 2)); // Demolish
    // Cost 2
    register_spell(faction, make_buff_spell("Blessed Ammo", faction, 2, 12)); // Shred when shooting
    register_spell(faction, make_direct_damage_spell("Lightning Fog", faction, 2, 9, 4)); // 4 hits each to up to 2 units
    // Cost 3
    register_spell(faction, make_buff_spell("Protective Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Psychic Terror", faction, 3, 6, 9, 0, false, true)); // Bane
}

// ==============================================================================
// Blood Prime Brothers Spells
// ==============================================================================
void SpellRegistry::register_blood_prime_brothers_spells() {
    const char* faction = "Blood Prime Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Blood Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Blood Wound", faction, 1, 12, 2)); // Demolish
    // Cost 2
    register_spell(faction, make_buff_spell("Burst of Rage", faction, 2, 12, RuleId::Furious));
    register_spell(faction, make_direct_damage_spell("Heavenly Lance", faction, 2, 18, 4, 1)); // AP(1)
    // Cost 3
    register_spell(faction, make_buff_spell("Blood Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Shield Breaker", faction, 3, 12, 6, 1, false, false, true)); // AP(1), Shred
}

// ==============================================================================
// Dark Prime Brothers Spells
// ==============================================================================
void SpellRegistry::register_dark_prime_brothers_spells() {
    const char* faction = "Dark Prime Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Dark Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Dark Wound", faction, 1, 12, 2)); // Demolish
    // Cost 2
    register_spell(faction, make_buff_spell("Blessed Ammo", faction, 2, 12)); // Shred when shooting
    register_spell(faction, make_direct_damage_spell("Lightning Fog", faction, 2, 9, 4));
    // Cost 3
    register_spell(faction, make_buff_spell("Dark Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Psychic Terror", faction, 3, 6, 9, 0, false, true)); // Bane
}

// ==============================================================================
// Knight Prime Brothers Spells
// ==============================================================================
void SpellRegistry::register_knight_prime_brothers_spells() {
    const char* faction = "Knight Prime Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Knight Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Knight Wound", faction, 1, 12, 2)); // Demolish
    // Cost 2
    register_spell(faction, make_debuff_spell("Banishing Sigil", faction, 2, 18, 0, -1)); // -1 to defense rolls
    register_spell(faction, make_direct_damage_spell("Doom Strike", faction, 2, 6, 2, 2, false, false, false, false, 0, 3)); // AP(2), Deadly(3)
    // Cost 3
    register_spell(faction, make_buff_spell("Knight Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Purge the Impure", faction, 3, 12, 3, 2)); // AP(2)
}

// ==============================================================================
// Watch Prime Brothers Spells
// ==============================================================================
void SpellRegistry::register_watch_prime_brothers_spells() {
    const char* faction = "Watch Prime Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Watch Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Watch Wound", faction, 1, 12, 2)); // Demolish
    // Cost 2
    register_spell(faction, make_buff_spell("Blessed Ammo", faction, 2, 12)); // Shred when shooting
    register_spell(faction, make_direct_damage_spell("Lightning Fog", faction, 2, 9, 4));
    // Cost 3
    register_spell(faction, make_buff_spell("Watch Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Psychic Terror", faction, 3, 6, 9, 0, false, true)); // Bane
}

// ==============================================================================
// Wolf Prime Brothers Spells
// ==============================================================================
void SpellRegistry::register_wolf_prime_brothers_spells() {
    const char* faction = "Wolf Prime Brothers";
    // Cost 1
    register_spell(faction, make_debuff_spell("Wolf Sight", faction, 1, 18)); // Unstoppable when shooting
    register_spell(faction, make_direct_damage_spell("Wolf Wound", faction, 1, 12, 2)); // Demolish
    // Cost 2
    register_spell(faction, make_buff_spell("Righteous Fury", faction, 2, 12)); // Piercing Assault
    register_spell(faction, make_direct_damage_spell("Godly Thunder", faction, 2, 24, 2, 4)); // AP(4)
    // Cost 3
    register_spell(faction, make_buff_spell("Wolf Dome", faction, 3, 12)); // Evasive
    register_spell(faction, make_direct_damage_spell("Storm of Power", faction, 3, 12, 6, 1, false, false, false, true)); // AP(1), Surge
}

// ==============================================================================
// Ratmen Clans Spells
// ==============================================================================
void SpellRegistry::register_ratmen_clans_spells() {
    const char* faction = "Ratmen Clans";
    // Cost 1
    register_spell(faction, make_buff_spell("Weapon Booster", faction, 1, 12)); // Scurry Boost
    register_spell(faction, make_direct_damage_spell("Focused Shock", faction, 1, 12, 2, 1, false, false, true)); // AP(1), Shred
    // Cost 2
    register_spell(faction, make_debuff_spell("Tech-Sickness", faction, 2, 18, 0, -1)); // -1 to defense rolls
    register_spell(faction, make_direct_damage_spell("System Takeover", faction, 2, 12, 6));
    // Cost 3
    register_spell(faction, make_buff_spell("Enhance Serum", faction, 3, 12, RuleId::Regeneration));
    register_spell(faction, make_direct_damage_spell("Power Surge", faction, 3, 12, 6)); // Hazardous
}

// ==============================================================================
// Rebel Guerrillas Spells
// ==============================================================================
void SpellRegistry::register_rebel_guerrillas_spells() {
    const char* faction = "Rebel Guerrillas";
    // Cost 1
    register_spell(faction, make_buff_spell("Aura of Peace", faction, 1, 12, RuleId::None, 0, 0)); // +1 to morale test
    register_spell(faction, make_direct_damage_spell("Mind Breaker", faction, 1, 12, 2, 1, false, false, false, true)); // AP(1), Surge
    // Cost 2
    register_spell(faction, make_debuff_spell("Bad Omen", faction, 2, 18)); // Furious against
    register_spell(faction, make_direct_damage_spell("Wave of Discord", faction, 2, 18, 2)); // Thrash
    // Cost 3
    register_spell(faction, make_buff_spell("Deep Meditation", faction, 3, 12, RuleId::None, 1)); // +1 to hit when shooting
    register_spell(faction, make_direct_damage_spell("Piercing Pulse", faction, 3, 24, 3, 4)); // AP(4)
}

// ==============================================================================
// Robot Legions Spells
// ==============================================================================
void SpellRegistry::register_robot_legions_spells() {
    const char* faction = "Robot Legions";
    // Cost 1
    register_spell(faction, make_debuff_spell("Triangulation Bots", faction, 1, 18)); // Indirect when shooting
    register_spell(faction, make_direct_damage_spell("Gauss Pulse", faction, 1, 24, 1)); // Puncture
    // Cost 2
    register_spell(faction, make_buff_spell("Phase Shield", faction, 2, 12)); // Phase Boost
    register_spell(faction, make_direct_damage_spell("Anti-Matter Beam", faction, 2, 12, 2, 2)); // AP(2)
    // Cost 3
    register_spell(faction, make_buff_spell("Repair Protocols", faction, 3, 12, RuleId::Regeneration));
    register_spell(faction, make_direct_damage_spell("Annihilation Blast", faction, 3, 12, 6, 1)); // AP(1)
}

// ==============================================================================
// Saurian Starhost Spells
// ==============================================================================
void SpellRegistry::register_saurian_starhost_spells() {
    const char* faction = "Saurian Starhost";
    // Cost 1
    register_spell(faction, make_buff_spell("Cold Blood Focus", faction, 1, 12)); // Cold Blooded Boost
    register_spell(faction, make_direct_damage_spell("Stellar Bolt", faction, 1, 18, 2, 1)); // AP(1)
    // Cost 2
    register_spell(faction, make_buff_spell("Primordial Strength", faction, 2, 12, RuleId::Furious));
    register_spell(faction, make_direct_damage_spell("Solar Flare", faction, 2, 12, 4, 2)); // AP(2)
    // Cost 3
    register_spell(faction, make_debuff_spell("Cosmic Terror", faction, 3, 18)); // Fear against
    register_spell(faction, make_direct_damage_spell("Meteor Storm", faction, 3, 18, 3, 0, false, false, false, false, 3)); // Blast(3)
}

// ==============================================================================
// Soul Snatcher Cults Spells
// ==============================================================================
void SpellRegistry::register_soul_snatcher_cults_spells() {
    const char* faction = "Soul Snatcher Cults";
    // Cost 1
    register_spell(faction, make_buff_spell("Soul Drain", faction, 1, 12, RuleId::Regeneration));
    register_spell(faction, make_direct_damage_spell("Soul Bolt", faction, 1, 18, 2, 1)); // AP(1)
    // Cost 2
    register_spell(faction, make_debuff_spell("Mind Leech", faction, 2, 18, -1)); // -1 to hit
    register_spell(faction, make_direct_damage_spell("Soul Rend", faction, 2, 12, 4)); // Soul damage
    // Cost 3
    register_spell(faction, make_buff_spell("Ethereal Form", faction, 3, 12)); // Phase ability
    register_spell(faction, make_direct_damage_spell("Soul Storm", faction, 3, 12, 6, 1, false, true)); // AP(1), Bane
}

// ==============================================================================
// Wormhole Daemons (all variants)
// ==============================================================================
void SpellRegistry::register_wormhole_daemons_spells() {
    // Wormhole Daemons Change
    {
        const char* faction = "Wormhole Daemons Change";
        register_spell(faction, make_debuff_spell("Warp Shift", faction, 1, 18)); // Phase shift
        register_spell(faction, make_direct_damage_spell("Warp Bolt", faction, 1, 18, 3));
        register_spell(faction, make_buff_spell("Change Shield", faction, 2, 12)); // Protection
        register_spell(faction, make_direct_damage_spell("Warp Fire", faction, 2, 12, 6));
        register_spell(faction, make_buff_spell("Warp Boon", faction, 3, 12)); // Changebound Boost
        register_spell(faction, make_direct_damage_spell("Warp Storm", faction, 3, 9, 9));
    }

    // Wormhole Daemons Lust
    {
        const char* faction = "Wormhole Daemons Lust";
        register_spell(faction, make_buff_spell("Seductive Aura", faction, 1, 12, RuleId::Counter));
        register_spell(faction, make_direct_damage_spell("Lust Bolt", faction, 1, 12, 2));
        register_spell(faction, make_buff_spell("Dance of Death", faction, 2, 12, RuleId::Fast));
        register_spell(faction, make_direct_damage_spell("Lust Fire", faction, 2, 12, 4, 1)); // AP(1)
        register_spell(faction, make_buff_spell("Lust Boon", faction, 3, 12)); // Lustbound Boost
        register_spell(faction, make_direct_damage_spell("Lust Storm", faction, 3, 12, 6, 1, false, false, true)); // AP(1), Shred
    }

    // Wormhole Daemons Plague
    {
        const char* faction = "Wormhole Daemons Plague";
        register_spell(faction, make_debuff_spell("Plague Touch", faction, 1, 18)); // Poison mark
        register_spell(faction, make_direct_damage_spell("Plague Bolt", faction, 1, 12, 2, 1)); // AP(1)
        register_spell(faction, make_buff_spell("Plague Shield", faction, 2, 12, RuleId::Regeneration));
        register_spell(faction, make_direct_damage_spell("Plague Fire", faction, 2, 24, 2, 4)); // AP(4)
        register_spell(faction, make_buff_spell("Plague Boon", faction, 3, 12)); // Plaguebound Boost
        register_spell(faction, make_direct_damage_spell("Plague Storm", faction, 3, 18, 6));
    }

    // Wormhole Daemons War
    {
        const char* faction = "Wormhole Daemons War";
        register_spell(faction, make_morale_spell("Battle Cry", faction, 1, 18)); // Morale test
        register_spell(faction, make_direct_damage_spell("War Bolt", faction, 1, 18, 1, 0, false, false, false, false, 3)); // Blast(3)
        register_spell(faction, make_buff_spell("Rage Shield", faction, 2, 12, RuleId::Furious));
        register_spell(faction, make_direct_damage_spell("War Fire", faction, 2, 6, 6)); // Break
        register_spell(faction, make_buff_spell("War Boon", faction, 3, 12)); // Warbound Boost
        register_spell(faction, make_direct_damage_spell("War Storm", faction, 3, 12, 3, 2)); // AP(2), hits 2 units
    }
}

} // namespace battle
