#!/usr/bin/env python3
"""Generate Excel spreadsheet for special rules review matrix."""

from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter

def create_rules_matrix():
    wb = Workbook()
    ws = wb.active
    ws.title = "Special Rules Matrix"

    # Define headers
    headers = [
        "Category", "Rule", "Description",
        # Phase columns
        "DEP", "MOV", "CHG", "SHT", "MEL", "MOR", "RND",
        # Sub-step columns
        "ATK", "HIT", "DEF", "WND", "ALC", "RGN",
        # Effect columns
        "Grants", "Condition", "Value", "Target", "IMP"
    ]

    # Define all rules with their data
    rules = [
        # WEAPON RULES
        ("Weapon", "AP(X)", "Armor Piercing - reduces enemy defense", "", "", "", "Y", "Y", "", "", "", "", "Y", "", "", "", "Defense penalty to target", "Always on weapon attack", "X", "Enemy", "YES"),
        ("Weapon", "Blast(X)", "Hits multiple models", "", "", "", "Y", "Y", "", "", "", "", "", "Y", "", "", "Multiply hits by min(X, enemy models)", "After hit roll", "X", "Enemy", "YES"),
        ("Weapon", "Deadly(X)", "Multiplies wounds dealt", "", "", "", "Y", "Y", "", "", "", "", "", "Y", "", "", "Multiply wounds by X", "After wound calculation", "X", "Enemy", "YES"),
        ("Weapon", "Lance", "Extra AP when charging", "", "", "Y", "", "Y", "", "", "", "", "Y", "", "", "", "+2 AP", "When charging", "+2", "Weapon", "YES"),
        ("Weapon", "Poison", "Enemy rerolls successful saves", "", "", "", "Y", "Y", "", "", "", "", "Y", "", "", "", "Reroll defense 6s", "On defense roll", "-", "Enemy", "YES"),
        ("Weapon", "Precise", "Bonus to hit", "", "", "", "Y", "Y", "", "", "", "Y", "", "", "", "", "+1 to hit roll", "Always", "+1", "Weapon", "YES"),
        ("Weapon", "Reliable", "Improved quality", "", "", "", "Y", "Y", "", "", "", "Y", "", "", "", "", "Quality becomes 2+", "Always", "2+", "Weapon", "YES"),
        ("Weapon", "Rending", "Critical hits ignore armor", "", "", "", "Y", "Y", "", "", "", "Y", "Y", "", "", "", "AP(4) on unmodified 6", "On unmodified 6 to hit", "AP4", "Enemy", "PARTIAL"),
        ("Weapon", "Bane", "Bypasses regeneration", "", "", "", "Y", "Y", "", "", "", "", "", "", "", "Y", "Target cannot use Regeneration", "On wound dealt", "-", "Enemy", "YES"),
        ("Weapon", "Impact(X)", "Bonus attacks on charge", "", "", "Y", "", "Y", "", "", "", "Y", "", "", "", "", "X extra attacks", "On charge only", "X", "Self", "PARTIAL"),
        ("Weapon", "Indirect", "Ignores cover", "", "", "", "Y", "", "", "", "", "", "Y", "", "", "", "Ignore cover bonus", "Always", "-1 cover", "Enemy", "PARTIAL"),
        ("Weapon", "Sniper", "Pick target model", "", "", "", "Y", "", "", "", "Y", "", "", "", "", "", "Choose which model takes wounds", "When declaring attack", "-", "Enemy", "NO"),
        ("Weapon", "Lock-On", "Bonus vs vehicles", "", "", "", "Y", "Y", "", "", "", "Y", "", "", "", "", "+1 to hit vs Vehicle keyword", "vs Vehicles", "+1", "Self", "NO"),
        ("Weapon", "Purge", "Bonus vs tough targets", "", "", "", "Y", "Y", "", "", "", "Y", "", "", "", "", "+1 to hit vs Tough(3+)", "vs Tough(3+)", "+1", "Self", "NO"),
        ("Weapon", "Limited", "Single use", "", "", "", "Y", "Y", "", "", "Y", "", "", "", "", "", "Can only be used once per game", "First use only", "1", "Weapon", "NO"),
        ("Weapon", "Linked", "Paired weapon requirement", "", "", "", "Y", "Y", "", "", "Y", "", "", "", "", "", "Only usable with paired weapon", "With paired weapon", "-", "Weapon", "NO"),

        # ATTACK MODIFIER RULES
        ("Attack Mod", "Furious", "Extra hits when charging", "", "", "Y", "", "Y", "", "", "", "", "", "Y", "", "", "Unmodified 6s to hit become extra hits", "When charging", "+6s", "Self", "YES"),
        ("Attack Mod", "PredatorFighter", "Recursive attacks on 6s", "", "", "", "", "Y", "", "", "", "Y", "", "", "", "", "Each 6 to hit generates another attack", "In melee on 6s", "Recursive", "Self", "YES"),
        ("Attack Mod", "Relentless", "Extra hits at range", "", "", "", "Y", "", "", "", "", "Y", "", "", "", "", "6s to hit become extra hits", "Shooting from >9\"", "+6s", "Self", "PARTIAL"),
        ("Attack Mod", "GoodShot", "Improved shooting", "", "", "", "Y", "", "", "", "", "Y", "", "", "", "", "+1 to hit when shooting", "Shooting only", "+1", "Self", "YES"),
        ("Attack Mod", "BadShot", "Reduced shooting", "", "", "", "Y", "", "", "", "", "Y", "", "", "", "", "-1 to hit when shooting", "Shooting only", "-1", "Self", "YES"),
        ("Attack Mod", "Surge", "Extra hits on 6s", "", "", "", "Y", "Y", "", "", "", "Y", "", "", "", "", "Each 6 to hit = +1 hit", "On 6 to hit", "+1", "Self", "NO"),
        ("Attack Mod", "Thrust", "Charge bonus", "", "", "Y", "", "Y", "", "", "", "Y", "Y", "", "", "", "+1 to hit and +1 AP", "When charging", "+1/+1", "Self", "NO"),
        ("Attack Mod", "VersatileAttack", "Flexible combat style", "", "", "", "Y", "Y", "", "", "Y", "Y", "Y", "", "", "", "Choose: AP+1 OR +1 to hit", "Before attack", "+1", "Self", "YES"),
        ("Attack Mod", "PointBlankSurge", "Close range bonus", "", "", "", "Y", "", "", "", "", "Y", "", "", "", "", "6s at 0-9\" = extra hit", "Shooting 0-9\"", "+1", "Self", "NO"),

        # DEFENSE MODIFIER RULES
        ("Defense Mod", "Tough(X)", "Multiple wounds", "", "", "", "Y", "Y", "", "", "", "", "", "", "Y", "", "Model has X wounds before dying", "Model property", "X", "Self", "YES"),
        ("Defense Mod", "Regeneration", "Heal wounds", "", "", "", "Y", "Y", "", "", "", "", "", "", "", "Y", "5+ to ignore each wound", "On each wound received", "5+", "Self", "YES"),
        ("Defense Mod", "Shielded", "Bonus defense", "", "", "", "Y", "Y", "", "", "", "", "Y", "", "", "", "+1 Defense vs non-spell attacks", "vs non-spell", "+1", "Self", "YES"),
        ("Defense Mod", "ShieldWall", "Melee defense bonus", "", "", "", "", "Y", "", "", "", "", "Y", "", "", "", "+1 Defense in melee", "In melee only", "+1", "Self", "YES"),
        ("Defense Mod", "Stealth", "Hard to hit at range", "", "", "", "Y", "", "", "", "", "", "Y", "", "", "", "-1 to be hit from far", "From >12\" away", "-1 enemy", "Self", "NO"),
        ("Defense Mod", "MeleeEvasion", "Dodge in melee", "", "", "", "", "Y", "", "", "", "", "Y", "", "", "", "+1 Defense in melee", "In melee only", "+1", "Self", "YES"),
        ("Defense Mod", "MeleeShrouding", "Protected in melee", "", "", "", "", "Y", "", "", "", "", "Y", "", "", "", "+1 Defense in melee", "In melee only", "+1", "Self", "YES"),
        ("Defense Mod", "RangedShrouding", "Protected from shooting", "", "", "", "Y", "", "", "", "", "", "Y", "", "", "", "+1 Defense when shot", "When shot", "+1", "Self", "YES"),
        ("Defense Mod", "Resistance", "Ignore wounds", "", "", "", "Y", "Y", "", "", "", "", "", "", "", "Y", "6+ ignore wound (2+ vs spell)", "After wound", "6+/2+", "Self", "YES"),
        ("Defense Mod", "Protected", "Reduce AP", "", "", "", "Y", "Y", "", "", "", "", "Y", "", "", "", "6+ to reduce incoming AP by 1", "Before defense roll", "6+", "Self", "NO"),

        # WOUND MODIFIER RULES
        ("Wound Mod", "Rupture", "Extra wounds and bypass regen", "", "", "", "Y", "Y", "", "", "", "", "", "Y", "", "Y", "+1 wound per 6 to hit, ignore regen", "On 6s to hit", "+1", "Enemy", "YES"),
        ("Wound Mod", "Shred", "Extra wounds on failed saves", "", "", "", "Y", "Y", "", "", "", "", "", "Y", "", "", "+1 wound per 1 on defense roll", "On 1s to defend", "+1", "Enemy", "YES"),

        # WOUND ALLOCATION RULES
        ("Allocation", "Hero", "Protected character", "", "", "", "Y", "Y", "", "", "", "", "", "", "Y", "", "Takes wounds last in unit", "Allocation order", "Last", "Self", "YES"),
        ("Allocation", "Takedown", "Target specific model", "", "", "", "Y", "Y", "", "", "Y", "", "", "", "Y", "", "Target single model, resolve as unit of 1", "Declaration", "1 model", "Enemy", "NO"),

        # REGENERATION BYPASS RULES
        ("Regen Bypass", "Bane", "Prevent healing", "", "", "", "Y", "Y", "", "", "", "", "", "", "", "Y", "Wounds bypass Regeneration", "On wounds dealt", "-", "Enemy", "YES"),
        ("Regen Bypass", "Rupture", "Prevent healing", "", "", "", "Y", "Y", "", "", "", "", "", "Y", "", "Y", "Wounds bypass Regeneration", "On wounds dealt", "-", "Enemy", "YES"),
        ("Regen Bypass", "Unstoppable", "Ignore enemy defenses", "", "", "", "Y", "Y", "", "", "", "", "", "", "", "Y", "Ignores enemy regeneration", "On wounds dealt", "-", "Enemy", "PARTIAL"),

        # MORALE RULES
        ("Morale", "Fearless", "Brave unit", "", "", "", "", "", "Y", "", "", "", "", "", "", "", "Reroll failed morale tests", "On morale failure", "Reroll", "Self", "YES"),
        ("Morale", "MoraleBoost", "Leadership bonus", "", "", "", "", "", "Y", "", "", "", "", "", "", "", "+1 to morale tests", "Always", "+1", "Self", "YES"),
        ("Morale", "NoRetreat", "Stand and die", "", "", "", "", "", "Y", "", "", "", "", "", "", "", "Take wounds instead of becoming Shaken", "On morale failure", "wounds", "Self", "YES"),
        ("Morale", "Battleborn", "Quick recovery", "", "", "", "", "", "", "Y", "", "", "", "", "", "", "4+ to rally from Shaken at round start", "Round start", "4+", "Self", "YES"),
        ("Morale", "Fear(X)", "Terrifying presence", "", "", "", "", "Y", "Y", "", "", "", "", "", "", "", "Count as +X wounds for morale", "In melee morale", "+X", "Self", "NO"),

        # COMBAT ORDER RULES
        ("Combat Order", "Counter", "Reactive strike", "", "", "", "", "Y", "", "", "Y", "", "", "", "", "", "Strike first when receiving a charge", "When charged", "First", "Self", "NO"),
        ("Combat Order", "HitAndRun", "Tactical retreat", "", "", "", "", "Y", "", "Y", "", "", "", "", "", "", "Can retreat after fighting", "End of melee", "-", "Self", "NO"),
        ("Combat Order", "SelfDestruct", "Explosive death", "", "", "", "", "Y", "", "", "", "", "", "", "Y", "", "Deal X hits to attacker when killed", "On model death", "X", "Enemy", "NO"),

        # MOVEMENT/DEPLOYMENT RULES
        ("Movement", "Scout", "Forward deployment", "Y", "", "", "", "", "", "", "", "", "", "", "", "", "Deploy 12\" ahead of deployment zone", "Deployment", "+12\"", "Self", "NO"),
        ("Movement", "Ambush", "Reserve deployment", "Y", "", "", "", "", "", "", "", "", "", "", "", "", "Deploy from reserve >9\" from enemy", "Deployment", "Reserve", "Self", "NO"),
        ("Movement", "Fast", "Increased speed", "", "Y", "Y", "", "", "", "", "", "", "", "", "", "", "9\" move instead of 6\"", "Movement", "9\"", "Self", "NO"),
        ("Movement", "Slow", "Reduced speed", "", "Y", "Y", "", "", "", "", "", "", "", "", "", "", "4\" move instead of 6\"", "Movement", "4\"", "Self", "NO"),
        ("Movement", "Agile", "Enhanced mobility", "", "Y", "Y", "", "", "", "", "", "", "", "", "", "", "+1\" advance, +2\" rush/charge", "Movement", "+1/+2", "Self", "NO"),
        ("Movement", "Flying", "Airborne", "", "Y", "Y", "", "", "", "", "", "", "", "", "", "", "Ignore terrain and units when moving", "Movement", "-", "Self", "NO"),
        ("Movement", "Strider", "Terrain specialist", "", "Y", "Y", "", "", "", "", "", "", "", "", "", "", "Ignore difficult terrain", "Movement", "-", "Self", "NO"),
        ("Movement", "RapidCharge", "Swift assault", "", "", "Y", "", "", "", "", "", "", "", "", "", "", "+4\" to charge range", "Charge only", "+4\"", "Self", "NO"),

        # MAGIC/SPECIAL RULES
        ("Magic", "Casting(X)", "Spellcaster", "", "", "", "", "", "", "Y", "", "", "", "", "", "", "Cast X spells per round", "Casting phase", "X", "Self", "NO"),
        ("Magic", "Devout", "Faction rule", "", "", "", "", "", "", "", "", "", "", "", "", "", "Faction-specific bonus", "Varies", "-", "Self", "NO"),
    ]

    # Styles
    header_font = Font(bold=True, color="FFFFFF")
    header_fill = PatternFill(start_color="4472C4", end_color="4472C4", fill_type="solid")

    category_fills = {
        "Weapon": PatternFill(start_color="E2EFDA", end_color="E2EFDA", fill_type="solid"),
        "Attack Mod": PatternFill(start_color="FCE4D6", end_color="FCE4D6", fill_type="solid"),
        "Defense Mod": PatternFill(start_color="DDEBF7", end_color="DDEBF7", fill_type="solid"),
        "Wound Mod": PatternFill(start_color="FFF2CC", end_color="FFF2CC", fill_type="solid"),
        "Allocation": PatternFill(start_color="E4DFEC", end_color="E4DFEC", fill_type="solid"),
        "Regen Bypass": PatternFill(start_color="F8CBAD", end_color="F8CBAD", fill_type="solid"),
        "Morale": PatternFill(start_color="D9E1F2", end_color="D9E1F2", fill_type="solid"),
        "Combat Order": PatternFill(start_color="C6EFCE", end_color="C6EFCE", fill_type="solid"),
        "Movement": PatternFill(start_color="FFEB9C", end_color="FFEB9C", fill_type="solid"),
        "Magic": PatternFill(start_color="E6B8B7", end_color="E6B8B7", fill_type="solid"),
    }

    yes_fill = PatternFill(start_color="C6EFCE", end_color="C6EFCE", fill_type="solid")
    partial_fill = PatternFill(start_color="FFEB9C", end_color="FFEB9C", fill_type="solid")
    no_fill = PatternFill(start_color="FFC7CE", end_color="FFC7CE", fill_type="solid")

    y_fill = PatternFill(start_color="92D050", end_color="92D050", fill_type="solid")

    thin_border = Border(
        left=Side(style='thin'),
        right=Side(style='thin'),
        top=Side(style='thin'),
        bottom=Side(style='thin')
    )

    center_align = Alignment(horizontal='center', vertical='center')
    wrap_align = Alignment(horizontal='left', vertical='center', wrap_text=True)

    # Write headers
    for col, header in enumerate(headers, 1):
        cell = ws.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.fill = header_fill
        cell.alignment = center_align
        cell.border = thin_border

    # Write data
    for row_idx, rule in enumerate(rules, 2):
        for col_idx, value in enumerate(rule, 1):
            cell = ws.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border

            # Category column styling
            if col_idx == 1:
                category = value
                if category in category_fills:
                    cell.fill = category_fills[category]
                cell.alignment = center_align

            # Y markers in phase/sub-step columns
            elif col_idx >= 4 and col_idx <= 16:
                cell.alignment = center_align
                if value == "Y":
                    cell.fill = y_fill
                    cell.font = Font(bold=True)

            # Effect columns
            elif col_idx >= 17 and col_idx <= 20:
                cell.alignment = wrap_align

            # Implementation status
            elif col_idx == 21:
                cell.alignment = center_align
                if value == "YES":
                    cell.fill = yes_fill
                    cell.font = Font(bold=True, color="006100")
                elif value == "PARTIAL":
                    cell.fill = partial_fill
                    cell.font = Font(bold=True, color="9C5700")
                elif value == "NO":
                    cell.fill = no_fill
                    cell.font = Font(bold=True, color="9C0006")

            # Rule name
            elif col_idx == 2:
                cell.font = Font(bold=True)
                category = rule[0]
                if category in category_fills:
                    cell.fill = category_fills[category]

            # Description
            elif col_idx == 3:
                cell.alignment = wrap_align
                category = rule[0]
                if category in category_fills:
                    cell.fill = category_fills[category]

    # Set column widths
    column_widths = {
        'A': 12,   # Category
        'B': 18,   # Rule
        'C': 45,   # Description
        'D': 5, 'E': 5, 'F': 5, 'G': 5, 'H': 5, 'I': 5, 'J': 5,  # Phases
        'K': 5, 'L': 5, 'M': 5, 'N': 5, 'O': 5, 'P': 5,  # Sub-steps
        'Q': 40,   # Grants
        'R': 25,   # Condition
        'S': 12,   # Value
        'T': 10,   # Target
        'U': 10,   # IMP
    }

    for col_letter, width in column_widths.items():
        ws.column_dimensions[col_letter].width = width

    # Freeze first row
    ws.freeze_panes = 'A2'

    # Add legend sheet
    ws_legend = wb.create_sheet("Legend")

    legend_data = [
        ("Column", "Meaning"),
        ("", ""),
        ("PHASE COLUMNS", ""),
        ("DEP", "Deployment - rule applies during unit deployment"),
        ("MOV", "Movement - rule applies during movement phase"),
        ("CHG", "Charge - rule applies during charge declaration/movement"),
        ("SHT", "Shooting - rule applies during shooting phase"),
        ("MEL", "Melee - rule applies during melee combat"),
        ("MOR", "Morale - rule applies during morale checks"),
        ("RND", "Round - rule applies at round start/end"),
        ("", ""),
        ("SUB-STEP COLUMNS", ""),
        ("ATK", "Attack Declaration - choosing targets, declaring attacks"),
        ("HIT", "Hit Roll - Quality test to determine hits"),
        ("DEF", "Defense Roll - Save test to block wounds"),
        ("WND", "Wound Calculation - modifying wounds dealt"),
        ("ALC", "Allocation - assigning wounds to models"),
        ("RGN", "Regeneration - rolls to ignore wounds"),
        ("", ""),
        ("EFFECT COLUMNS", ""),
        ("Grants", "What bonus/effect the rule provides"),
        ("Condition", "When/how the effect triggers"),
        ("Value", "Numeric value or modifier"),
        ("Target", "Who is affected: Self, Enemy, or Weapon"),
        ("", ""),
        ("IMPLEMENTATION STATUS", ""),
        ("YES", "Fully implemented in combat engine"),
        ("PARTIAL", "Partially implemented, may need work"),
        ("NO", "Not yet implemented"),
    ]

    for row_idx, (col1, col2) in enumerate(legend_data, 1):
        ws_legend.cell(row=row_idx, column=1, value=col1)
        ws_legend.cell(row=row_idx, column=2, value=col2)

        if col1 in ["PHASE COLUMNS", "SUB-STEP COLUMNS", "EFFECT COLUMNS", "IMPLEMENTATION STATUS"]:
            ws_legend.cell(row=row_idx, column=1).font = Font(bold=True)
        if row_idx == 1:
            ws_legend.cell(row=row_idx, column=1).font = Font(bold=True)
            ws_legend.cell(row=row_idx, column=2).font = Font(bold=True)

    ws_legend.column_dimensions['A'].width = 20
    ws_legend.column_dimensions['B'].width = 50

    # Save
    output_path = "/home/user/Science-Battle-Simulator/docs/special_rules_review_matrix.xlsx"
    wb.save(output_path)
    print(f"Created: {output_path}")
    return output_path

if __name__ == "__main__":
    create_rules_matrix()
