#!/usr/bin/env python3
"""Extract faction names and IDs from all_factions_merged.json."""

import json
import argparse
from pathlib import Path


def extract_faction_ids(json_path: Path) -> list[dict[str, str]]:
    """Extract faction name and ID pairs from merged JSON file."""
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    factions = []
    for faction in data.get('factions', []):
        factions.append({
            'name': faction.get('name', ''),
            'faction_id': faction.get('faction_id', '')
        })

    return factions


def main():
    parser = argparse.ArgumentParser(
        description='Extract faction names and IDs from all_factions_merged.json'
    )
    parser.add_argument(
        'input_file',
        type=Path,
        nargs='?',
        default=Path(__file__).parent.parent / 'docs' / 'all_factions_merged.json',
        help='Path to all_factions_merged.json (default: docs/all_factions_merged.json)'
    )
    parser.add_argument(
        '-o', '--output',
        type=Path,
        help='Output JSON file (if not specified, prints to stdout)'
    )
    parser.add_argument(
        '--csv',
        action='store_true',
        help='Output as CSV instead of JSON'
    )

    args = parser.parse_args()

    if not args.input_file.exists():
        print(f"Error: File not found: {args.input_file}")
        return 1

    factions = extract_faction_ids(args.input_file)

    if args.csv:
        output = "faction_id,name\n"
        for f in factions:
            output += f"{f['faction_id']},{f['name']}\n"
    else:
        output = json.dumps(factions, indent=2)

    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(output)
        print(f"Wrote {len(factions)} factions to {args.output}")
    else:
        print(output)

    return 0


if __name__ == '__main__':
    exit(main())
