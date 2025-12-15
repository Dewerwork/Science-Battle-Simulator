#!/usr/bin/env python3
"""
Parse all_factions_merged.txt and create a CSV with unit counts,
bucketing units together if they have the same loadout (ignoring point cost).

This groups units by:
- Faction
- Unit name
- Size
- Quality/Defense
- Rules
- Weapons

Two entries with identical loadouts but different point costs are counted as one.
"""

import csv
import re
from collections import defaultdict
from pathlib import Path
from typing import Dict, Tuple, Optional

# =========================================================
# SETTINGS
# =========================================================
# Input: The merged file location
OUTPUT_DIR = r"C:\Users\David\Documents\Army Factions\pipeline_output"
MERGED_FILENAME = "all_factions_merged.txt"

# Output: CSV filename
CSV_FILENAME = "unit_counts_by_loadout.csv"

# Regex to parse unit header
# Format: UnitName [UID:xxx] [size] Qn+ Dn+ | pts | rules
# or:     UnitName [size] Qn+ Dn+ | pts | rules
UNIT_HEADER_PATTERN = re.compile(
    r"^(.+?)\s+(?:\[UID:[A-Fa-f0-9]+\]\s+)?\[(\d+)\]\s+Q(\d+)\+\s+D(\d+)\+\s+\|\s*(\d+)pts\s+\|\s*(.*)$"
)


def parse_merged_file(merged_path: Path) -> Dict[Tuple[str, str, str], int]:
    """
    Parse the merged file and count units by loadout signature (ignoring points).

    Returns a dict mapping (faction, unit_name, loadout_signature) -> count
    The loadout_signature includes size, Q, D, rules, and weapons but NOT points.
    """
    counts: Dict[Tuple[str, str, str], int] = defaultdict(int)
    current_faction = "Unknown"
    pending_header: Optional[Tuple[str, str, str, str, str, str]] = None  # name, size, q, d, rules

    with merged_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            raw_line = line
            line = line.strip()

            # Check for faction header
            if line.startswith("#"):
                if line.startswith("# ="):
                    continue
                # Extract faction name
                faction_match = re.match(r"^#\s+([^=].+)$", line)
                if faction_match:
                    current_faction = faction_match.group(1).strip()
                continue

            # Check for unit header
            header_match = UNIT_HEADER_PATTERN.match(line)
            if header_match:
                # If we had a pending header without weapons, flush it
                if pending_header:
                    faction, name, sig = pending_header
                    counts[(faction, name, sig)] += 1

                unit_name = header_match.group(1).strip()
                size = header_match.group(2)
                quality = header_match.group(3)
                defense = header_match.group(4)
                # points = header_match.group(5)  # Ignored!
                rules = header_match.group(6).strip()

                # Store pending - we need to read the next line for weapons
                pending_header = (current_faction, unit_name, size, quality, defense, rules)
                continue

            # If we have a pending header and this line has content, it's the weapons line
            if pending_header and line:
                faction, name, size, quality, defense, rules = pending_header
                weapons = line

                # Build signature WITHOUT point cost
                signature = f"[{size}] Q{quality}+ D{defense}+ | {rules} || {weapons}"

                counts[(faction, name, signature)] += 1
                pending_header = None
                continue

            # Empty line - flush any pending header without weapons
            if not line and pending_header:
                faction, name, size, quality, defense, rules = pending_header
                signature = f"[{size}] Q{quality}+ D{defense}+ | {rules} || (no weapons)"
                counts[(faction, name, signature)] += 1
                pending_header = None

    # Flush final pending header if any
    if pending_header:
        faction, name, size, quality, defense, rules = pending_header
        signature = f"[{size}] Q{quality}+ D{defense}+ | {rules} || (no weapons)"
        counts[(faction, name, signature)] += 1

    return counts


def write_csv(counts: Dict[Tuple[str, str, str], int], csv_path: Path) -> None:
    """Write the counts to a CSV file."""
    # Sort by faction, then by unit name, then by signature
    sorted_items = sorted(
        counts.items(),
        key=lambda x: (x[0][0].lower(), x[0][1].lower(), x[0][2].lower())
    )

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["Faction", "Unit", "Loadout Signature", "Count"])

        for (faction, unit_name, signature), count in sorted_items:
            writer.writerow([faction, unit_name, signature, count])


def generate_summary(counts: Dict[Tuple[str, str, str], int]) -> None:
    """Print summary statistics."""
    total_entries = sum(counts.values())
    unique_loadouts = len(counts)

    # Find duplicates (count > 1)
    duplicates = [(k, v) for k, v in counts.items() if v > 1]
    duplicate_count = len(duplicates)
    duplicate_entries = sum(v for _, v in duplicates)

    # Count factions
    factions = set(faction for faction, _, _ in counts.keys())

    # Stats per faction
    faction_unique: Dict[str, int] = defaultdict(int)
    faction_total: Dict[str, int] = defaultdict(int)
    faction_dupes: Dict[str, int] = defaultdict(int)

    for (faction, _, _), count in counts.items():
        faction_unique[faction] += 1
        faction_total[faction] += count
        if count > 1:
            faction_dupes[faction] += 1

    print(f"\n[SUMMARY]")
    print(f"  Total factions: {len(factions)}")
    print(f"  Unique loadouts (ignoring points): {unique_loadouts:,}")
    print(f"  Total entries in file: {total_entries:,}")

    if duplicate_count > 0:
        print(f"\n[DUPLICATES] (same loadout, different point costs)")
        print(f"  Loadouts with duplicates: {duplicate_count:,}")
        print(f"  Total duplicate entries: {duplicate_entries:,}")

        # Show top duplicates
        top_dupes = sorted(duplicates, key=lambda x: x[1], reverse=True)[:10]
        print(f"\n  Top duplicates:")
        for (faction, unit, sig), count in top_dupes:
            print(f"    {count}x {unit} ({faction})")
    else:
        print(f"\n[OK] No duplicate loadouts found (all entries have unique signatures)")

    print(f"\n[BY FACTION]")
    for faction in sorted(factions, key=str.lower):
        dupe_note = f" ({faction_dupes[faction]} with dupes)" if faction_dupes[faction] else ""
        print(f"  {faction}: {faction_unique[faction]:,} unique loadouts, {faction_total[faction]:,} total{dupe_note}")


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
