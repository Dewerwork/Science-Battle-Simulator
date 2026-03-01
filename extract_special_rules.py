#!/usr/bin/env python3
"""
Extract special rules from army text files and count occurrences.

Usage: python extract_special_rules.py <input_file> [output_file]

Input format example:
Hive Lord [FID:19E5A4A7] [BKT:AF28C984] [1] Q3+ D2+ | 345pts | Fear(2), Fearless, Hero, Hive Bond, Tough(12)
18" Shredder Cannon (A4, Rending), 2x Heavy Razor Claws (A3, AP(1)), Stomp (A4, AP(1))

Output: CSV file with special rules and their counts
"""

import re
import sys
import csv
from collections import Counter
from pathlib import Path


def extract_special_rules(line: str) -> list[str]:
    """Extract special rules from a unit line."""
    # Match lines with the unit format: Name [...] | pts | special_rules
    # The special rules come after the last | on lines containing 'pts'
    if '|' not in line or 'pts' not in line:
        return []

    parts = line.split('|')
    if len(parts) < 3:
        return []

    # Special rules are in the last part after the pts section
    rules_part = parts[-1].strip()

    # Split by comma and clean up each rule
    rules = []
    for rule in rules_part.split(','):
        rule = rule.strip()
        if rule:
            rules.append(rule)

    return rules


def process_file(input_path: str) -> Counter:
    """Process input file and return counter of special rules."""
    rule_counts = Counter()

    with open(input_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            rules = extract_special_rules(line)
            rule_counts.update(rules)

    return rule_counts


def write_csv(rule_counts: Counter, output_path: str):
    """Write special rules and counts to CSV file."""
    with open(output_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['Special Rule', 'Count'])

        # Sort by count descending, then alphabetically
        sorted_rules = sorted(rule_counts.items(), key=lambda x: (-x[1], x[0]))

        for rule, count in sorted_rules:
            writer.writerow([rule, count])


def main():
    if len(sys.argv) < 2:
        print("Usage: python extract_special_rules.py <input_file> [output_file]")
        print("\nExtracts special rules from army text files and outputs a CSV with counts.")
        sys.exit(1)

    input_path = sys.argv[1]

    # Default output path: same name with _special_rules.csv suffix
    if len(sys.argv) >= 3:
        output_path = sys.argv[2]
    else:
        input_stem = Path(input_path).stem
        output_path = f"{input_stem}_special_rules.csv"

    if not Path(input_path).exists():
        print(f"Error: Input file '{input_path}' not found.")
        sys.exit(1)

    rule_counts = process_file(input_path)

    if not rule_counts:
        print("No special rules found in input file.")
        sys.exit(0)

    write_csv(rule_counts, output_path)

    print(f"Extracted {len(rule_counts)} unique special rules from {sum(rule_counts.values())} total occurrences.")
    print(f"Output written to: {output_path}")


if __name__ == '__main__':
    main()
