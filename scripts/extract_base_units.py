#!/usr/bin/env python3
"""
Extract base units from army JSON files for simulation.

Reads *_units.json files and extracts only the base unit configurations
(without upgrades), outputting a batch_sim compatible JSON file.
"""

import argparse
import glob
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional


def transform_weapon(weapon: Dict[str, Any]) -> Dict[str, Any]:
    """Transform a weapon to batch_sim compatible format."""
    return {
        "name": weapon.get("name", "Unknown"),
        "count": weapon.get("count", 1),
        "range": weapon.get("range") or 0,  # null -> 0 (melee)
        "attacks": weapon.get("attacks", 1),
        "ap": weapon.get("ap") or 0,  # null -> 0
        "special": weapon.get("special_rules", []),
    }


def transform_unit(unit: Dict[str, Any]) -> Dict[str, Any]:
    """Transform a unit to batch_sim compatible format (base only, no upgrades)."""
    transformed = {
        "name": unit.get("name", "Unknown"),
        "size": unit.get("size", 1),
        "quality": unit.get("quality", 4),
        "defense": unit.get("defense", 4),
        "points": unit.get("base_points", 0),
        "rules": unit.get("special_rules", []),
        "weapons": [transform_weapon(w) for w in unit.get("weapons", [])],
    }

    # Include tough if present and non-zero
    tough = unit.get("tough")
    if tough:
        transformed["tough"] = tough

    return transformed


def extract_army_name(filepath: Path) -> str:
    """Extract army name from filename like 'GF_-_Alien_Hives_3.5.1_units.json'."""
    name = filepath.stem  # Remove .json
    name = name.replace("_units", "")  # Remove _units suffix

    # Remove version numbers like _3.5.1
    import re
    name = re.sub(r"_\d+\.\d+(\.\d+)?$", "", name)

    # Remove GF_-_ prefix if present
    if name.startswith("GF_-_"):
        name = name[5:]

    return name


def process_file(filepath: Path) -> tuple[str, List[Dict[str, Any]]]:
    """Process a single units JSON file and return (army_name, units)."""
    with open(filepath, "r", encoding="utf-8") as f:
        data = json.load(f)

    units = data.get("units", [])
    army_name = extract_army_name(filepath)

    transformed_units = [transform_unit(u) for u in units]

    return army_name, transformed_units


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract base units from army JSON files for batch_sim.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Single file
  %(prog)s --input docs/parsed_output/GF_-_Alien_Hives_3.5.1_units.json --output base_units.json

  # Multiple files with glob pattern
  %(prog)s --input "docs/parsed_output/*_units.json" --output all_base_units.json

  # All files in a directory
  %(prog)s --input-dir docs/parsed_output/ --output all_base_units.json
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
        help="Output JSON file path",
    )

    parser.add_argument(
        "--include-metadata",
        action="store_true",
        help="Include metadata section with source files and timestamps",
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

    # Process all files
    all_units: List[Dict[str, Any]] = []
    source_files: List[str] = []
    armies_processed: List[str] = []

    for filepath in input_files:
        if not filepath.exists():
            print(f"  Warning: File not found, skipping: {filepath}", file=sys.stderr)
            continue

        print(f"  Reading: {filepath.name}")
        try:
            army_name, units = process_file(filepath)
            all_units.extend(units)
            source_files.append(filepath.name)
            armies_processed.append(army_name)
            print(f"    -> {len(units)} base units from {army_name}")
        except json.JSONDecodeError as e:
            print(f"  Error: Invalid JSON in {filepath}: {e}", file=sys.stderr)
            continue
        except Exception as e:
            print(f"  Error processing {filepath}: {e}", file=sys.stderr)
            continue

    if not all_units:
        print("Error: No units extracted", file=sys.stderr)
        return 1

    # Build output
    output_data: Dict[str, Any] = {"units": all_units}

    if args.include_metadata:
        output_data["metadata"] = {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "source_files": source_files,
            "armies": armies_processed,
            "total_units": len(all_units),
        }

    # Ensure output directory exists
    args.output.parent.mkdir(parents=True, exist_ok=True)

    # Write output
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(output_data, f, indent=2, ensure_ascii=False)

    print(f"\nWrote {len(all_units)} base units to: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
