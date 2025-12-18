#!/usr/bin/env python3
"""Generate Excel spreadsheet for special rules engine configuration.

Reads faction-specific rules from Faction Specific Army Rules.xlsx and
generates an engine-ready configuration spreadsheet.
"""

from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.worksheet.datavalidation import DataValidation
import openpyxl
from collections import defaultdict
import re

def extract_rules_from_faction_file():
    """Extract all unique rules from the faction rules Excel file."""
    wb = openpyxl.load_workbook('/home/user/Science-Battle-Simulator/Faction Specific Army Rules.xlsx')
    ws = wb['Sheet1']

    rules = {}
    armies_per_rule = defaultdict(set)

    for row in ws.iter_rows(min_row=2, values_only=True):
        army, rule_type, rule_name, rule_text, type_col, once_per_game, once_per_activation = row
        if rule_name and rule_text:
            if rule_name not in rules:
                rules[rule_name] = {
                    'text': str(rule_text),
                    'type': type_col or rule_type or '',
                    'once_per_game': bool(once_per_game),
                    'once_per_activation': bool(once_per_activation)
                }
            armies_per_rule[rule_name].add(army)

    return rules, armies_per_rule

def parse_rule_for_engine(name, info):
    """Parse a rule's text to determine engine configuration."""
    text = info['text'].lower()
    rule_type = (info['type'] or '').lower()

    # Default values
    phases = {'DEP': '', 'MOV': '', 'CHG': '', 'SHT': '', 'MEL': '', 'MOR': '', 'RND': ''}
    trigger_step = ''
    trigger_condition = 'always'
    effect_type = ''
    effect_target = ''
    effect_value = ''
    target_unit = 'SELF'
    priority = '10'

    # Determine phases based on keywords in text
    if any(x in text for x in ['deploy', 'ambush', 'scout', 'infiltrate']):
        phases['DEP'] = 'Y'
    if any(x in text for x in ['move', 'advance', 'rush', 'fast', 'slow', 'strider', 'flying']):
        phases['MOV'] = 'Y'
    if any(x in text for x in ['charge', 'charging', 'impact']):
        phases['CHG'] = 'Y'
    if any(x in text for x in ['shoot', 'shooting', 'range', 'ranged']):
        phases['SHT'] = 'Y'
    if any(x in text for x in ['melee', 'close combat', 'fight']):
        phases['MEL'] = 'Y'
    if any(x in text for x in ['morale', 'shaken', 'routed', 'fearless', 'fear']):
        phases['MOR'] = 'Y'
    if any(x in text for x in ['beginning of', 'end of', 'round', 'activation']):
        phases['RND'] = 'Y'

    # If nothing detected, check type column
    if not any(phases.values()):
        if 'weapon' in rule_type:
            phases['SHT'] = 'Y'
            phases['MEL'] = 'Y'
        elif 'defense' in rule_type:
            phases['SHT'] = 'Y'
            phases['MEL'] = 'Y'
        elif 'movement' in rule_type:
            phases['MOV'] = 'Y'
        elif 'aura' in rule_type:
            phases['RND'] = 'Y'

    # Determine trigger step and effects based on keywords
    # Hit modifiers
    if '+1 to hit' in text or 'gets +1 to hit' in text:
        trigger_step = 'HIT_ROLL'
        effect_type = 'ADD'
        effect_target = 'quality_mod'
        effect_value = '1'
    elif '-1 to hit' in text or 'gets -1 to hit' in text:
        trigger_step = 'HIT_ROLL'
        effect_type = 'ADD'
        effect_target = 'quality_mod'
        effect_value = '-1'
    elif 'unmodified roll of 6 to hit' in text or 'unmodified results of 6 to hit' in text:
        trigger_step = 'AFTER_HIT'
        trigger_condition = 'unmodified_6_hit'
        if 'extra hit' in text or '+1 attack' in text or 'may roll +1 attack' in text:
            effect_type = 'ADD'
            effect_target = 'hits'
            effect_value = 'sixes_rolled'
        elif 'ap (+' in text or 'ap(+' in text:
            match = re.search(r'ap\s*\(\+?(\d+)\)', text)
            if match:
                effect_type = 'ADD'
                effect_target = 'ap'
                effect_value = match.group(1)
        elif 'extra wound' in text:
            effect_type = 'ADD'
            effect_target = 'wounds'
            effect_value = 'sixes_rolled'

    # Defense modifiers
    elif '+1 to defense' in text or 'gets +1 to defense' in text or '+1 defense' in text:
        trigger_step = 'DEF_ROLL'
        effect_type = 'ADD'
        effect_target = 'defense_mod'
        effect_value = '1'
    elif '-1 to defense' in text or 'gets -1 to defense' in text or '-1 defense' in text:
        trigger_step = 'DEF_ROLL'
        effect_type = 'ADD'
        effect_target = 'defense_mod'
        effect_value = '-1'
        target_unit = 'ENEMY'

    # Morale modifiers
    elif '+1 to morale' in text:
        trigger_step = 'MORALE'
        effect_type = 'ADD'
        effect_target = 'morale_mod'
        effect_value = '1'
    elif 'fearless' in text and ('reroll' in text or 'get fearless' in text):
        trigger_step = 'MORALE'
        effect_type = 'REROLL'
        effect_target = 'morale_test'
        effect_value = 'true'

    # Movement modifiers
    elif 'moves +' in text or '+1"' in text or '+2"' in text:
        trigger_step = 'MOVEMENT'
        effect_type = 'ADD'
        effect_target = 'move_distance'
        match = re.search(r'\+(\d+)"', text)
        if match:
            effect_value = match.group(1)

    # AP modifiers
    elif 'ap (+' in text or 'ap(+' in text:
        trigger_step = 'CALC_AP'
        effect_type = 'ADD'
        effect_target = 'ap'
        match = re.search(r'ap\s*\(\+?(\d+)\)', text)
        if match:
            effect_value = match.group(1)
    elif 'ap (' in text or 'ap(' in text:
        trigger_step = 'CALC_AP'
        effect_type = 'SET'
        effect_target = 'ap'
        match = re.search(r'ap\s*\((\d+)\)', text)
        if match:
            effect_value = match.group(1)

    # Blast/hits
    elif 'blast' in text:
        trigger_step = 'AFTER_HIT'
        effect_type = 'MULTIPLY'
        effect_target = 'hits'
        match = re.search(r'blast\s*\((\d+)\)', text)
        if match:
            effect_value = f"min({match.group(1)}, enemy_models)"

    # Deadly
    elif 'deadly' in text:
        trigger_step = 'AFTER_WOUND'
        effect_type = 'MULTIPLY'
        effect_target = 'wounds'
        match = re.search(r'deadly\s*\((\d+)\)', text)
        if match:
            effect_value = match.group(1)

    # Regeneration/ignore wounds
    elif 'ignore' in text and 'wound' in text:
        if 'regeneration' in text:
            trigger_step = 'REGEN'
            effect_type = 'NEGATE'
            effect_target = 'regeneration'
            effect_value = 'true'
            target_unit = 'ENEMY'
        else:
            trigger_step = 'REGEN'
            effect_type = 'ROLL'
            match = re.search(r'(\d)\+', text)
            if match:
                effect_target = 'ignore_wound'
                effect_value = f"{match.group(1)}+"

    # Aura effects
    elif 'pick one' in text or 'pick up to' in text:
        trigger_step = 'PRE_ATTACK'
        effect_type = 'GRANT'
        if 'enemy' in text:
            target_unit = 'ENEMY'
        if 'friendly' in text:
            target_unit = 'FRIENDLY'
        # Try to find what is granted
        if 'takes' in text and 'hit' in text:
            effect_type = 'DAMAGE'
            match = re.search(r'takes?\s+(\d+)\s+hits?', text)
            if match:
                effect_value = match.group(1)
                effect_target = 'direct_hits'

    # Takes X hits (direct damage)
    elif 'takes' in text and 'hit' in text:
        trigger_step = 'DIRECT_DAMAGE'
        effect_type = 'DAMAGE'
        target_unit = 'ENEMY'
        match = re.search(r'takes?\s+(\d+)\s+hits?', text)
        if match:
            effect_target = 'direct_hits'
            effect_value = match.group(1)

    # Counter
    elif 'counter' in text and 'strike' in text:
        trigger_step = 'COMBAT_ORDER'
        trigger_condition = 'is_charged'
        effect_type = 'SET'
        effect_target = 'strike_order'
        effect_value = 'first'

    # Ambush
    elif 'ambush' in text:
        phases['DEP'] = 'Y'
        trigger_step = 'DEPLOYMENT'
        effect_type = 'SET'
        effect_target = 'deploy_type'
        effect_value = 'reserve'

    # Scout/Infiltrate
    elif 'scout' in text or 'infiltrate' in text:
        phases['DEP'] = 'Y'
        trigger_step = 'DEPLOYMENT'
        effect_type = 'ADD'
        effect_target = 'deploy_distance'
        effect_value = '12'

    # Flying
    elif 'flying' in text or 'fly over' in text:
        phases['MOV'] = 'Y'
        trigger_step = 'MOVEMENT'
        effect_type = 'NEGATE'
        effect_target = 'terrain_blocking'
        effect_value = 'true'

    # Strider
    elif 'strider' in text or 'ignore' in text and 'terrain' in text:
        phases['MOV'] = 'Y'
        trigger_step = 'MOVEMENT'
        effect_type = 'NEGATE'
        effect_target = 'difficult_terrain'
        effect_value = 'true'

    # Cover related
    elif 'cover' in text and 'ignore' in text:
        trigger_step = 'DEF_ROLL'
        effect_type = 'NEGATE'
        effect_target = 'cover_bonus'
        effect_value = 'true'
        target_unit = 'ENEMY'

    # Shielded
    elif 'shielded' in text:
        trigger_step = 'DEF_ROLL'
        trigger_condition = 'attack_not_spell'
        effect_type = 'ADD'
        effect_target = 'defense_mod'
        effect_value = '1'

    # Tough
    elif 'tough' in text:
        trigger_step = 'ALLOCATE'
        effect_type = 'SET'
        effect_target = 'wounds_to_kill'
        match = re.search(r'tough\s*\((\d+)\)', text)
        if match:
            effect_value = match.group(1)

    # Furious
    elif 'furious' in text:
        trigger_step = 'AFTER_HIT'
        trigger_condition = 'is_charging'
        effect_type = 'ADD'
        effect_target = 'hits'
        effect_value = 'sixes_rolled'

    # Relentless
    elif 'relentless' in text:
        trigger_step = 'AFTER_HIT'
        trigger_condition = 'range_over_9'
        effect_type = 'ADD'
        effect_target = 'hits'
        effect_value = 'sixes_rolled'

    # Handle once per game/activation
    if info['once_per_game']:
        trigger_condition = f"once_per_game AND {trigger_condition}" if trigger_condition != 'always' else 'once_per_game'
    if info['once_per_activation']:
        trigger_condition = f"once_per_activation AND {trigger_condition}" if trigger_condition != 'always' else 'once_per_activation'

    # If we still don't have phases, default to combat
    if not any(phases.values()):
        phases['SHT'] = 'Y'
        phases['MEL'] = 'Y'

    return {
        'phases': phases,
        'trigger_step': trigger_step,
        'trigger_condition': trigger_condition,
        'effect_type': effect_type,
        'effect_target': effect_target,
        'effect_value': effect_value,
        'target_unit': target_unit,
        'priority': priority
    }

def create_rule_id(name):
    """Create a code-friendly rule ID from the rule name."""
    # Remove parentheses content
    clean = re.sub(r'\s*\([^)]*\)', '', name)
    # Replace spaces and special chars with underscores
    clean = re.sub(r'[^a-zA-Z0-9]', '_', clean)
    # Remove consecutive underscores
    clean = re.sub(r'_+', '_', clean)
    # Remove leading/trailing underscores
    clean = clean.strip('_')
    return clean.upper()

def create_rules_matrix():
    wb = Workbook()
    ws = wb.active
    ws.title = "Special Rules"

    # Define headers
    headers = [
        "Rule ID",           # A
        "Rule Name",         # B
        "Description",       # C
        "DEP",               # D
        "MOV",               # E
        "CHG",               # F
        "SHT",               # G
        "MEL",               # H
        "MOR",               # I
        "RND",               # J
        "Trigger Step",      # K
        "Trigger Condition", # L
        "Effect Type",       # M
        "Effect Target",     # N
        "Effect Value",      # O
        "Effect Target Unit",# P
        "Priority",          # Q
        "Notes",             # R
    ]

    # Extract rules from faction file
    rules_data, armies_per_rule = extract_rules_from_faction_file()

    # Process all rules
    processed_rules = []
    for name in sorted(rules_data.keys()):
        info = rules_data[name]
        parsed = parse_rule_for_engine(name, info)

        # Truncate description if too long
        desc = info['text']
        if len(desc) > 500:
            desc = desc[:497] + '...'

        # Create notes with army list
        armies = armies_per_rule.get(name, set())
        armies_str = ', '.join(sorted(armies)[:5])  # First 5 armies
        if len(armies) > 5:
            armies_str += f' (+{len(armies)-5} more)'
        notes = f"Type: {info['type']}. Armies: {armies_str}"

        rule_tuple = (
            create_rule_id(name),
            name,
            desc,
            parsed['phases']['DEP'],
            parsed['phases']['MOV'],
            parsed['phases']['CHG'],
            parsed['phases']['SHT'],
            parsed['phases']['MEL'],
            parsed['phases']['MOR'],
            parsed['phases']['RND'],
            parsed['trigger_step'],
            parsed['trigger_condition'],
            parsed['effect_type'],
            parsed['effect_target'],
            parsed['effect_value'],
            parsed['target_unit'],
            parsed['priority'],
            notes
        )
        processed_rules.append(rule_tuple)

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

    # Write headers
    for col, header in enumerate(headers, 1):
        cell = ws.cell(row=1, column=col, value=header)
        cell.font = header_font
        cell.alignment = center_align
        cell.border = thin_border

        if col <= 3:
            cell.fill = header_fill
        elif col <= 10:
            cell.fill = phase_header_fill
        else:
            cell.fill = engine_header_fill

    # Write data
    for row_idx, rule in enumerate(processed_rules, 2):
        for col_idx, value in enumerate(rule, 1):
            cell = ws.cell(row=row_idx, column=col_idx, value=value)
            cell.border = thin_border

            if 4 <= col_idx <= 10:
                cell.alignment = center_align
                if value == "Y":
                    cell.fill = y_fill
                    cell.font = Font(bold=True)
            elif col_idx in [3, 18]:
                cell.alignment = wrap_align
            elif col_idx in [1, 2, 13, 16, 17]:
                cell.alignment = center_align
            else:
                cell.alignment = Alignment(horizontal='left', vertical='center')

    # Column widths
    column_widths = {
        'A': 25, 'B': 30, 'C': 60,
        'D': 5, 'E': 5, 'F': 5, 'G': 5, 'H': 5, 'I': 5, 'J': 5,
        'K': 18, 'L': 30, 'M': 12, 'N': 20, 'O': 20, 'P': 12, 'Q': 8, 'R': 50,
    }

    for col_letter, width in column_widths.items():
        ws.column_dimensions[col_letter].width = width

    for row in range(2, len(processed_rules) + 2):
        ws.row_dimensions[row].height = 40

    ws.freeze_panes = 'D2'

    # Data validation
    effect_types = '"ADD,MULTIPLY,SET,REROLL,REPLACE,NEGATE,ROLL,CHOOSE,TRIGGER,ENABLE,GRANT,DAMAGE"'
    dv_effect = DataValidation(type="list", formula1=effect_types, allow_blank=True)
    ws.add_data_validation(dv_effect)
    dv_effect.add(f'M2:M{len(processed_rules)+1}')

    target_units = '"SELF,ENEMY,WEAPON,FRIENDLY"'
    dv_target = DataValidation(type="list", formula1=target_units, allow_blank=True)
    ws.add_data_validation(dv_target)
    dv_target.add(f'P2:P{len(processed_rules)+1}')

    # Legend sheet
    ws_legend = wb.create_sheet("Legend")
    legend_data = [
        ("COLUMN REFERENCE", "", ""),
        ("", "", ""),
        ("Column", "Values", "Description"),
        ("", "", ""),
        ("Rule ID", "(text)", "Unique identifier used in code"),
        ("Rule Name", "(text)", "Display name with parameter notation"),
        ("Description", "(text)", "Full rule text from army book"),
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
        ("", "MOVEMENT", "During movement"),
        ("", "DEPLOYMENT", "During deployment"),
        ("", "DIRECT_DAMAGE", "Direct damage outside normal attacks"),
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
        ("", "attack_not_spell", "Attack is not a spell"),
        ("", "once_per_game", "Can only be used once per game"),
        ("", "once_per_activation", "Can only be used once per activation"),
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
        ("", "GRANT", "Grant a rule to another unit"),
        ("", "DAMAGE", "Deal direct damage"),
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
        ("", "direct_hits", "Direct hit damage"),
        ("", "", ""),
        ("Target Unit", "(from list)", "Who is affected"),
        ("", "SELF", "The unit with this rule"),
        ("", "ENEMY", "The opposing unit"),
        ("", "WEAPON", "The weapon being used"),
        ("", "FRIENDLY", "A friendly unit (for auras)"),
        ("", "", ""),
        ("Priority", "(number)", "Lower numbers process first"),
    ]

    for row_idx, (col1, col2, col3) in enumerate(legend_data, 1):
        ws_legend.cell(row=row_idx, column=1, value=col1)
        ws_legend.cell(row=row_idx, column=2, value=col2)
        ws_legend.cell(row=row_idx, column=3, value=col3)

        if col1 in ["COLUMN REFERENCE", "PHASE COLUMNS", "ENGINE CONFIGURATION"] or (col1 == "Column"):
            ws_legend.cell(row=row_idx, column=1).font = Font(bold=True)
            if col1 == "Column":
                ws_legend.cell(row=row_idx, column=2).font = Font(bold=True)
                ws_legend.cell(row=row_idx, column=3).font = Font(bold=True)
        elif col1 and col2:
            ws_legend.cell(row=row_idx, column=1).font = Font(bold=True)

    ws_legend.column_dimensions['A'].width = 20
    ws_legend.column_dimensions['B'].width = 25
    ws_legend.column_dimensions['C'].width = 50

    # Save
    output_path = "/home/user/Science-Battle-Simulator/docs/special_rules_review_matrix.xlsx"
    wb.save(output_path)
    print(f"Created: {output_path}")
    print(f"Total rules: {len(processed_rules)}")
    return output_path

if __name__ == "__main__":
    create_rules_matrix()
