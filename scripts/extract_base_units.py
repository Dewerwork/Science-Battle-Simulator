#!/usr/bin/env python3
"""
Extract base units from army JSON files for simulation.

Reads *_units.json files and extracts only the base unit configurations
(without upgrades), outputting a batch_sim compatible text file.

Output format (two lines per unit):
  UnitName [FID:XXXXXXXX] [BKT:YYYYYYYY] [count] QX+ DX+ | Xpts | Rule1, Rule2, ...
  weapons line
"""

import argparse
import glob
import hashlib
import json
import sys
from pathlib import Path
from typing import Any, Dict, List


def compute_hash(data: str) -> str:
    """Compute an 8-character hex hash from a string."""
    return hashlib.md5(data.encode()).hexdigest()[:8].upper()


def compute_faction_id(army_name: str) -> str:
    """Compute FID hash from army name."""
    return compute_hash(f"faction:{army_name}")


def compute_bucket_hash(unit: Dict[str, Any]) -> str:
    """Compute BKT hash from unit properties (unique per base unit)."""
    # Include key properties to make hash unique per unit configuration
    hash_input = f"{unit.get('name', '')}:{unit.get('base_points', 0)}:{unit.get('size', 1)}"
    # Include weapon names to differentiate units with same name but different loadouts
    weapons = unit.get("weapons", [])
    weapon_str = ",".join(sorted(w.get("name", "") for w in weapons))
    hash_input += f":{weapon_str}"
    return compute_hash(hash_input)


def format_weapon(weapon: Dict[str, Any]) -> str:
    """Format a weapon for the text output."""
    count = weapon.get("count", 1)
    name = weapon.get("name", "Unknown")
    attacks = weapon.get("attacks", 1)
    ap = weapon.get("ap") or 0
    range_inches = weapon.get("range") or 0
    special_rules = weapon.get("special_rules", [])

    # Build weapon string
    parts = []

    # Count prefix (only if > 1)
    count_prefix = f"{count}x " if count > 1 else ""

    # Range prefix (only if > 0)
    range_prefix = f'{range_inches}" ' if range_inches > 0 else ""

    # Stats in parentheses
    stats = [f"A{attacks}"]
    if ap > 0:
        stats.append(f"AP({ap})")
    stats.extend(special_rules)

    return f"{count_prefix}{range_prefix}{name} ({', '.join(stats)})"


def format_unit_text(unit: Dict[str, Any], faction_id: str) -> str:
    """Format a unit as two-line text format for batch_sim."""
    name = unit.get("name", "Unknown")
    size = unit.get("size", 1)
    quality = unit.get("quality", 4)
    defense = unit.get("defense", 4)
    points = unit.get("base_points", 0)
    rules = unit.get("special_rules", [])
    weapons = unit.get("weapons", [])

    bucket_hash = compute_bucket_hash(unit)

    # Format rules
    rules_str = ", ".join(rules) if rules else "-"

    # Header line
    header = f"{name} [FID:{faction_id}] [BKT:{bucket_hash}] [{size}] Q{quality}+ D{defense}+ | {points}pts | {rules_str}"

    # Weapons line
    if weapons:
        weapons_str = ", ".join(format_weapon(w) for w in weapons)
    else:
        weapons_str = "-"

    return f"{header}\n{weapons_str}\n"


def extract_army_name(filepath: Path) -> str:
    """Extract army name from filename like 'GF_-_Alien_Hives_3.5.1_units.json'."""
    import re

    name = filepath.stem  # Remove .json
    name = name.replace("_units", "")  # Remove _units suffix

    # Remove version numbers like _3.5.1
    name = re.sub(r"_\d+\.\d+(\.\d+)?$", "", name)

    # Remove GF_-_ prefix if present
    if name.startswith("GF_-_"):
        name = name[5:]

    # Replace underscores with spaces for nicer display
    name = name.replace("_", " ")

    return name


def process_file(filepath: Path) -> tuple[str, List[Dict[str, Any]]]:
    """Process a single units JSON file and return (army_name, units)."""
    with open(filepath, "r", encoding="utf-8") as f:
        data = json.load(f)

    units = data.get("units", [])
    army_name = extract_army_name(filepath)

    return army_name, units


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract base units from army JSON files for batch_sim.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Single file
  %(prog)s --input docs/parsed_output/GF_-_Alien_Hives_3.5.1_units.json --output base_units.txt

  # Multiple files with glob pattern
  %(prog)s --input "docs/parsed_output/*_units.json" --output all_base_units.txt

  # All files in a directory
  %(prog)s --input-dir docs/parsed_output/ --output all_base_units.txt

Output format (two lines per unit):
  UnitName [FID:XXXXXXXX] [BKT:YYYYYYYY] [count] QX+ DX+ | Xpts | Rule1, Rule2
  weapons line (e.g., "3x ccws (A1), 12" flamer (A1, Blast(3))")
        """,
    )

    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument(
        "--input", "-i",
        type=str,
        help="Input file path or glob pattern (e.g., 'docs/*_units.json')",
    )
    input_group.add_argument(
        "--input-dir", "-d",
        type=Path,
        help="Directory containing *_units.json files",
    )

    parser.add_argument(
        "--output", "-o",
        type=Path,
        required=True,
        help="Output text file path",
    )

    args = parser.parse_args()

    # Collect input files
    input_files: List[Path] = []

    if args.input:
        # Handle glob pattern or single file
        matches = glob.glob(args.input)
        if not matches:
            print(f"Error: No files match pattern: {args.input}", file=sys.stderr)
            return 1
        input_files = [Path(m) for m in matches if m.endswith("_units.json")]
        if not input_files:
            # Maybe it's a single file without the _units.json suffix pattern
            input_files = [Path(m) for m in matches if m.endswith(".json")]
    else:
        # Directory mode
        if not args.input_dir.is_dir():
            print(f"Error: Not a directory: {args.input_dir}", file=sys.stderr)
            return 1
        input_files = list(args.input_dir.glob("*_units.json"))

    if not input_files:
        print("Error: No *_units.json files found", file=sys.stderr)
        return 1

    # Sort for consistent output
    input_files.sort()

    print(f"Processing {len(input_files)} file(s)...")

    # Process all files and collect output
    output_lines: List[str] = []
    total_units = 0

    for filepath in input_files:
        if not filepath.exists():
            print(f"  Warning: File not found, skipping: {filepath}", file=sys.stderr)
            continue

        print(f"  Reading: {filepath.name}")
        try:
            army_name, units = process_file(filepath)
            faction_id = compute_faction_id(army_name)

            for unit in units:
                output_lines.append(format_unit_text(unit, faction_id))
                total_units += 1

            print(f"    -> {len(units)} base units from {army_name} [FID:{faction_id}]")
        except json.JSONDecodeError as e:
            print(f"  Error: Invalid JSON in {filepath}: {e}", file=sys.stderr)
            continue
        except Exception as e:
            print(f"  Error processing {filepath}: {e}", file=sys.stderr)
            continue

    if not output_lines:
        print("Error: No units extracted", file=sys.stderr)
        return 1

    # Ensure output directory exists
    args.output.parent.mkdir(parents=True, exist_ok=True)

    # Write output
    with open(args.output, "w", encoding="utf-8") as f:
        f.write("\n".join(output_lines))

    print(f"\nWrote {total_units} base units to: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
