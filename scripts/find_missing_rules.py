#!/usr/bin/env python3
"""
Find special rules in unit loadouts that aren't accounted for in the review matrix.

This script:
1. Reads the merged all units text file
2. Extracts all special rules from unit stats and weapon profiles
3. Compares against the special_rules_review_matrix.xlsx
4. Reports any rules not found in the review matrix
"""

import re
import pandas as pd
from pathlib import Path
from collections import defaultdict


def normalize_rule_name(rule: str) -> str:
    """
    Normalize a rule name for comparison.
    - Strip whitespace
    - Convert to uppercase
    - Replace spaces/hyphens with underscores
    - Remove parenthetical values like (3) for parameterized rules
    """
    rule = rule.strip()
    # Extract base rule name (without parameter)
    match = re.match(r'^([A-Za-z][A-Za-z\s\-\']+?)(?:\s*\([\d\+]+\))?$', rule)
    if match:
        base_name = match.group(1).strip()
    else:
        base_name = rule

    # Normalize: uppercase, replace spaces/hyphens with underscores
    normalized = base_name.upper().replace(' ', '_').replace('-', '_').replace("'", '_')
    return normalized


# Known truncation mappings (source file has truncated rule names)
TRUNCATION_FIXES = {
    'AIRCRA': 'AIRCRAFT',
    'COURAGE_BU': 'COURAGE_BUFF',
    'GUARDED_BU': 'GUARDED_BUFF',
    'PRECISION_SHOOTER_BU': 'PRECISION_SHOOTER_BUFF',
    'CASTING_BU': 'CASTING_BUFF',
    'MORALE_DEBU': 'MORALE_DEBUFF',
}

# Entries that are NOT rules (data quality issues in source file)
NOT_RULES = {
    'TAKE_ONE_BOSS_CARBINE_ATTACHMENT',
    'GREAT_ELEMENTAL_SWORD',
    'GREAT_SHIELD',
    'HOLY_STATUE',
    'HOLY_STATUE_S_FLAME_STRIKES',
    'A',  # Parsing artifact
}


def extract_rules_from_unit_line(line: str) -> list:
    """
    Extract special rules from a unit stats line.
    Format: UnitName [count] Q4+ D2+ | pts | Rule1, Rule2, Rule3...
    """
    rules = []

    # Find the part after the points value
    parts = line.split('|')
    if len(parts) >= 3:
        rules_part = parts[2].strip()
        # Split by comma, but be careful with parenthetical values
        # Use regex to split properly
        rule_matches = re.findall(r'([A-Za-z][A-Za-z\s\-\']*(?:\s*\([\d\+]+\))?)', rules_part)
        for rule in rule_matches:
            rule = rule.strip()
            if rule and not rule.startswith('Q') and not rule.startswith('D'):
                rules.append(rule)

    return rules


def extract_rules_from_weapon_line(line: str) -> list:
    """
    Extract special rules from a weapon line.
    Format: WeaponName (range, A#, Rule1, Rule2...)
    or: 5x WeaponName (A#, Rule1, Rule2...)
    """
    rules = []

    # Find content inside parentheses
    paren_match = re.search(r'\(([^)]+)\)', line)
    if paren_match:
        content = paren_match.group(1)
        # Split by comma
        parts = [p.strip() for p in content.split(',')]

        for part in parts:
            # Skip numeric/range values
            if re.match(r'^\d+\"?$', part):  # range like 12"
                continue
            if re.match(r'^A\d+$', part):  # attack count like A3
                continue
            if re.match(r'^Q\d+\+$', part):  # quality
                continue
            if re.match(r'^D\d+\+$', part):  # defense
                continue

            # This is likely a rule
            # Handle parameterized rules like AP(1), Blast(3)
            rule_match = re.match(r'^([A-Za-z][A-Za-z\s\-\']*(?:\s*\([\d\+]+\))?)$', part)
            if rule_match:
                rules.append(rule_match.group(1).strip())

    return rules


def load_known_rules(excel_path: str) -> dict:
    """
    Load known rules from the review matrix Excel file.
    Returns a dict mapping normalized names to original Rule IDs.
    """
    df = pd.read_excel(excel_path)

    known_rules = {}
    for _, row in df.iterrows():
        rule_id = str(row['Rule ID']).strip()
        rule_name = str(row['Rule Name']).strip() if pd.notna(row['Rule Name']) else ''

        # Add the Rule ID (normalized)
        normalized_id = normalize_rule_name(rule_id)
        known_rules[normalized_id] = rule_id

        # Also add the Rule Name (normalized) as an alias
        if rule_name:
            # Handle parameterized names like "AP(X)" -> "AP"
            base_name = re.sub(r'\s*\([^)]*\)\s*$', '', rule_name)
            normalized_name = normalize_rule_name(base_name)
            if normalized_name not in known_rules:
                known_rules[normalized_name] = rule_id

    return known_rules


def find_missing_rules(units_file: str, matrix_file: str) -> dict:
    """
    Main function to find rules in units file not in the matrix.

    Returns dict with:
    - missing_rules: set of rule names not found
    - rule_occurrences: dict mapping missing rules to where they appear
    - total_rules_found: total unique rules extracted
    - known_rules_matched: set of rules that were matched
    """
    # Load known rules
    known_rules = load_known_rules(matrix_file)
    print(f"Loaded {len(known_rules)} known rule variations from matrix")

    # Track results
    all_rules_found = set()
    rule_occurrences = defaultdict(list)

    # Read and parse the units file
    with open(units_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    current_unit = None
    line_num = 0

    for line in lines:
        line_num += 1
        line = line.strip()

        if not line:
            continue

        # Check if this is a unit line (contains Q#+ D#+)
        if re.search(r'Q\d+\+.*D\d+\+', line) or re.search(r'\[\d+\].*\|.*pts', line):
            # Extract unit name
            unit_match = re.match(r'^([^\[]+)', line)
            if unit_match:
                current_unit = unit_match.group(1).strip()

            # Extract rules from unit line
            rules = extract_rules_from_unit_line(line)
            for rule in rules:
                all_rules_found.add(rule)
                rule_occurrences[rule].append({
                    'line': line_num,
                    'unit': current_unit,
                    'context': 'unit_stat'
                })
        else:
            # This is likely a weapon line
            rules = extract_rules_from_weapon_line(line)
            for rule in rules:
                all_rules_found.add(rule)
                rule_occurrences[rule].append({
                    'line': line_num,
                    'unit': current_unit,
                    'context': 'weapon'
                })

    # Find missing rules
    missing_rules = set()
    matched_rules = set()
    truncated_rules = {}  # Maps truncated -> fixed name
    not_rules = set()  # Entries that are not actually rules

    for rule in all_rules_found:
        normalized = normalize_rule_name(rule)

        # Skip known non-rule entries
        if normalized in NOT_RULES:
            not_rules.add(rule)
            continue

        # Check for truncation fix
        if normalized in TRUNCATION_FIXES:
            fixed = TRUNCATION_FIXES[normalized]
            truncated_rules[rule] = fixed
            if fixed in known_rules:
                matched_rules.add(rule)
                continue
            else:
                # Truncated but the fixed version is also missing
                normalized = fixed  # Use the fixed version for subsequent matching

        if normalized in known_rules:
            matched_rules.add(rule)
        else:
            # Try some common variations
            found = False
            # Try without numbers
            base_normalized = re.sub(r'_*\d+_*', '', normalized)
            if base_normalized in known_rules:
                matched_rules.add(rule)
                found = True

            if not found:
                missing_rules.add(rule)

    return {
        'missing_rules': missing_rules,
        'rule_occurrences': dict(rule_occurrences),
        'total_rules_found': len(all_rules_found),
        'all_rules_found': all_rules_found,
        'known_rules_matched': matched_rules,
        'known_rules_db': known_rules,
        'truncated_rules': truncated_rules,
        'not_rules': not_rules
    }


def main():
    # Hardcoded file paths
    units_file = Path('/home/user/Science-Battle-Simulator/docs/MERGED_ALL_TXT.txt')
    matrix_file = Path('/home/user/Science-Battle-Simulator/docs/special_rules_review_matrix.xlsx')
    output_file = Path('/home/user/Science-Battle-Simulator/docs/missing_rules_report.txt')

    # Check files exist
    if not units_file.exists():
        print(f"ERROR: Units file not found: {units_file}")
        return
    if not matrix_file.exists():
        print(f"ERROR: Matrix file not found: {matrix_file}")
        return

    print("="*80)
    print("SPECIAL RULES GAP ANALYSIS")
    print("="*80)
    print(f"\nUnits file: {units_file}")
    print(f"Matrix file: {matrix_file}")
    print()

    # Run analysis
    results = find_missing_rules(str(units_file), str(matrix_file))

    print(f"\n{'='*80}")
    print("RESULTS")
    print("="*80)
    print(f"\nTotal unique rules found in units file: {results['total_rules_found']}")
    print(f"Rules matched to matrix: {len(results['known_rules_matched'])}")
    print(f"Rules NOT in matrix: {len(results['missing_rules'])}")
    print(f"Truncated rules (auto-fixed): {len(results['truncated_rules'])}")
    print(f"Non-rule entries filtered: {len(results['not_rules'])}")

    if results['truncated_rules']:
        print(f"\n{'='*80}")
        print("DATA QUALITY: Truncated rule names in source file")
        print("="*80)
        for orig, fixed in sorted(results['truncated_rules'].items()):
            print(f"  '{orig}' -> '{fixed}'")

    if results['not_rules']:
        print(f"\n{'='*80}")
        print("DATA QUALITY: Non-rule entries found in rule positions")
        print("="*80)
        for item in sorted(results['not_rules']):
            print(f"  - {item}")

    if results['missing_rules']:
        print(f"\n{'='*80}")
        print("MISSING RULES (not found in special_rules_review_matrix.xlsx)")
        print("="*80)

        # Sort missing rules and show with occurrence count
        missing_with_counts = []
        for rule in sorted(results['missing_rules']):
            count = len(results['rule_occurrences'].get(rule, []))
            missing_with_counts.append((rule, count))

        # Sort by count descending
        missing_with_counts.sort(key=lambda x: -x[1])

        print(f"\n{'Rule Name':<40} {'Occurrences':<15} {'Sample Unit'}")
        print("-"*80)

        for rule, count in missing_with_counts:
            occurrences = results['rule_occurrences'].get(rule, [])
            sample_unit = occurrences[0]['unit'] if occurrences else 'N/A'
            # Truncate long unit names
            if len(sample_unit) > 25:
                sample_unit = sample_unit[:22] + '...'
            print(f"{rule:<40} {count:<15} {sample_unit}")

        # Also write to a file
        with open(output_file, 'w') as f:
            f.write("MISSING RULES REPORT\n")
            f.write("="*80 + "\n\n")
            f.write(f"Total unique rules in units file: {results['total_rules_found']}\n")
            f.write(f"Rules matched to matrix: {len(results['known_rules_matched'])}\n")
            f.write(f"Rules NOT in matrix: {len(results['missing_rules'])}\n\n")

            f.write("MISSING RULES:\n")
            f.write("-"*80 + "\n")
            for rule, count in missing_with_counts:
                f.write(f"{rule:<40} (found {count} times)\n")
                # Show first 3 example occurrences
                occurrences = results['rule_occurrences'].get(rule, [])[:3]
                for occ in occurrences:
                    f.write(f"    - Line {occ['line']}: {occ['unit']} ({occ['context']})\n")

        print(f"\n\nDetailed report written to: {output_file}")
    else:
        print("\n✓ All rules in the units file are accounted for in the matrix!")

    # Also show rules that ARE matched for verification
    print(f"\n{'='*80}")
    print("SAMPLE OF MATCHED RULES (first 20)")
    print("="*80)
    matched_list = sorted(results['known_rules_matched'])[:20]
    for rule in matched_list:
        normalized = normalize_rule_name(rule)
        matrix_id = results['known_rules_db'].get(normalized, 'N/A')
        print(f"  {rule:<30} -> {matrix_id}")


if __name__ == '__main__':
    main()
