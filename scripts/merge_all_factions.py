#!/usr/bin/env python3
"""
Merge all faction-specific final merged text files into a single master JSON file.

This script finds all *.final.merged.txt files in the pipeline output directory
and combines them into one all_factions_merged.json file.
"""

import json
from pathlib import Path
from typing import List, Dict, Any

# =========================================================
# SETTINGS
# =========================================================
# Input: The pipeline output directory containing faction subdirectories
OUTPUT_DIR = r"C:\Users\David\Documents\Army Factions\pipeline_output4"

# Output: The merged file will be created in the output directory
MERGED_FILENAME = "all_factions_merged.json"


def find_merged_files(output_dir: Path) -> List[Path]:
    """Find all *.final.merged.txt files in the output directory."""
    merged_files = sorted(
        output_dir.glob("*/*.final.merged.txt"),
        key=lambda p: p.stem.lower()
    )
    return merged_files


def extract_faction_name(file_path: Path) -> str:
    """Extract faction name from the file path or filename."""
    # The faction name is usually in the parent directory name or file stem
    # e.g., "Blessed_Sisters_3.5.1/Blessed_Sisters.final.merged.txt"
    parent_name = file_path.parent.name
    # Remove version suffix like "_3.5.1" or "_unknown"
    parts = parent_name.rsplit("_", 1)
    if len(parts) > 1 and (parts[1].replace(".", "").isdigit() or parts[1] == "unknown"):
        return parts[0].replace("_", " ")
    return parent_name.replace("_", " ")


def merge_all_factions(output_dir: Path) -> Path:
    """Merge all faction files into one master JSON file."""
    merged_files = find_merged_files(output_dir)

    if not merged_files:
        print(f"[WARN] No *.final.merged.txt files found in {output_dir}")
        return None

    print(f"[INFO] Found {len(merged_files)} faction file(s) to merge:")
    for f in merged_files:
        print(f"  - {f.name}")

    factions_data: List[Dict[str, Any]] = []
    total_unit_count = 0

    for file_path in merged_files:
        faction_name = extract_faction_name(file_path)

        # Read file content
        try:
            content = file_path.read_text(encoding="utf-8", errors="replace")
        except Exception as e:
            print(f"[ERROR] Failed to read {file_path}: {e}")
            continue

        lines = content.splitlines()

        # Strip BOM if present
        if lines and lines[0].startswith("\ufeff"):
            lines[0] = lines[0].lstrip("\ufeff")

        # Skip empty files
        if not any(line.strip() for line in lines):
            print(f"[WARN] Skipping empty file: {file_path.name}")
            continue

        # Strip leading/trailing blank lines
        while lines and not lines[0].strip():
            lines.pop(0)
        while lines and not lines[-1].strip():
            lines.pop()

        # Count units for this faction
        unit_count = sum(1 for line in lines if " Q" in line and " D" in line and "pts |" in line)
        total_unit_count += unit_count

        factions_data.append({
            "name": faction_name,
            "units": lines,
            "unit_count": unit_count
        })

    # Build output structure
    output_data = {
        "factions": factions_data,
        "total_factions": len(factions_data),
        "total_unit_loadouts": total_unit_count
    }

    # Write output file
    out_file = output_dir / MERGED_FILENAME
    with open(out_file, "w", encoding="utf-8") as f:
        json.dump(output_data, f, indent=2, ensure_ascii=False)

    print(f"\n[OK] Merged {len(factions_data)} factions -> {out_file}")
    print(f"[OK] Total unit loadouts: ~{total_unit_count:,}")

    return out_file


def run() -> None:
    output_dir = Path(OUTPUT_DIR).expanduser()

    if not output_dir.exists():
        raise FileNotFoundError(f"Output directory not found: {output_dir}")

    merge_all_factions(output_dir)


if __name__ == "__main__":
    run()
