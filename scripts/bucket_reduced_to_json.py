#!/usr/bin/env python3
"""
Convert bucket_reduced.txt to structured JSON format.

Parses the .bucket_reduced.txt output and creates a JSON file with
structured unit data including parsed weapons and rules.
"""

import argparse
import json
import re
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# Regex for parsing unit header
# Example: APC [UID:6A9BF98F] [BKT:06B00E6E] [1] Q3+ D2+ | 310pts | Battleborn, Fast, ...
HEADER_RE = re.compile(
    r"^(?P<name>.+?)\s+"
    r"(?:\[UID:(?P<uid>[0-9A-Fa-f]+)\]\s+)?"
    r"\[BKT:(?P<bkt>[0-9A-Fa-f]+)\]\s+"
    r"\[(?P<size>\d+)\]\s+"
    r"Q(?P<q>\d)\+\s+D(?P<d>\d)\+\s+\|\s+"
    r"(?P<pts>\d+)\s*pts\s+\|\s+"
    r"(?P<rules>.*)$"
)

# Regex for parsing weapons
WEAPON_RE = re.compile(
    r"(?:(?P<count>\d+)x\s+)?"
    r"(?:(?P<range>\d+)\"\s+)?"
    r"(?P<name>[^(]+?)\s*"
    r"\((?P<stats>.+)\)$"
)


def parse_rules(rules_str: str) -> List[str]:
    """Parse rules string into list, handling nested parentheses."""
    if not rules_str or rules_str.strip() == "-":
        return []

    rules = []
    current = ""
    depth = 0

    for char in rules_str:
        if char == "(":
            depth += 1
            current += char
        elif char == ")":
            depth -= 1
            current += char
        elif char == "," and depth == 0:
            rule = current.strip()
            if rule:
                rules.append(rule)
            current = ""
        else:
            current += char

    rule = current.strip()
    if rule:
        rules.append(rule)

    return rules


def parse_weapon_stats(stats_str: str) -> Tuple[int, Optional[int], List[str]]:
    """Parse weapon stats like 'A3, AP(1), Rending' -> (attacks, ap, specials)."""
    attacks = 0
    ap = None
    specials = []

    parts = []
    current = ""
    depth = 0

    for char in stats_str:
        if char == "(":
            depth += 1
            current += char
        elif char == ")":
            depth -= 1
            current += char
        elif char == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char
    if current.strip():
        parts.append(current.strip())

    for part in parts:
        part = part.strip()
        if not part:
            continue

        m = re.match(r"^A(\d+)$", part, re.IGNORECASE)
        if m:
            attacks = int(m.group(1))
            continue

        m = re.match(r"^AP\((-?\d+)\)$", part, re.IGNORECASE)
        if m:
            ap = int(m.group(1))
            continue

        specials.append(part)

    return attacks, ap, specials


def parse_weapons_line(weapons_str: str) -> List[Dict[str, Any]]:
    """Parse weapons line into list of weapon dicts."""
    weapons = []

    if not weapons_str or weapons_str.strip() == "-":
        return weapons

    parts = []
    current = ""
    depth = 0

    for char in weapons_str:
        if char == "(":
            depth += 1
            current += char
        elif char == ")":
            depth -= 1
            current += char
        elif char == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char
    if current.strip():
        parts.append(current.strip())

    for part in parts:
        part = part.strip()
        if not part:
            continue

        m = WEAPON_RE.match(part)
        if m:
            count = int(m.group("count") or 1)
            range_str = m.group("range")
            range_inches = int(range_str) if range_str else None
            name = m.group("name").strip()
            stats_str = m.group("stats")

            attacks, ap, specials = parse_weapon_stats(stats_str)

            weapon = {
                "name": name,
                "count": count,
                "attacks": attacks,
            }
            if range_inches is not None:
                weapon["range"] = range_inches
            if ap is not None:
                weapon["ap"] = ap
            if specials:
                weapon["special"] = specials

            weapons.append(weapon)

    return weapons


def parse_bucket_reduced_file(filepath: Path) -> List[Dict[str, Any]]:
    """Parse a bucket_reduced.txt file into structured data."""
    units = []
    lines = filepath.read_text(encoding="utf-8").splitlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        if not line:
            i += 1
            continue

        m = HEADER_RE.match(line)
        if m:
            unit = {
                "name": m.group("name").strip(),
                "bucket_hash": m.group("bkt"),
                "size": int(m.group("size")),
                "quality": int(m.group("q")),
                "defense": int(m.group("d")),
                "points": int(m.group("pts")),
                "rules": parse_rules(m.group("rules")),
            }

            if m.group("uid"):
                unit["uid"] = m.group("uid")

            # Next line should be weapons
            i += 1
            if i < len(lines):
                weapons_line = lines[i].strip()
                unit["weapons"] = parse_weapons_line(weapons_line)
            else:
                unit["weapons"] = []

            units.append(unit)

        i += 1

    return units


def convert_to_json(input_path: Path, output_path: Path) -> None:
    """Convert bucket_reduced.txt to JSON."""
    print(f"Reading from: {input_path}")
    units = parse_bucket_reduced_file(input_path)
    print(f"  Parsed {len(units)} units")

    output_data = {
        "units": units,
        "total_units": len(units),
    }

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(output_data, f, indent=2, ensure_ascii=False)

    print(f"Wrote JSON to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Convert bucket_reduced.txt to JSON format."
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input .bucket_reduced.txt file"
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        help="Output JSON file path (default: input with .json extension)"
    )

    args = parser.parse_args()

    input_path = args.input
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1

    output_path = args.output
    if output_path is None:
        output_path = input_path.with_suffix(".json")

    convert_to_json(input_path, output_path)
    return 0


if __name__ == "__main__":
    exit(main())
