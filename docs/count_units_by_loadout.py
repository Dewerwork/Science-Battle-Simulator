#!/usr/bin/env python3
"""
Parse all_factions_merged.txt and create a deduplicated text file,
bucketing units together if they have the same loadout (ignoring point cost).

This groups units by:
- Faction
- Unit name
- Size
- Quality/Defense
- Rules
- Weapons

Two entries with identical loadouts but different point costs are deduplicated,
keeping only one representative entry.

Outputs:
- A deduplicated text file in the same format as the simulator input
- A CSV with counts
- Summary statistics printed to console
"""

import csv
import re
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Tuple, Optional, NamedTuple

# =========================================================
# SETTINGS
# =========================================================
# Input: The merged file location
OUTPUT_DIR = r"C:\Users\David\Documents\Army Factions\pipeline_output"
MERGED_FILENAME = "all_factions_merged.txt"

# Output filenames
DEDUPED_FILENAME = "all_factions_deduplicated.txt"
CSV_FILENAME = "unit_counts_by_loadout.csv"

# Include faction headers in deduplicated output
ADD_FACTION_HEADERS = True

# Regex to parse unit header
# Format: UnitName [UID:xxx] [size] Qn+ Dn+ | pts | rules
# or:     UnitName [size] Qn+ Dn+ | pts | rules
UNIT_HEADER_PATTERN = re.compile(
    r"^(.+?)\s+(?:\[UID:[A-Fa-f0-9]+\]\s+)?\[(\d+)\]\s+Q(\d+)\+\s+D(\d+)\+\s+\|\s*(\d+)pts\s+\|\s*(.*)$"
)


class UnitEntry(NamedTuple):
    """Represents a parsed unit entry."""
    faction: str
    unit_name: str
    size: str
    quality: str
    defense: str
    points: str
    rules: str
    weapons: str
    header_line: str  # Original header line for output


def make_signature(entry: UnitEntry) -> str:
    """Create a signature for deduplication (excludes points)."""
    return f"{entry.unit_name}|[{entry.size}]|Q{entry.quality}+|D{entry.defense}+|{entry.rules}|{entry.weapons}"


def parse_merged_file(merged_path: Path) -> Tuple[Dict[str, List[UnitEntry]], Dict[Tuple[str, str], int]]:
    """
    Parse the merged file and collect unit entries.

    Returns:
    - entries_by_faction: dict mapping faction -> list of unique UnitEntry objects
    - counts: dict mapping (faction, signature) -> count of duplicates
    """
    # Track unique entries per faction
    seen_signatures: Dict[Tuple[str, str], UnitEntry] = {}  # (faction, sig) -> first entry
    counts: Dict[Tuple[str, str], int] = defaultdict(int)

    current_faction = "Unknown"
    pending: Optional[Tuple[str, str, str, str, str, str, str, str]] = None
    # pending = (faction, name, size, q, d, points, rules, header_line)

    with merged_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            stripped = line.strip()

            # Check for faction header
            if stripped.startswith("#"):
                # Flush pending if any
                if pending:
                    faction, name, size, q, d, pts, rules, header_line = pending
                    entry = UnitEntry(faction, name, size, q, d, pts, rules, "", header_line)
                    sig = make_signature(entry)
                    key = (faction, sig)
                    counts[key] += 1
                    if key not in seen_signatures:
                        seen_signatures[key] = entry
                    pending = None

                if stripped.startswith("# ="):
                    continue
                faction_match = re.match(r"^#\s+([^=].+)$", stripped)
                if faction_match:
                    current_faction = faction_match.group(1).strip()
                continue

            # Check for unit header
            header_match = UNIT_HEADER_PATTERN.match(stripped)
            if header_match:
                # Flush pending if any
                if pending:
                    faction, name, size, q, d, pts, rules, header_line = pending
                    entry = UnitEntry(faction, name, size, q, d, pts, rules, "", header_line)
                    sig = make_signature(entry)
                    key = (faction, sig)
                    counts[key] += 1
                    if key not in seen_signatures:
                        seen_signatures[key] = entry
                    pending = None

                unit_name = header_match.group(1).strip()
                size = header_match.group(2)
                quality = header_match.group(3)
                defense = header_match.group(4)
                points = header_match.group(5)
                rules = header_match.group(6).strip()

                pending = (current_faction, unit_name, size, quality, defense, points, rules, stripped)
                continue

            # If we have pending and this line has content, it's weapons
            if pending and stripped:
                faction, name, size, q, d, pts, rules, header_line = pending
                weapons = stripped

                entry = UnitEntry(faction, name, size, q, d, pts, rules, weapons, header_line)
                sig = make_signature(entry)
                key = (faction, sig)
                counts[key] += 1
                if key not in seen_signatures:
                    seen_signatures[key] = entry
                pending = None
                continue

            # Empty line - flush pending without weapons
            if not stripped and pending:
                faction, name, size, q, d, pts, rules, header_line = pending
                entry = UnitEntry(faction, name, size, q, d, pts, rules, "", header_line)
                sig = make_signature(entry)
                key = (faction, sig)
                counts[key] += 1
                if key not in seen_signatures:
                    seen_signatures[key] = entry
                pending = None

    # Flush final pending
    if pending:
        faction, name, size, q, d, pts, rules, header_line = pending
        entry = UnitEntry(faction, name, size, q, d, pts, rules, "", header_line)
        sig = make_signature(entry)
        key = (faction, sig)
        counts[key] += 1
        if key not in seen_signatures:
            seen_signatures[key] = entry

    # Group entries by faction
    entries_by_faction: Dict[str, List[UnitEntry]] = defaultdict(list)
    for (faction, _), entry in seen_signatures.items():
        entries_by_faction[faction].append(entry)

    # Sort entries within each faction by unit name
    for faction in entries_by_faction:
        entries_by_faction[faction].sort(key=lambda e: (e.unit_name.lower(), e.weapons.lower()))

    return dict(entries_by_faction), dict(counts)


def write_deduplicated_file(entries_by_faction: Dict[str, List[UnitEntry]], output_path: Path) -> int:
    """
    Write deduplicated entries to a text file in simulator format.

    Returns the total number of unique entries written.
    """
    lines: List[str] = []
    total_entries = 0

    for faction in sorted(entries_by_faction.keys(), key=str.lower):
        entries = entries_by_faction[faction]

        if ADD_FACTION_HEADERS:
            if lines:
                lines.append("")
            lines.append(f"# {'=' * 50}")
            lines.append(f"# {faction}")
            lines.append(f"# {'=' * 50}")
            lines.append("")

        for entry in entries:
            # Write header line (use original to preserve formatting)
            lines.append(entry.header_line)
            # Write weapons line
            if entry.weapons:
                lines.append(entry.weapons)
            lines.append("")
            total_entries += 1

    # Write to file
    output_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")

    return total_entries


def write_csv(counts: Dict[Tuple[str, str], int], csv_path: Path) -> None:
    """Write the counts to a CSV file."""
    # Sort by faction, then by signature
    sorted_items = sorted(counts.items(), key=lambda x: (x[0][0].lower(), x[0][1].lower()))

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["Faction", "Loadout Signature", "Count"])

        for (faction, signature), count in sorted_items:
            writer.writerow([faction, signature, count])


def generate_summary(counts: Dict[Tuple[str, str], int], unique_count: int) -> None:
    """Print summary statistics."""
    total_entries = sum(counts.values())
    unique_loadouts = len(counts)

    # Find duplicates (count > 1)
    duplicates = [(k, v) for k, v in counts.items() if v > 1]
    duplicate_count = len(duplicates)
    duplicate_entries = sum(v for _, v in duplicates)
    removed_count = total_entries - unique_loadouts

    # Count factions
    factions = set(faction for faction, _ in counts.keys())

    # Stats per faction
    faction_unique: Dict[str, int] = defaultdict(int)
    faction_total: Dict[str, int] = defaultdict(int)
    faction_dupes: Dict[str, int] = defaultdict(int)

    for (faction, _), count in counts.items():
        faction_unique[faction] += 1
        faction_total[faction] += count
        if count > 1:
            faction_dupes[faction] += 1

    print(f"\n{'=' * 60}")
    print(f"DEDUPLICATION SUMMARY")
    print(f"{'=' * 60}")
    print(f"  Total factions:                 {len(factions)}")
    print(f"  Total entries in original file: {total_entries:,}")
    print(f"  Unique loadouts (after dedup):  {unique_loadouts:,}")
    print(f"  Duplicates removed:             {removed_count:,}")

    if duplicate_count > 0:
        print(f"\n{'=' * 60}")
        print(f"DUPLICATES FOUND (same loadout, different point costs)")
        print(f"{'=' * 60}")
        print(f"  Loadouts with duplicates: {duplicate_count:,}")
        print(f"  Total duplicate entries:  {duplicate_entries:,}")

        # Show top duplicates
        top_dupes = sorted(duplicates, key=lambda x: x[1], reverse=True)[:10]
        print(f"\n  Top 10 most duplicated:")
        for (faction, sig), count in top_dupes:
            # Extract unit name from signature
            unit_name = sig.split("|")[0]
            print(f"    {count}x  {unit_name} ({faction})")
    else:
        print(f"\n[OK] No duplicate loadouts found")

    print(f"\n{'=' * 60}")
    print(f"BY FACTION")
    print(f"{'=' * 60}")
    for faction in sorted(factions, key=str.lower):
        removed = faction_total[faction] - faction_unique[faction]
        if removed > 0:
            print(f"  {faction}: {faction_unique[faction]:,} unique ({removed:,} removed)")
        else:
            print(f"  {faction}: {faction_unique[faction]:,} unique")


def run() -> None:
    output_dir = Path(OUTPUT_DIR).expanduser()
    merged_path = output_dir / MERGED_FILENAME
    deduped_path = output_dir / DEDUPED_FILENAME
    csv_path = output_dir / CSV_FILENAME

    if not merged_path.exists():
        raise FileNotFoundError(f"Merged file not found: {merged_path}")

    print(f"[INFO] Reading: {merged_path}")
    entries_by_faction, counts = parse_merged_file(merged_path)

    if not counts:
        print("[WARN] No units found in the merged file.")
        return

    print(f"[INFO] Writing deduplicated file: {deduped_path}")
    unique_count = write_deduplicated_file(entries_by_faction, deduped_path)

    print(f"[INFO] Writing CSV: {csv_path}")
    write_csv(counts, csv_path)

    generate_summary(counts, unique_count)

    print(f"\n{'=' * 60}")
    print(f"OUTPUT FILES")
    print(f"{'=' * 60}")
    print(f"  Deduplicated text: {deduped_path}")
    print(f"  Counts CSV:        {csv_path}")


if __name__ == "__main__":
    run()
