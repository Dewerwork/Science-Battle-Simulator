#!/usr/bin/env python3
"""
Special Rules Matching Script

This script reads a CSV file containing special rules and matches them against
a text file with unit information. It performs basic text matching, stops at
the first match for each rule, and outputs results to a CSV file.
"""

import csv
from pathlib import Path

# =============================================================================
# CONFIGURATION - Hardcoded file paths
# =============================================================================
RULES_CSV_FILE = '/home/user/Science-Battle-Simulator/docs/special_rules_supergrouping.csv'
UNITS_TEXT_FILE = '/home/user/Science-Battle-Simulator/docs/MERGED_ALL_TXT.txt'
OUTPUT_CSV_FILE = '/home/user/Science-Battle-Simulator/docs/special_rules_match_results.csv'


def load_special_rules(csv_path: str) -> list[str]:
    """
    Load special rule names from the CSV file.

    Handles various CSV formats:
    - With header 'rule_name', 'Rule Name', or similar
    - Single column CSV (just rule names)
    - Always reads from the first column

    Args:
        csv_path: Path to the CSV file containing special rules

    Returns:
        List of rule names from the first column
    """
    rules = []
    with open(csv_path, 'r', encoding='utf-8') as f:
        # Try to detect if there's a header
        first_line = f.readline().strip()
        f.seek(0)  # Reset to beginning

        # Check if first line looks like a header
        first_lower = first_line.lower()
        has_header = any(h in first_lower for h in ['rule', 'name', 'special'])

        if has_header:
            reader = csv.DictReader(f)
            # Try various possible column names
            for row in reader:
                rule_name = None
                # Try different possible header names
                for key in ['rule_name', 'Rule Name', 'Rule_Name', 'rulename',
                            'RuleName', 'name', 'Name', 'rule', 'Rule']:
                    if key in row:
                        rule_name = row[key].strip()
                        break
                # If no known header found, use first column
                if rule_name is None and row:
                    first_key = list(row.keys())[0]
                    rule_name = row[first_key].strip()
                if rule_name:
                    rules.append(rule_name)
        else:
            # No header - just read first column directly
            reader = csv.reader(f)
            for row in reader:
                if row and row[0].strip():
                    rules.append(row[0].strip())

    return rules


def load_unit_text(text_path: str) -> str:
    """
    Load the unit text file content.

    Handles potential null bytes in the source file.

    Args:
        text_path: Path to the text file with unit information

    Returns:
        File content as string
    """
    with open(text_path, 'rb') as f:
        content = f.read()
    # Remove null bytes and decode
    content = content.replace(b'\x00', b'')
    return content.decode('utf-8', errors='replace')


def match_all_rules(rules: list[str], unit_text: str) -> list[dict]:
    """
    Match all rules against the unit text using optimized single-pass algorithm.

    Instead of searching the entire file for each rule (O(rules × lines)),
    this iterates through lines once and checks all unmatched rules per line.
    Uses simple case-insensitive string matching for speed.

    Args:
        rules: List of rule names to search for
        unit_text: The text content to search in

    Returns:
        List of result dictionaries for each rule
    """
    # Initialize results for all rules
    results = {rule: {
        'rule_name': rule,
        'has_match': False,
        'first_match_line': None,
        'context': None
    } for rule in rules}

    # Create lookup with lowercase rule names for case-insensitive matching
    # Maps lowercase rule name -> original rule name
    unmatched_rules = {rule.lower(): rule for rule in rules}

    # Single pass through the file
    lines = unit_text.splitlines()
    total_lines = len(lines)

    for line_num, line in enumerate(lines, start=1):
        if not unmatched_rules:
            # All rules matched, stop early
            break

        line_lower = line.lower()

        # Check each unmatched rule against this line
        matched_this_line = []
        for rule_lower, rule_original in unmatched_rules.items():
            if rule_lower in line_lower:
                # Found a match
                context = line.strip()
                if len(context) > 100:
                    context = context[:97] + '...'

                results[rule_original] = {
                    'rule_name': rule_original,
                    'has_match': True,
                    'first_match_line': line_num,
                    'context': context
                }
                matched_this_line.append(rule_lower)

        # Remove matched rules from unmatched set
        for rule_lower in matched_this_line:
            del unmatched_rules[rule_lower]

        # Progress indicator for large files
        if line_num % 500000 == 0:
            print(f"  Processed {line_num:,}/{total_lines:,} lines, "
                  f"{len(rules) - len(unmatched_rules)}/{len(rules)} rules matched...")

    # Return results in original order
    return [results[rule] for rule in rules]


def write_results_csv(results: list[dict], output_path: str):
    """
    Write the matching results to a CSV file.

    Args:
        results: List of result dictionaries
        output_path: Path to the output CSV file
    """
    fieldnames = ['rule_name', 'has_match', 'first_match_line', 'context']

    with open(output_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(results)


def main():
    """Main function to run the special rules matching."""
    rules_path = Path(RULES_CSV_FILE)
    units_path = Path(UNITS_TEXT_FILE)
    output_path = Path(OUTPUT_CSV_FILE)

    # Check input files exist
    if not rules_path.exists():
        print(f"ERROR: Rules CSV file not found: {rules_path}")
        return 1
    if not units_path.exists():
        print(f"ERROR: Units text file not found: {units_path}")
        return 1

    print("=" * 60)
    print("SPECIAL RULES MATCHING SCRIPT")
    print("=" * 60)
    print(f"\nRules CSV:    {rules_path}")
    print(f"Units file:   {units_path}")
    print(f"Output file:  {output_path}")
    print()

    # Load data
    print("Loading special rules...")
    rules = load_special_rules(str(rules_path))
    print(f"  Loaded {len(rules)} rules")

    print("Loading unit text file...")
    unit_text = load_unit_text(str(units_path))
    print(f"  Loaded {len(unit_text)} characters")
    print()

    # Perform matching
    print("Matching rules against unit text...")
    results = match_all_rules(rules, unit_text)

    # Calculate statistics
    matched_count = sum(1 for r in results if r['has_match'])
    unmatched_count = len(results) - matched_count

    print()
    print("=" * 60)
    print("RESULTS SUMMARY")
    print("=" * 60)
    print(f"Total rules:     {len(results)}")
    print(f"Rules matched:   {matched_count}")
    print(f"Rules unmatched: {unmatched_count}")
    if len(results) > 0:
        print(f"Match rate:      {matched_count/len(results)*100:.1f}%")
    else:
        print("Match rate:      N/A (no rules loaded)")
        print("\nERROR: No rules were loaded from the CSV file.")
        print("Please check that the CSV file has rule names in the first column.")
        return 1

    # Write output
    write_results_csv(results, str(output_path))
    print(f"\nResults written to: {output_path}")

    # Show sample of unmatched rules
    unmatched = [r for r in results if not r['has_match']]
    if unmatched:
        print(f"\nSample unmatched rules (first 10):")
        for r in unmatched[:10]:
            print(f"  - {r['rule_name']}")

    return 0


if __name__ == '__main__':
    exit(main())
