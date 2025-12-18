#!/usr/bin/env python3
"""Generate Excel spreadsheet for special rules engine configuration."""

from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter
from openpyxl.worksheet.datavalidation import DataValidation

def create_rules_matrix():
    wb = Workbook()
    ws = wb.active
    ws.title = "Special Rules"

    # Define headers
    headers = [
        "Rule ID",           # A - code identifier
        "Rule Name",         # B - display name
        "Description",       # C - what it does
        # Phase columns (Y if applies)
        "DEP",               # D - Deployment
        "MOV",               # E - Movement
        "CHG",               # F - Charge
        "SHT",               # G - Shooting
        "MEL",               # H - Melee
        "MOR",               # I - Morale
        "RND",               # J - Round start/end
        # Engine configuration
        "Trigger Step",      # K - CALC_ATTACKS, HIT_ROLL, CALC_AP, DEF_ROLL, CALC_WOUNDS, ALLOCATE, REGEN, MORALE
        "Trigger Condition", # L - always, is_charging, unmodified_6, vs_tough3, etc.
        "Effect Type",       # M - ADD, MULTIPLY, SET, REROLL, REPLACE, NEGATE, ROLL
        "Effect Target",     # N - attacks, hits, ap, defense, wounds, quality, morale
        "Effect Value",      # O - numeric or X (parameter) or formula
        "Effect Target Unit",# P - SELF, ENEMY, WEAPON
        "Priority",          # Q - order of operations (lower = earlier)
        "Notes",             # R - implementation notes
    ]

    # Define all rules with engine-ready data
    # Format: (rule_id, name, description, DEP, MOV, CHG, SHT, MEL, MOR, RND, trigger_step, trigger_condition, effect_type, effect_target, effect_value, target_unit, priority, notes)
    rules = [
        # WEAPON RULES - AP/Damage modifiers
        ("AP", "AP(X)", "Armor Piercing - adds X to defense target number", "", "", "", "Y", "Y", "", "", "CALC_AP", "always", "ADD", "defense_target", "X", "ENEMY", "10", "X is weapon parameter"),
        ("BLAST", "Blast(X)", "Each hit affects multiple models", "", "", "", "Y", "Y", "", "", "AFTER_HIT", "always", "MULTIPLY", "hits", "min(X, enemy_models)", "ENEMY", "20", "Capped by defender model count"),
        ("DEADLY", "Deadly(X)", "Multiplies final wounds dealt", "", "", "", "Y", "Y", "", "", "AFTER_WOUND", "always", "MULTIPLY", "wounds", "X", "ENEMY", "10", "Applied after wound calculation"),
        ("LANCE", "Lance", "+2 AP when charging", "", "", "Y", "", "Y", "", "", "CALC_AP", "is_charging", "ADD", "ap", "2", "WEAPON", "10", "Only in melee when charging"),
        ("POISON", "Poison", "Defender must reroll successful 6s on defense", "", "", "", "Y", "Y", "", "", "DEF_ROLL", "always", "REROLL", "defense_6s", "true", "ENEMY", "10", "Reroll 6s on defense"),
        ("PRECISE", "Precise", "+1 to hit rolls", "", "", "", "Y", "Y", "", "", "HIT_ROLL", "always", "ADD", "quality_mod", "1", "WEAPON", "10", "Flat bonus to quality test"),
        ("RELIABLE", "Reliable", "Quality test target becomes 2+", "", "", "", "Y", "Y", "", "", "HIT_ROLL", "always", "SET", "effective_quality", "2", "WEAPON", "5", "Overrides model quality"),
        ("RENDING", "Rending", "Unmodified 6s to hit gain AP(4)", "", "", "", "Y", "Y", "", "", "CALC_AP", "unmodified_6_hit", "SET", "ap", "4", "WEAPON", "20", "Per-hit basis, only on natural 6s"),
        ("BANE", "Bane", "Wounds bypass Regeneration", "", "", "", "Y", "Y", "", "", "REGEN", "always", "NEGATE", "regeneration", "true", "ENEMY", "10", "Blocks regen rolls entirely"),
        ("IMPACT", "Impact(X)", "Gain X extra attacks on charge", "", "", "Y", "", "Y", "", "", "CALC_ATTACKS", "is_charging", "ADD", "attacks", "X", "WEAPON", "10", "Extra attacks on charge only"),
        ("INDIRECT", "Indirect", "Ignore cover bonus to defense", "", "", "", "Y", "", "", "", "DEF_ROLL", "always", "NEGATE", "cover_bonus", "true", "ENEMY", "10", "Removes cover modifier"),
        ("SNIPER", "Sniper", "Choose which enemy model receives wounds", "", "", "", "Y", "", "", "", "ALLOCATE", "always", "REPLACE", "allocation_order", "chosen_model", "ENEMY", "10", "Bypasses normal allocation"),
        ("LOCK_ON", "Lock-On", "+1 to hit against Vehicle targets", "", "", "", "Y", "Y", "", "", "HIT_ROLL", "target_is_vehicle", "ADD", "quality_mod", "1", "SELF", "10", "Requires Vehicle keyword"),
        ("PURGE", "Purge", "+1 to hit against Tough(3+) targets", "", "", "", "Y", "Y", "", "", "HIT_ROLL", "target_has_tough_3plus", "ADD", "quality_mod", "1", "SELF", "10", "Check defender Tough value"),
        ("LIMITED", "Limited", "Can only be used once per battle", "", "", "", "Y", "Y", "", "", "CALC_ATTACKS", "not_used_this_game", "SET", "weapon_available", "false_after_use", "WEAPON", "1", "Track usage per game"),
        ("LINKED", "Linked", "Only usable when firing paired weapon", "", "", "", "Y", "Y", "", "", "CALC_ATTACKS", "paired_weapon_fired", "SET", "weapon_available", "true", "WEAPON", "1", "Requires paired weapon"),

        # ATTACK MODIFIER RULES
        ("FURIOUS", "Furious", "Unmodified 6s to hit count as extra hits when charging", "", "", "Y", "", "Y", "", "", "AFTER_HIT", "is_charging AND unmodified_6_hit", "ADD", "hits", "sixes_rolled", "SELF", "15", "Only on charge, uses tracked 6s"),
        ("PREDATOR_FIGHTER", "Predator Fighter", "Each 6 to hit in melee generates another attack roll", "", "", "", "", "Y", "", "", "HIT_ROLL", "unmodified_6_hit", "ROLL", "extra_attacks", "recursive", "SELF", "20", "Recursive - new 6s generate more"),
        ("RELENTLESS", "Relentless", "Unmodified 6s to hit become extra hits at >9\" range", "", "", "", "Y", "", "", "", "AFTER_HIT", "range_over_9", "ADD", "hits", "sixes_rolled", "SELF", "15", "Shooting only, range check"),
        ("GOOD_SHOT", "Good Shot", "+1 to hit when shooting", "", "", "", "Y", "", "", "", "HIT_ROLL", "phase_is_shooting", "ADD", "quality_mod", "1", "SELF", "10", "Shooting phase only"),
        ("BAD_SHOT", "Bad Shot", "-1 to hit when shooting", "", "", "", "Y", "", "", "", "HIT_ROLL", "phase_is_shooting", "ADD", "quality_mod", "-1", "SELF", "10", "Shooting phase only"),
        ("SURGE", "Surge", "Each 6 to hit generates +1 hit", "", "", "", "Y", "Y", "", "", "AFTER_HIT", "unmodified_6_hit", "ADD", "hits", "sixes_rolled", "SELF", "15", "Not charging-dependent"),
        ("THRUST", "Thrust", "+1 to hit and +1 AP when charging", "", "", "Y", "", "Y", "", "", "HIT_ROLL,CALC_AP", "is_charging", "ADD", "quality_mod,ap", "1,1", "SELF", "10", "Dual effect on charge"),
        ("VERSATILE_ATTACK", "Versatile Attack", "Before attacking, choose +1 AP or +1 to hit", "", "", "", "Y", "Y", "", "", "PRE_ATTACK", "always", "CHOOSE", "ap OR quality_mod", "1", "SELF", "5", "Player choice, AI optimizes"),
        ("POINT_BLANK_SURGE", "Point Blank Surge", "6s to hit at 0-9\" range generate extra hits", "", "", "", "Y", "", "", "", "AFTER_HIT", "range_0_to_9 AND unmodified_6_hit", "ADD", "hits", "sixes_rolled", "SELF", "15", "Short range only"),

        # DEFENSE MODIFIER RULES
        ("TOUGH", "Tough(X)", "Model has X wounds before being removed", "", "", "", "Y", "Y", "", "", "ALLOCATE", "always", "SET", "wounds_to_kill", "X", "SELF", "1", "Model property, not per-attack"),
        ("REGENERATION", "Regeneration", "5+ roll to ignore each wound received", "", "", "", "Y", "Y", "", "", "REGEN", "always", "ROLL", "ignore_wound", "5+", "SELF", "10", "Roll per wound, can be negated"),
        ("SHIELDED", "Shielded", "+1 Defense against non-spell attacks", "", "", "", "Y", "Y", "", "", "DEF_ROLL", "attack_not_spell", "ADD", "defense_mod", "1", "SELF", "10", "Check weapon/attack type"),
        ("SHIELD_WALL", "Shield Wall", "+1 Defense in melee combat", "", "", "", "", "Y", "", "", "DEF_ROLL", "phase_is_melee", "ADD", "defense_mod", "1", "SELF", "10", "Melee phase only"),
        ("STEALTH", "Stealth", "-1 to be hit from more than 12\" away", "", "", "", "Y", "", "", "", "HIT_ROLL", "range_over_12", "ADD", "enemy_quality_mod", "-1", "SELF", "10", "Affects attacker's roll"),
        ("MELEE_EVASION", "Melee Evasion", "+1 Defense in melee combat", "", "", "", "", "Y", "", "", "DEF_ROLL", "phase_is_melee", "ADD", "defense_mod", "1", "SELF", "10", "Stacks with Shield Wall"),
        ("MELEE_SHROUDING", "Melee Shrouding", "+1 Defense in melee combat", "", "", "", "", "Y", "", "", "DEF_ROLL", "phase_is_melee", "ADD", "defense_mod", "1", "SELF", "10", "Equivalent to Melee Evasion"),
        ("RANGED_SHROUDING", "Ranged Shrouding", "+1 Defense against shooting attacks", "", "", "", "Y", "", "", "", "DEF_ROLL", "phase_is_shooting", "ADD", "defense_mod", "1", "SELF", "10", "Shooting phase only"),
        ("RESISTANCE", "Resistance", "6+ to ignore wounds (2+ vs spells)", "", "", "", "Y", "Y", "", "", "REGEN", "always", "ROLL", "ignore_wound", "6+ OR 2+_vs_spell", "SELF", "20", "After regen, separate roll"),
        ("PROTECTED", "Protected", "6+ to reduce incoming AP by 1", "", "", "", "Y", "Y", "", "", "CALC_AP", "always", "ROLL", "reduce_ap", "6+ for -1", "SELF", "5", "Roll before defense"),

        # WOUND MODIFIER RULES
        ("RUPTURE", "Rupture", "+1 wound per 6 to hit, bypasses regeneration", "", "", "", "Y", "Y", "", "", "AFTER_WOUND,REGEN", "unmodified_6_hit", "ADD,NEGATE", "wounds,regeneration", "sixes_rolled,true", "ENEMY", "15", "Dual effect"),
        ("SHRED", "Shred", "+1 wound per 1 rolled on defense", "", "", "", "Y", "Y", "", "", "AFTER_DEF", "defense_rolled_1", "ADD", "wounds", "ones_rolled", "ENEMY", "15", "Triggered by defender failure"),

        # WOUND ALLOCATION RULES
        ("HERO", "Hero", "This model receives wounds last in its unit", "", "", "", "Y", "Y", "", "", "ALLOCATE", "always", "SET", "allocation_priority", "last", "SELF", "1", "Allocation order modifier"),
        ("TAKEDOWN", "Takedown", "Choose a specific model to receive all wounds", "", "", "", "Y", "Y", "", "", "ALLOCATE", "always", "REPLACE", "allocation_target", "chosen_single", "ENEMY", "1", "Bypasses normal order"),

        # MORALE RULES
        ("FEARLESS", "Fearless", "Reroll failed morale tests", "", "", "", "", "", "Y", "", "MORALE", "morale_failed", "REROLL", "morale_test", "true", "SELF", "10", "Single reroll"),
        ("MORALE_BOOST", "Morale Boost", "+1 to morale test rolls", "", "", "", "", "", "Y", "", "MORALE", "always", "ADD", "morale_mod", "1", "SELF", "10", "Flat bonus"),
        ("NO_RETREAT", "No Retreat", "Take wounds instead of becoming Shaken", "", "", "", "", "", "Y", "", "MORALE", "would_become_shaken", "REPLACE", "shaken_result", "roll_for_wounds", "SELF", "10", "Roll dice = morale margin, 1-3 = wound"),
        ("BATTLEBORN", "Battleborn", "4+ to recover from Shaken at round start", "", "", "", "", "", "", "Y", "ROUND_START", "is_shaken", "ROLL", "rally", "4+", "SELF", "10", "Before other actions"),
        ("FEAR", "Fear(X)", "Count as +X wounds when calculating morale", "", "", "", "", "Y", "Y", "", "MORALE", "enemy_checking_morale", "ADD", "effective_wounds", "X", "SELF", "10", "Affects enemy morale calc"),

        # COMBAT ORDER RULES
        ("COUNTER", "Counter", "Strike first when receiving a charge", "", "", "Y", "", "Y", "", "", "COMBAT_ORDER", "is_charged", "SET", "strike_order", "first", "SELF", "1", "Swaps initiative"),
        ("HIT_AND_RUN", "Hit and Run", "May retreat after melee combat", "", "", "", "", "Y", "", "Y", "ROUND_END", "after_melee", "ENABLE", "retreat_option", "true", "SELF", "10", "Optional action"),
        ("SELF_DESTRUCT", "Self Destruct(X)", "Deal X hits to attacker when this model dies", "", "", "", "", "Y", "", "", "ON_DEATH", "model_killed", "TRIGGER", "hits_to_attacker", "X", "ENEMY", "10", "Resolve as separate attack"),

        # MOVEMENT/DEPLOYMENT RULES
        ("SCOUT", "Scout", "Deploy up to 12\" ahead of deployment zone", "Y", "", "", "", "", "", "", "DEPLOYMENT", "always", "ADD", "deploy_distance", "12", "SELF", "10", "Before game starts"),
        ("AMBUSH", "Ambush", "Deploy from reserve, >9\" from enemies", "Y", "", "", "", "", "", "", "DEPLOYMENT", "always", "SET", "deploy_type", "reserve_9", "SELF", "10", "Not on table turn 1"),
        ("FAST", "Fast", "9\" move instead of standard 6\"", "", "Y", "Y", "", "", "", "", "MOVEMENT", "always", "SET", "move_distance", "9", "SELF", "1", "Base movement override"),
        ("SLOW", "Slow", "4\" move instead of standard 6\"", "", "Y", "Y", "", "", "", "", "MOVEMENT", "always", "SET", "move_distance", "4", "SELF", "1", "Base movement override"),
        ("AGILE", "Agile", "+1\" advance, +2\" rush and charge", "", "Y", "Y", "", "", "", "", "MOVEMENT", "always", "ADD", "advance,rush,charge", "1,2,2", "SELF", "10", "Multiple movement bonuses"),
        ("FLYING", "Flying", "Ignore terrain and models when moving", "", "Y", "Y", "", "", "", "", "MOVEMENT", "always", "NEGATE", "terrain_penalty,model_blocking", "true,true", "SELF", "10", "Path ignores obstacles"),
        ("STRIDER", "Strider", "Ignore difficult terrain penalties", "", "Y", "Y", "", "", "", "", "MOVEMENT", "always", "NEGATE", "difficult_terrain", "true", "SELF", "10", "Terrain only"),
        ("RAPID_CHARGE", "Rapid Charge", "+4\" to charge distance", "", "", "Y", "", "", "", "", "MOVEMENT", "is_charging", "ADD", "charge_distance", "4", "SELF", "10", "Charge move only"),

        # MAGIC RULES
        ("CASTING", "Casting(X)", "Can cast X spells per round", "", "", "", "", "", "", "Y", "SPELL_PHASE", "always", "SET", "spell_slots", "X", "SELF", "1", "Magic phase resource"),
        ("DEVOUT", "Devout", "Faction-specific bonus (varies)", "", "", "", "", "", "", "", "VARIES", "faction_specific", "VARIES", "varies", "varies", "SELF", "10", "See faction rules"),
    ]

    # Styles
    header_font = Font(bold=True, color="FFFFFF")
    header_fill = PatternFill(start_color="4472C4", end_color="4472C4", fill_type="solid")
    phase_header_fill = PatternFill(start_color="70AD47", end_color="70AD47", fill_type="solid")
    engine_header_fill = PatternFill(start_color="ED7D31", end_color="ED7D31", fill_type="solid")

    y_fill = PatternFill(start_color="C6EFCE", end_color="C6EFCE", fill_type="solid")

    thin_border = Border(
        left=Side(style='thin'),
        right=Side(style='thin'),
        top=Side(style='thin'),
        bottom=Side(style='thin')
    )

    center_align = Alignment(horizontal='center', vertical='center')
    wrap_align = Alignment(horizontal='left', vertical='center', wrap_text=True)

    # Write headers with color coding
    for col, header in enumerate(headers, 1):
        cell = ws.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.alignment = center_align
        cell.border = thin_border

        # Color code header groups
        if col <= 3:  # Rule info
            cell.fill = header_fill
        elif col <= 10:  # Phase columns
            cell.fill = phase_header_fill
        else:  # Engine config
            cell.fill = engine_header_fill

    # Write data
    for row_idx, rule in enumerate(rules, 2):
        for col_idx, value in enumerate(rule, 1):
            cell = ws.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border

            # Phase columns (Y markers)
            if 4 <= col_idx <= 10:
                cell.alignment = center_align
                if value == "Y":
                    cell.fill = y_fill
                    cell.font = Font(bold=True)
            # Description and Notes - wrap text
            elif col_idx in [3, 18]:
                cell.alignment = wrap_align
            # Centered columns
            elif col_idx in [1, 2, 13, 16, 17]:
                cell.alignment = center_align
            else:
                cell.alignment = Alignment(horizontal='left', vertical='center')

    # Set column widths
    column_widths = {
        'A': 18,   # Rule ID
        'B': 20,   # Rule Name
        'C': 50,   # Description
        'D': 5, 'E': 5, 'F': 5, 'G': 5, 'H': 5, 'I': 5, 'J': 5,  # Phases
        'K': 20,   # Trigger Step
        'L': 35,   # Trigger Condition
        'M': 12,   # Effect Type
        'N': 25,   # Effect Target
        'O': 25,   # Effect Value
        'P': 12,   # Target Unit
        'Q': 8,    # Priority
        'R': 40,   # Notes
    }

    for col_letter, width in column_widths.items():
        ws.column_dimensions[col_letter].width = width

    # Set row height for better readability
    for row in range(2, len(rules) + 2):
        ws.row_dimensions[row].height = 25

    # Freeze first row and first 3 columns
    ws.freeze_panes = 'D2'

    # Add data validation for Effect Type
    effect_types = '"ADD,MULTIPLY,SET,REROLL,REPLACE,NEGATE,ROLL,CHOOSE,TRIGGER,ENABLE"'
    dv_effect = DataValidation(type="list", formula1=effect_types, allow_blank=True)
    dv_effect.error = "Please select from the list"
    dv_effect.errorTitle = "Invalid Effect Type"
    ws.add_data_validation(dv_effect)
    dv_effect.add(f'M2:M{len(rules)+1}')

    # Add data validation for Target Unit
    target_units = '"SELF,ENEMY,WEAPON"'
    dv_target = DataValidation(type="list", formula1=target_units, allow_blank=True)
    dv_target.error = "Please select from the list"
    dv_target.errorTitle = "Invalid Target Unit"
    ws.add_data_validation(dv_target)
    dv_target.add(f'P2:P{len(rules)+1}')

    # ================== LEGEND SHEET ==================
    ws_legend = wb.create_sheet("Legend")

    legend_data = [
        ("COLUMN REFERENCE", "", ""),
        ("", "", ""),
        ("Column", "Values", "Description"),
        ("", "", ""),
        ("Rule ID", "(text)", "Unique identifier used in code"),
        ("Rule Name", "(text)", "Display name with parameter notation"),
        ("Description", "(text)", "Human-readable explanation of the rule"),
        ("", "", ""),
        ("PHASE COLUMNS", "", "Mark Y if rule applies in this phase"),
        ("DEP", "Y or blank", "Deployment - before battle begins"),
        ("MOV", "Y or blank", "Movement phase"),
        ("CHG", "Y or blank", "Charge declaration and movement"),
        ("SHT", "Y or blank", "Shooting phase"),
        ("MEL", "Y or blank", "Melee combat phase"),
        ("MOR", "Y or blank", "Morale checks"),
        ("RND", "Y or blank", "Round start/end effects"),
        ("", "", ""),
        ("ENGINE CONFIGURATION", "", ""),
        ("Trigger Step", "(see list)", "When in combat resolution the rule activates"),
        ("", "CALC_ATTACKS", "When determining number of attacks"),
        ("", "HIT_ROLL", "During the quality/hit test"),
        ("", "AFTER_HIT", "After hits determined, before defense"),
        ("", "CALC_AP", "When calculating armor penetration"),
        ("", "DEF_ROLL", "During the defense/save test"),
        ("", "AFTER_DEF", "After defense roll resolved"),
        ("", "AFTER_WOUND", "After wounds calculated"),
        ("", "ALLOCATE", "When assigning wounds to models"),
        ("", "REGEN", "During regeneration rolls"),
        ("", "MORALE", "During morale tests"),
        ("", "ROUND_START", "At beginning of round"),
        ("", "ROUND_END", "At end of round"),
        ("", "ON_DEATH", "When a model is removed"),
        ("", "PRE_ATTACK", "Before attack sequence begins"),
        ("", "COMBAT_ORDER", "When determining strike order"),
        ("", "", ""),
        ("Trigger Condition", "(expression)", "Boolean condition that must be true"),
        ("", "always", "Always triggers"),
        ("", "is_charging", "Unit is charging this round"),
        ("", "is_charged", "Unit was charged this round"),
        ("", "unmodified_6_hit", "Natural 6 rolled on hit"),
        ("", "phase_is_shooting", "Currently in shooting phase"),
        ("", "phase_is_melee", "Currently in melee phase"),
        ("", "range_over_9", "Target is more than 9\" away"),
        ("", "range_over_12", "Target is more than 12\" away"),
        ("", "range_0_to_9", "Target is within 9\""),
        ("", "attack_not_spell", "Attack is not a spell"),
        ("", "target_is_vehicle", "Target has Vehicle keyword"),
        ("", "target_has_tough_3plus", "Target has Tough(3) or higher"),
        ("", "", ""),
        ("Effect Type", "(from list)", "How the rule modifies values"),
        ("", "ADD", "Add value to target (can be negative)"),
        ("", "MULTIPLY", "Multiply target by value"),
        ("", "SET", "Replace target with value"),
        ("", "REROLL", "Allow reroll of specified dice"),
        ("", "REPLACE", "Replace behavior entirely"),
        ("", "NEGATE", "Cancel/ignore something"),
        ("", "ROLL", "Require a dice roll for effect"),
        ("", "CHOOSE", "Player/AI chooses between options"),
        ("", "TRIGGER", "Cause a secondary effect"),
        ("", "ENABLE", "Unlock an optional action"),
        ("", "", ""),
        ("Effect Target", "(stat name)", "What is being modified"),
        ("", "attacks", "Number of attack dice"),
        ("", "hits", "Number of successful hits"),
        ("", "quality_mod", "Modifier to quality roll"),
        ("", "ap", "Armor penetration value"),
        ("", "defense_target", "Defense roll target number"),
        ("", "defense_mod", "Modifier to defense roll"),
        ("", "wounds", "Number of wounds dealt"),
        ("", "regeneration", "Regeneration ability"),
        ("", "morale_mod", "Modifier to morale test"),
        ("", "allocation_priority", "Wound allocation order"),
        ("", "", ""),
        ("Effect Value", "(varies)", "The modification amount"),
        ("", "X", "Use weapon/rule parameter"),
        ("", "sixes_rolled", "Number of 6s from hit roll"),
        ("", "ones_rolled", "Number of 1s from defense roll"),
        ("", "(number)", "Fixed numeric value"),
        ("", "(formula)", "Calculated expression"),
        ("", "", ""),
        ("Target Unit", "(from list)", "Who is affected"),
        ("", "SELF", "The unit with this rule"),
        ("", "ENEMY", "The opposing unit"),
        ("", "WEAPON", "The weapon being used"),
        ("", "", ""),
        ("Priority", "(number)", "Lower numbers process first"),
    ]

    for row_idx, (col1, col2, col3) in enumerate(legend_data, 1):
        ws_legend.cell(row=row_idx, column=1, value=col1)
        ws_legend.cell(row=row_idx, column=2, value=col2)
        ws_legend.cell(row=row_idx, column=3, value=col3)

        # Bold section headers
        if col1 in ["COLUMN REFERENCE", "PHASE COLUMNS", "ENGINE CONFIGURATION"] or (col1 == "Column"):
            ws_legend.cell(row=row_idx, column=1).font = Font(bold=True)
            if col1 == "Column":
                ws_legend.cell(row=row_idx, column=2).font = Font(bold=True)
                ws_legend.cell(row=row_idx, column=3).font = Font(bold=True)
        # Bold field names in col1
        elif col1 and col2:
            ws_legend.cell(row=row_idx, column=1).font = Font(bold=True)

    ws_legend.column_dimensions['A'].width = 20
    ws_legend.column_dimensions['B'].width = 25
    ws_legend.column_dimensions['C'].width = 50

    # Save
    output_path = "/home/user/Science-Battle-Simulator/docs/special_rules_review_matrix.xlsx"
    wb.save(output_path)
    print(f"Created: {output_path}")
    return output_path

if __name__ == "__main__":
    create_rules_matrix()
