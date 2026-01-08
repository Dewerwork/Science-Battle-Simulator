#!/usr/bin/env python3
"""
Special Rules Matching Script

This script reads a CSV file containing special rules and matches them against
a text file with unit information. It performs basic text matching, stops at
the first match for each rule, and outputs results to a CSV file.
"""

import csv
import re
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

    Args:
        csv_path: Path to the CSV file containing special rules

    Returns:
        List of rule names from the first column
    """
    rules = []
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rule_name = row.get('rule_name', '').strip()
            if rule_name:
                rules.append(rule_name)
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


def find_rule_match(rule_name: str, unit_text: str) -> dict:
    """
    Search for a rule in the unit text and return match information.

    Stops at the first match found.

    Args:
        rule_name: The special rule name to search for
        unit_text: The full text content to search in

    Returns:
        Dictionary with 'found' (bool), 'line_number' (int or None),
        and 'context' (str or None - snippet of matching line)
    """
    # Escape special regex characters in rule name for safe matching
    escaped_rule = re.escape(rule_name)

    # Search line by line to get line number
    lines = unit_text.splitlines()
    for line_num, line in enumerate(lines, start=1):
        # Case-insensitive search for the rule name
        if re.search(escaped_rule, line, re.IGNORECASE):
            # Truncate context if too long
            context = line.strip()
            if len(context) > 100:
                context = context[:97] + '...'
            return {
                'found': True,
                'line_number': line_num,
                'context': context
            }

    return {
        'found': False,
        'line_number': None,
        'context': None
    }


def match_all_rules(rules: list[str], unit_text: str) -> list[dict]:
    """
    Match all rules against the unit text.

    Args:
        rules: List of rule names to search for
        unit_text: The text content to search in

    Returns:
        List of result dictionaries for each rule
    """
    results = []
    for rule in rules:
        match_info = find_rule_match(rule, unit_text)
        results.append({
            'rule_name': rule,
            'has_match': match_info['found'],
            'first_match_line': match_info['line_number'],
            'context': match_info['context']
        })
    return results


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
    print(f"Match rate:      {matched_count/len(results)*100:.1f}%")

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
