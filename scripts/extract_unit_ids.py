#!/usr/bin/env python3
"""
Extract unit IDs, names, and faction IDs from a merged faction file.

This script parses text files with [FID:...] and [BKT:...] tags and outputs
a CSV with unit identifiers for easy lookup and mapping.

Usage:
    python extract_unit_ids.py <input_file> [output.csv]
"""

import argparse
import csv
import re
from pathlib import Path


# Regex to parse unit header lines with FID and/or BKT tags
# Format: "Unit Name [FID:XXXXXXXX] [BKT:YYYYYYYY] [count] Q#+ D#+ | ###pts | rules"
HEADER_RE = re.compile(
    r"^(?P<name>.+?)\s+"
    r"(?:\[FID:(?P<fid>[A-Fa-f0-9]{8})\]\s+)?"
    r"(?:\[BKT:(?P<bkt>[A-Fa-f0-9]{8})\]\s+)?"
    r"\[(?P<count>\d+)\]\s+"
    r"Q(?P<q>\d)\+\s+D(?P<d>\d)\+\s*\|\s*"
    r"(?P<pts>\d+)\s*pts"
)


def extract_units(filepath: Path) -> list[dict]:
    """Extract unit information from a merged faction file."""
    units = []

    content = filepath.read_text(encoding="utf-8", errors="replace")
    lines = content.splitlines()

    unit_id = 0
    for line in lines:
        line = line.strip()
        if not line:
            continue

        match = HEADER_RE.match(line)
        if match:
            units.append({
                "unit_id": unit_id,
                "name": match.group("name").strip(),
                "faction_id": match.group("fid") or "",
                "bucket_hash": match.group("bkt") or "",
                "points": int(match.group("pts")),
                "models": int(match.group("count")),
                "quality": int(match.group("q")),
                "defense": int(match.group("d")),
            })
            unit_id += 1

    return units


def write_csv(units: list[dict], output_path: Path) -> None:
    """Write units to CSV file."""
    fieldnames = ["unit_id", "name", "faction_id", "bucket_hash", "points", "models", "quality", "defense"]

    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(units)


def print_faction_summary(units: list[dict]) -> None:
    """Print a summary of unique factions found."""
    faction_counts = {}
    for u in units:
        fid = u["faction_id"] or "(none)"
        faction_counts[fid] = faction_counts.get(fid, 0) + 1

    print("\nFaction ID Summary:")
    print("-" * 40)
    for fid, count in sorted(faction_counts.items(), key=lambda x: -x[1]):
        print(f"  {fid}: {count:,} units")
    print("-" * 40)
    print(f"  Total: {len(units):,} units across {len(faction_counts)} faction(s)")


def main():
    parser = argparse.ArgumentParser(
        description="Extract unit IDs and faction info from merged faction files."
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input file (text format with [FID:...] [BKT:...] tags)"
    )
    parser.add_argument(
        "output",
        type=Path,
        nargs="?",
        help="Output CSV file (default: input_units.csv)"
    )

    args = parser.parse_args()

    input_path = args.input
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1

    output_path = args.output
    if output_path is None:
        output_path = input_path.with_name(input_path.stem + "_units.csv")

    print(f"Reading: {input_path}")
    units = extract_units(input_path)

    if not units:
        print("Error: No units found in file")
        return 1

    print(f"Found {len(units):,} units")
    print_faction_summary(units)

    write_csv(units, output_path)
    print(f"\nWrote: {output_path}")

    return 0


if __name__ == "__main__":
    exit(main())
