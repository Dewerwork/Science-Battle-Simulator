#!/usr/bin/env python3
"""
Parse all_factions_merged.txt and create a CSV with unit counts per faction.

Reads the merged file and outputs a CSV with columns:
- Faction: The faction name
- Unit: The base unit name (without UID)
- Count: Number of loadout variants for that unit
"""

import csv
import re
from collections import defaultdict
from pathlib import Path
from typing import Dict, Tuple

# =========================================================
# SETTINGS
# =========================================================
# Input: The merged file location
OUTPUT_DIR = r"C:\Users\David\Documents\Army Factions\pipeline_output"
MERGED_FILENAME = "all_factions_merged.txt"

# Output: CSV filename
CSV_FILENAME = "unit_counts_by_faction.csv"

# Regex patterns
FACTION_HEADER_PATTERN = re.compile(r"^#\s+([^=].+)$")  # Lines like "# Faction Name"
UNIT_HEADER_PATTERN = re.compile(
    r"^(.+?)\s+\[(?:UID:[A-Fa-f0-9]+\]\s+\[)?(\d+)\]\s+Q\d+\+\s+D\d+\+\s+\|"
)


def parse_merged_file(merged_path: Path) -> Dict[Tuple[str, str], int]:
    """
    Parse the merged file and count units by faction.

    Returns a dict mapping (faction, unit_name) -> count
    """
    counts: Dict[Tuple[str, str], int] = defaultdict(int)
    current_faction = "Unknown"

    with merged_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()

            # Check for faction header
            if line.startswith("#"):
                # Skip separator lines
                if line.startswith("# ="):
                    continue
                match = FACTION_HEADER_PATTERN.match(line)
                if match:
                    current_faction = match.group(1).strip()
                continue

            # Check for unit header
            match = UNIT_HEADER_PATTERN.match(line)
            if match:
                unit_name = match.group(1).strip()
                counts[(current_faction, unit_name)] += 1

    return counts


def write_csv(counts: Dict[Tuple[str, str], int], csv_path: Path) -> None:
    """Write the counts to a CSV file."""
    # Sort by faction, then by unit name
    sorted_items = sorted(counts.items(), key=lambda x: (x[0][0].lower(), x[0][1].lower()))

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["Faction", "Unit", "Count"])

        for (faction, unit_name), count in sorted_items:
            writer.writerow([faction, unit_name, count])


def generate_summary(counts: Dict[Tuple[str, str], int]) -> None:
    """Print summary statistics."""
    total_loadouts = sum(counts.values())
    unique_units = len(counts)

    # Count factions
    factions = set(faction for faction, _ in counts.keys())

    # Units per faction
    faction_totals: Dict[str, int] = defaultdict(int)
    faction_unique: Dict[str, int] = defaultdict(int)
    for (faction, _), count in counts.items():
        faction_totals[faction] += count
        faction_unique[faction] += 1

    print(f"\n[SUMMARY]")
    print(f"  Total factions: {len(factions)}")
    print(f"  Total unique (faction, unit) pairs: {unique_units:,}")
    print(f"  Total loadout variants: {total_loadouts:,}")
    print(f"\n[BY FACTION]")

    for faction in sorted(factions, key=str.lower):
        print(f"  {faction}: {faction_unique[faction]} units, {faction_totals[faction]:,} loadouts")


def run() -> None:
    output_dir = Path(OUTPUT_DIR).expanduser()
    merged_path = output_dir / MERGED_FILENAME
    csv_path = output_dir / CSV_FILENAME

    if not merged_path.exists():
        raise FileNotFoundError(f"Merged file not found: {merged_path}")

    print(f"[INFO] Reading: {merged_path}")
    counts = parse_merged_file(merged_path)

    if not counts:
        print("[WARN] No units found in the merged file.")
        return

    print(f"[INFO] Writing CSV: {csv_path}")
    write_csv(counts, csv_path)

    generate_summary(counts)

    print(f"\n[OK] CSV created: {csv_path}")


if __name__ == "__main__":
    run()
