#!/usr/bin/env python3
"""
Generate an Excel review sheet with all chunk_sim special rules.
"""

from openpyxl import Workbook
from openpyxl.styles import Font, Fill, PatternFill, Border, Side, Alignment
from openpyxl.utils import get_column_letter

def create_rules_sheet():
    wb = Workbook()

    # =============================================
    # SHEET 1: Core Special Rules Overview
    # =============================================
    ws1 = wb.active
    ws1.title = "Core Rules Overview"

    # Define styles
    header_font = Font(bold=True, color="FFFFFF", size=11)
    header_fill = PatternFill(start_color="4472C4", end_color="4472C4", fill_type="solid")
    category_fill = PatternFill(start_color="B4C6E7", end_color="B4C6E7", fill_type="solid")
    implemented_fill = PatternFill(start_color="C6EFCE", end_color="C6EFCE", fill_type="solid")
    partial_fill = PatternFill(start_color="FFEB9C", end_color="FFEB9C", fill_type="solid")
    not_impl_fill = PatternFill(start_color="FFC7CE", end_color="FFC7CE", fill_type="solid")
    thin_border = Border(
        left=Side(style='thin'),
        right=Side(style='thin'),
        top=Side(style='thin'),
        bottom=Side(style='thin')
    )

    # Headers
    headers = ["Rule Name", "Category", "Description/Effect", "Parameters", "Application Phase", "Implementation Status", "Source Location"]
    for col, header in enumerate(headers, 1):
        cell = ws1.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.fill = header_fill
        cell.border = thin_border
        cell.alignment = Alignment(horizontal='center', vertical='center', wrap_text=True)

    # Core Rules Data
    rules_data = [
        # WEAPON RULES
        ["AP (Armor Piercing)", "Weapon", "Reduces enemy defense by AP value", "Numeric (1-4)", "All attack phases", "✅ Implemented", "types.hpp:104"],
        ["Blast(X)", "Weapon", "Multiply hits by X (capped by model count)", "Numeric (2-3)", "After hit roll", "✅ Implemented", "types.hpp:105"],
        ["Deadly(X)", "Weapon", "Multiply wounds by X", "Numeric", "After wound calculation", "✅ Implemented", "types.hpp:106"],
        ["Lance", "Weapon", "+2 AP on charge attacks", "+2 (fixed)", "Charge phase only", "✅ Implemented", "types.hpp:107"],
        ["Poison", "Weapon", "Reroll defense 6s", "None", "Defense rolls", "✅ Implemented", "types.hpp:108"],
        ["Precise", "Weapon", "+1 to hit", "+1 (fixed)", "All attack phases", "✅ Implemented", "types.hpp:109"],
        ["Reliable", "Weapon", "Quality becomes 2+", "Quality threshold", "All attack phases", "✅ Implemented", "types.hpp:110"],
        ["Rending", "Weapon", "Unmodified 6s to hit get AP(4)", "AP(4)", "Hit roll phase", "⚠️ Partial", "types.hpp:111"],
        ["Bane", "Weapon", "Bypass regeneration, reroll defense 6s", "None", "Wound allocation", "✅ Implemented", "types.hpp:112"],
        ["Impact(X)", "Weapon", "X extra attacks on charge", "Numeric", "Charge phase only", "⚠️ Partial", "types.hpp:113"],
        ["Indirect", "Weapon", "Ignore cover", "None", "Shooting phase", "⚠️ Partial", "types.hpp:114"],
        ["Sniper", "Weapon", "Pick target model", "None", "Target declaration", "❌ Not Implemented", "types.hpp:115"],
        ["Lock_On", "Weapon", "+1 to hit vs vehicles", "+1 (conditional)", "Shooting phase", "❌ Not Implemented", "types.hpp:116"],
        ["Purge", "Weapon", "+1 to hit vs Tough(3+)", "+1 (conditional)", "All attack phases", "❌ Not Implemented", "types.hpp:117"],
        ["Surge", "Weapon", "6s to hit deal 1 extra hit", "+1 hit on 6s", "Hit roll phase", "❌ Not Implemented", "types.hpp:143"],
        ["Thrust", "Weapon", "+1 to hit and AP(+1) when charging", "+1/+1", "Charge phase", "❌ Not Implemented", "types.hpp:144"],
        ["Takedown", "Weapon", "Pick target model, resolve as unit of 1", "None", "Target declaration", "✅ Implemented", "types.hpp:145"],
        ["Limited", "Weapon", "Weapon may only be used once per game", "Once per game", "Weapon usage", "❌ Not Implemented", "types.hpp:146"],

        # DEFENSE RULES
        ["Regeneration", "Defense", "5+ save to ignore wound", "5+ (fixed)", "Wound allocation", "✅ Implemented", "types.hpp:120"],
        ["Tough(X)", "Defense", "X wounds to kill model", "Numeric (1-6)", "Wound allocation", "✅ Implemented", "types.hpp:121"],
        ["Protected", "Defense", "6+ to reduce AP by 1", "6+ threshold", "Defense roll phase", "❌ Not Implemented", "types.hpp:122"],
        ["Stealth", "Defense", "-1 to be hit from >12\" away", "-1 (range: 12\"+)", "Shooting phase", "❌ Not Implemented", "types.hpp:123"],
        ["ShieldWall", "Defense", "+1 Defense in melee", "+1 (melee only)", "Melee phase", "✅ Implemented", "types.hpp:124"],
        ["Shielded", "Defense", "+1 defense vs non-spell hits", "+1 (conditional)", "Defense rolls", "✅ Implemented", "types.hpp:149"],
        ["Resistance", "Defense", "6+ ignore wounds (2+ vs spells)", "6+/2+ (conditional)", "Wound allocation", "✅ Implemented", "types.hpp:150"],

        # UNIT BEHAVIOR RULES
        ["Fearless", "Unit Behavior", "Reroll failed morale tests", "None", "Morale phase", "✅ Implemented", "types.hpp:127"],
        ["Furious", "Unit Behavior", "Extra hits on 6s when charging", "6s = extra hits", "Melee charge phase", "✅ Implemented", "types.hpp:128"],
        ["Hero", "Unit Behavior", "Takes wounds last", "None", "Wound allocation", "✅ Implemented", "types.hpp:129"],
        ["Relentless", "Unit Behavior", "Extra hits on 6s shooting >9\"", "6s trigger extra hits", "Shooting phase", "⚠️ Partial", "types.hpp:130"],
        ["Fear(X)", "Unit Behavior", "Counts as +X wounds in melee for morale", "Numeric (1-4)", "Morale phase", "✅ Implemented", "types.hpp:131"],
        ["Counter", "Unit Behavior", "Strikes first when charged", "None", "Melee phase", "✅ Implemented", "types.hpp:132"],
        ["Fast", "Movement", "9\" move instead of 6\"", "9\" movement", "Movement phase", "✅ Implemented", "types.hpp:133"],
        ["Flying", "Movement", "Can fly over terrain/units", "None", "Movement phase", "✅ Implemented", "types.hpp:134"],
        ["Strider", "Movement", "Ignore difficult terrain", "None", "Movement phase", "✅ Implemented", "types.hpp:135"],
        ["Scout", "Deployment", "Deploy 12\" ahead", "12\" deployment", "Deployment phase", "✅ Implemented", "types.hpp:136"],
        ["Ambush", "Deployment", "Can deploy >9\" from enemy", "9\" range", "Deployment phase", "✅ Implemented", "types.hpp:137"],
        ["Devout", "Unit Behavior", "Faction-specific rule bonus", "Varies by faction", "All phases", "✅ Implemented", "types.hpp:138"],
        ["PiercingAssault", "Weapon", "AP(1) on melee in charge", "AP(1)", "Charge phase", "✅ Implemented", "types.hpp:139"],
        ["Unstoppable", "Defense", "Ignore regen and negative modifiers", "None", "Defense resolution", "✅ Implemented", "types.hpp:140"],
        ["Casting", "Special", "Can cast X spells", "Numeric (1-3)", "Spell casting phase", "✅ Implemented", "types.hpp:141"],
        ["Slow", "Movement", "4\" move instead of 6\"", "4\" movement", "Movement phase", "✅ Implemented", "types.hpp:142"],

        # FACTION-SPECIFIC RULES
        ["NoRetreat", "Faction", "Can't be shaken/routed, take wounds instead", "None", "Morale/routing phase", "✅ Implemented", "types.hpp:151"],
        ["MoraleBoost", "Faction", "+1 to morale test rolls", "+1", "Morale phase", "✅ Implemented", "types.hpp:152"],
        ["Rupture", "Faction", "Ignore regen, extra wound on unmodified 6 to hit", "+1 wound per 6", "Hit roll phase", "✅ Implemented", "types.hpp:153"],
        ["Agile", "Movement", "+1\" advance, +2\" rush/charge", "+1\"/+2\"", "Movement phase", "✅ Implemented", "types.hpp:154"],
        ["HitAndRun", "Faction", "Can retreat after fighting", "None", "Melee resolution", "✅ Implemented", "types.hpp:155"],
        ["PointBlankSurge", "Weapon", "6s to hit deal extra hit at short range (0-9\")", "+1 hit at 0-9\"", "Shooting phase", "❌ Not Implemented", "types.hpp:156"],
        ["Shred", "Weapon", "Extra wound on unmodified 1 to block", "+1 wound", "Defense roll phase", "✅ Implemented", "types.hpp:157"],
        ["Smash", "Weapon", "Ignore regen, +Blast(3) vs Defense 5+/6+", "Blast(3)", "Wound phase", "✅ Implemented", "types.hpp:158"],
        ["Battleborn", "Faction", "4+ to stop being Shaken at round start", "4+ threshold", "Round start/morale", "✅ Implemented", "types.hpp:159"],
        ["PredatorFighter", "Faction", "6s in melee generate extra attacks", "Recursive extra attacks", "Melee phase", "✅ Implemented", "types.hpp:160"],
        ["RapidCharge", "Movement", "+4\" charge move", "+4\" movement", "Charge phase", "✅ Implemented", "types.hpp:161"],
        ["SelfDestruct", "Faction", "Deal X hits to attacker when killed in melee", "X hits", "Model death/melee", "✅ Implemented", "types.hpp:162"],
        ["VersatileAttack", "Weapon", "Choose AP+1 or +1 to hit each activation", "+1 (conditional)", "Attack declaration", "✅ Implemented", "types.hpp:163"],
        ["GoodShot", "Weapon", "+1 to hit when shooting", "+1 (shooting only)", "Shooting phase", "✅ Implemented", "types.hpp:164"],
        ["BadShot", "Weapon", "-1 to hit when shooting", "-1 (shooting only)", "Shooting phase", "✅ Implemented", "types.hpp:165"],
        ["MeleeEvasion", "Defense", "-1 to be hit in melee", "-1 (melee only)", "Melee phase", "✅ Implemented", "types.hpp:166"],
        ["MeleeShrouding", "Defense", "-1 to be hit in melee", "-1 (melee only)", "Melee phase", "✅ Implemented", "types.hpp:167"],
        ["RangedShrouding", "Defense", "-1 to be hit when shooting at this unit", "-1 (shooting only)", "Shooting phase", "✅ Implemented", "types.hpp:168"],
        ["BaneInMelee", "Weapon", "All melee attacks have Bane", "None", "Melee phase", "✅ Implemented", "types.hpp:169"],
        ["HoldTheLine", "Faction", "Reroll failed morale tests", "None", "Morale phase", "✅ Implemented", "types.hpp:170"],
    ]

    # Add data to sheet
    for row_idx, row_data in enumerate(rules_data, 2):
        for col_idx, value in enumerate(row_data, 1):
            cell = ws1.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border
            cell.alignment = Alignment(vertical='center', wrap_text=True)

            # Apply status coloring
            if col_idx == 6:  # Implementation Status column
                if "✅" in value:
                    cell.fill = implemented_fill
                elif "⚠️" in value:
                    cell.fill = partial_fill
                elif "❌" in value:
                    cell.fill = not_impl_fill

    # Set column widths
    ws1.column_dimensions['A'].width = 20
    ws1.column_dimensions['B'].width = 15
    ws1.column_dimensions['C'].width = 45
    ws1.column_dimensions['D'].width = 20
    ws1.column_dimensions['E'].width = 20
    ws1.column_dimensions['F'].width = 20
    ws1.column_dimensions['G'].width = 18

    # =============================================
    # SHEET 2: Aura Rules
    # =============================================
    ws2 = wb.create_sheet("Aura Rules")

    aura_headers = ["Aura Name", "Effect Granted", "Description", "Source Location"]
    for col, header in enumerate(aura_headers, 1):
        cell = ws2.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.fill = header_fill
        cell.border = thin_border
        cell.alignment = Alignment(horizontal='center', vertical='center', wrap_text=True)

    aura_data = [
        ["Furious Aura", "Furious", "Grants Furious rule - extra hits on 6s when charging", "unit_parser.hpp:225"],
        ["Shielded Aura", "Shielded", "Grants +1 defense vs non-spell hits", "unit_parser.hpp:226"],
        ["Regeneration Aura", "Regeneration", "Grants 5+ save to ignore wounds", "unit_parser.hpp:227"],
        ["Relentless Aura", "Relentless", "Grants extra hits on 6s when shooting >9\"", "unit_parser.hpp:228"],
        ["Scout Aura", "Scout", "Grants Scout deployment ability", "unit_parser.hpp:229"],
        ["Stealth Aura", "Stealth", "Grants -1 to be hit from >12\" away", "unit_parser.hpp:230"],
        ["Counter-Attack Aura", "Counter", "Grants strike first when charged", "unit_parser.hpp:231-233"],
        ["Fearless Aura", "Fearless", "Grants reroll failed morale tests", "unit_parser.hpp:234"],
        ["Ambush Aura", "Ambush", "Grants ability to deploy >9\" from enemy", "unit_parser.hpp:235"],
    ]

    for row_idx, row_data in enumerate(aura_data, 2):
        for col_idx, value in enumerate(row_data, 1):
            cell = ws2.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border
            cell.alignment = Alignment(vertical='center', wrap_text=True)

    ws2.column_dimensions['A'].width = 22
    ws2.column_dimensions['B'].width = 18
    ws2.column_dimensions['C'].width = 50
    ws2.column_dimensions['D'].width = 22

    # =============================================
    # SHEET 3: Faction Army Rules
    # =============================================
    ws3 = wb.create_sheet("Faction Army Rules")

    faction_headers = ["Faction", "Rule Name", "Rule Type", "Effect Description", "Trigger Timing", "Source Lines"]
    for col, header in enumerate(faction_headers, 1):
        cell = ws3.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.fill = header_fill
        cell.border = thin_border
        cell.alignment = Alignment(horizontal='center', vertical='center', wrap_text=True)

    faction_data = [
        ["Alien Hives", "Hive Bond", "ArmyWide", "Grants MoraleBoost to all units", "Always", "79-123"],
        ["Alien Hives", "Psychic Blast", "Aura", "Deals 3 wounds with AP(1)", "WhenAttacking", "79-123"],
        ["Battle Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken at round start", "StartOfRound", "126-150"],
        ["Battle Brothers", "Furious Aura", "Aura", "Grants Furious to nearby units", "Always", "126-150"],
        ["Blessed Sisters", "Devout", "ArmyWide", "Grants Devout rule", "Always", "153-184"],
        ["Blessed Sisters", "Furious Aura", "Aura", "Grants Furious to nearby units", "Always", "153-184"],
        ["Blood Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "187-212"],
        ["Blood Brothers", "Furious", "Special", "Extra hits on 6s when charging", "OnCharge", "187-212"],
        ["Blood Prime Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "215-245"],
        ["Blood Prime Brothers", "Agile", "Special", "+1\" advance, +2\" rush/charge", "Always", "215-245"],
        ["Change Disciples", "Resistance", "ArmyWide", "6+ ignore wounds (2+ vs spells)", "WhenDefending", "248-272"],
        ["Custodian Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "275-303"],
        ["Custodian Brothers", "Shielded", "Special", "+1 defense vs non-spell hits", "WhenDefending", "275-303"],
        ["DAO Union", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "306-333"],
        ["DAO Union", "Good Shot", "Special", "+1 to hit when shooting", "WhenAttacking", "306-333"],
        ["Dark Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "336-363"],
        ["Dark Brothers", "Stealth", "Special", "-1 to be hit from >12\"", "WhenDefending", "336-363"],
        ["Dark Elf Raiders", "Agile", "ArmyWide", "+1\" advance, +2\" rush/charge", "Always", "366-392"],
        ["Dark Elf Raiders", "Hit and Run", "Special", "Can retreat after fighting", "Always", "366-392"],
        ["Dark Prime Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "395-425"],
        ["Dwarf Guilds", "Hold The Line", "ArmyWide", "Reroll failed morale tests", "Always", "428-457"],
        ["Dwarf Guilds", "Shielded", "Special", "+1 defense vs non-spell hits", "WhenDefending", "428-457"],
        ["Elven Jesters", "Agile", "ArmyWide", "+1\" advance, +2\" rush/charge", "Always", "460-481"],
        ["Eternal Dynasty", "No Retreat", "ArmyWide", "Can't be shaken/routed", "Always", "484-518"],
        ["Eternal Dynasty", "Regeneration", "Special", "5+ save to ignore wound", "WhenDefending", "484-518"],
        ["Goblin Reclaimers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "521-547"],
        ["Havoc Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "550-574"],
        ["Havoc Brothers", "Furious", "Special", "Extra hits on 6s when charging", "OnCharge", "550-574"],
        ["High Elf Fleets", "Agile", "ArmyWide", "+1\" advance, +2\" rush/charge", "Always", "577-605"],
        ["High Elf Fleets", "Counter", "Special", "Strikes first when charged", "OnBeingCharged", "577-605"],
        ["Human Defense Force", "Hold The Line", "ArmyWide", "Reroll failed morale tests", "Always", "608-635"],
        ["Human Inquisition", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "638-679"],
        ["Human Inquisition", "Devout", "Special", "Faction-specific rule bonus", "Always", "638-679"],
        ["Infected Colonies", "Fearless", "ArmyWide", "Reroll failed morale tests", "Always", "682-702"],
        ["Infected Colonies", "Regeneration", "Special", "5+ save to ignore wound", "WhenDefending", "682-702"],
        ["Jackals", "Agile", "ArmyWide", "+1\" advance, +2\" rush/charge", "Always", "705-733"],
        ["Jackals", "Hit and Run", "Special", "Can retreat after fighting", "Always", "705-733"],
        ["Knight Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "736-763"],
        ["Knight Prime Brothers", "Battleborn", "ArmyWide", "4+ to stop being Shaken", "StartOfRound", "766-796"],
        ["Lust Disciples", "Agile", "ArmyWide", "+1\" advance, +2\" rush/charge", "Always", "799+"],
    ]

    for row_idx, row_data in enumerate(faction_data, 2):
        for col_idx, value in enumerate(row_data, 1):
            cell = ws3.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border
            cell.alignment = Alignment(vertical='center', wrap_text=True)

    ws3.column_dimensions['A'].width = 22
    ws3.column_dimensions['B'].width = 18
    ws3.column_dimensions['C'].width = 12
    ws3.column_dimensions['D'].width = 40
    ws3.column_dimensions['E'].width = 18
    ws3.column_dimensions['F'].width = 15

    # =============================================
    # SHEET 4: Rule Categories Summary
    # =============================================
    ws4 = wb.create_sheet("Categories Summary")

    cat_headers = ["Category", "Count", "Description", "Examples"]
    for col, header in enumerate(cat_headers, 1):
        cell = ws4.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.fill = header_fill
        cell.border = thin_border
        cell.alignment = Alignment(horizontal='center', vertical='center', wrap_text=True)

    cat_data = [
        ["Weapon Rules", "18", "Rules that modify weapon attacks (hit rolls, wounds, AP)", "AP, Blast, Deadly, Lance, Poison, Rending"],
        ["Defense Rules", "7", "Rules that modify defense and survival", "Regeneration, Tough, Stealth, ShieldWall, Shielded"],
        ["Unit Behavior", "11", "Rules that affect unit actions and morale", "Fearless, Furious, Hero, Counter, Relentless"],
        ["Movement", "6", "Rules that modify movement capabilities", "Fast, Slow, Flying, Strider, Agile, RapidCharge"],
        ["Deployment", "2", "Rules that affect deployment", "Scout, Ambush"],
        ["Faction-Specific", "14", "Rules unique to certain factions", "NoRetreat, Battleborn, PredatorFighter, HitAndRun"],
        ["Aura Effects", "9", "Rules that grant effects to nearby units", "Furious Aura, Shielded Aura, Regeneration Aura"],
    ]

    for row_idx, row_data in enumerate(cat_data, 2):
        for col_idx, value in enumerate(row_data, 1):
            cell = ws4.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border
            cell.alignment = Alignment(vertical='center', wrap_text=True)

    ws4.column_dimensions['A'].width = 18
    ws4.column_dimensions['B'].width = 10
    ws4.column_dimensions['C'].width = 50
    ws4.column_dimensions['D'].width = 45

    # =============================================
    # SHEET 5: Implementation Status Summary
    # =============================================
    ws5 = wb.create_sheet("Implementation Status")

    status_headers = ["Status", "Count", "Percentage", "Rules"]
    for col, header in enumerate(status_headers, 1):
        cell = ws5.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.fill = header_fill
        cell.border = thin_border
        cell.alignment = Alignment(horizontal='center', vertical='center', wrap_text=True)

    implemented = [r[0] for r in rules_data if "✅" in r[5]]
    partial = [r[0] for r in rules_data if "⚠️" in r[5]]
    not_impl = [r[0] for r in rules_data if "❌" in r[5]]
    total = len(rules_data)

    status_data = [
        ["✅ Fully Implemented", str(len(implemented)), f"{len(implemented)/total*100:.1f}%", ", ".join(implemented[:15]) + ("..." if len(implemented) > 15 else "")],
        ["⚠️ Partially Implemented", str(len(partial)), f"{len(partial)/total*100:.1f}%", ", ".join(partial)],
        ["❌ Not Implemented", str(len(not_impl)), f"{len(not_impl)/total*100:.1f}%", ", ".join(not_impl)],
        ["TOTAL", str(total), "100%", ""],
    ]

    for row_idx, row_data in enumerate(status_data, 2):
        for col_idx, value in enumerate(row_data, 1):
            cell = ws5.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border
            cell.alignment = Alignment(vertical='center', wrap_text=True)

            # Apply status coloring to first column
            if col_idx == 1:
                if "✅" in value:
                    cell.fill = implemented_fill
                elif "⚠️" in value:
                    cell.fill = partial_fill
                elif "❌" in value:
                    cell.fill = not_impl_fill

    ws5.column_dimensions['A'].width = 25
    ws5.column_dimensions['B'].width = 10
    ws5.column_dimensions['C'].width = 12
    ws5.column_dimensions['D'].width = 80

    # =============================================
    # SHEET 6: Trigger Timings Reference
    # =============================================
    ws6 = wb.create_sheet("Trigger Timings")

    timing_headers = ["Trigger Timing", "Description", "Example Rules"]
    for col, header in enumerate(timing_headers, 1):
        cell = ws6.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.fill = header_fill
        cell.border = thin_border
        cell.alignment = Alignment(horizontal='center', vertical='center', wrap_text=True)

    timing_data = [
        ["Always", "Rule is always active", "Fast, Flying, Tough, Regeneration, Agile"],
        ["OncePerGame", "Can only be used once per game", "Limited"],
        ["OncePerActivation", "Can be used once per unit activation", "VersatileAttack"],
        ["OnCharge", "Triggers when the unit charges", "Lance, Furious, Impact, PiercingAssault"],
        ["OnBeingCharged", "Triggers when being charged by enemy", "Counter"],
        ["StartOfRound", "Triggers at the start of each round", "Battleborn"],
        ["WhenShaken", "Triggers when unit becomes shaken", "NoRetreat"],
        ["WhenAttacking", "Triggers during attack resolution", "AP, Blast, Deadly, Precise, GoodShot, BadShot"],
        ["WhenDefending", "Triggers when being attacked", "ShieldWall, Shielded, MeleeEvasion, RangedShrouding"],
        ["OnModelKilled", "Triggers when a model is killed", "SelfDestruct"],
    ]

    for row_idx, row_data in enumerate(timing_data, 2):
        for col_idx, value in enumerate(row_data, 1):
            cell = ws6.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border
            cell.alignment = Alignment(vertical='center', wrap_text=True)

    ws6.column_dimensions['A'].width = 22
    ws6.column_dimensions['B'].width = 45
    ws6.column_dimensions['C'].width = 50

    # Save workbook
    output_path = "/home/user/Science-Battle-Simulator/chunk_sim_rules_review.xlsx"
    wb.save(output_path)
    print(f"Excel file created: {output_path}")
    return output_path

if __name__ == "__main__":
    create_rules_sheet()
