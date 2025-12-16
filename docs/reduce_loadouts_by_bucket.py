#!/usr/bin/env python3
"""
Reduce unit loadouts by grouping effectively identical units.

Units are considered "effectively the same" if they have:
- Same Quality/Defense
- Same unit size
- Same Tough value
- Same special rules (normalized and sorted)
- Same weapon profiles (range bucket, AP, attacks, special rules)

Output: One representative per bucket in .final.merged.txt format
"""

from __future__ import annotations

import argparse
import hashlib
import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# Range buckets for grouping weapons
RANGE_BUCKETS = [0, 6, 12, 18, 24, 30, 36]


def range_bucket(rng: Optional[int]) -> str:
    """Convert range to bucket string."""
    if rng is None or rng == 0:
        return "Melee"
    for b in RANGE_BUCKETS:
        if rng <= b:
            return f"{b}\""
    return "36+\""


# =========================================================
# Rule Normalization - Group similar rules together
# =========================================================

# Rules that effectively allow ignoring/bypassing terrain
TERRAIN_IGNORE_RULES = {
    "flying", "strider", "hover",
}

# Rules that provide deployment/positioning advantages
DEPLOYMENT_RULES = {
    "scout", "ambush", "infiltrate",
}

# Rules that modify movement speed (we'll normalize the direction)
SPEED_BOOST_RULES = {
    "fast",
}

SPEED_PENALTY_RULES = {
    "slow",
}

# Rules that provide defensive bonuses
DEFENSE_BOOST_RULES = {
    "stealth", "shrouded",
}

# Rules to completely ignore (don't affect combat simulation much)
IGNORABLE_RULES = {
    "transport",  # Transport capacity doesn't affect direct combat
    "aircraft",   # Mostly affects movement, not combat stats
    "artillery",  # Affects targeting, not raw combat
    "no retreat", # Morale-related
    "fearless",   # Morale-related - wait, this might matter for simulation
    "hero",       # Just a unit type marker
    "unique",     # Just a marker
}


def normalize_rule_for_bucket(rule: str) -> Optional[str]:
    """
    Normalize a rule for bucketing purposes.
    Returns None if the rule should be ignored.
    Returns a canonical form if the rule should be grouped with similar rules.
    Returns the original rule (lowercased) if no normalization applies.
    """
    # Extract base rule name (without parenthetical values)
    # e.g., "Tough(6)" -> "tough", "Fear(2)" -> "fear"
    rule_lower = rule.lower().strip()
    base_rule = re.sub(r'\([^)]*\)', '', rule_lower).strip()

    # Check if this rule should be ignored entirely
    if base_rule in IGNORABLE_RULES:
        return None

    # Normalize terrain-ignoring rules
    if base_rule in TERRAIN_IGNORE_RULES:
        return "_TERRAIN_IGNORE_"

    # Normalize deployment advantage rules
    if base_rule in DEPLOYMENT_RULES:
        return "_DEPLOY_ADVANTAGE_"

    # Normalize speed rules
    if base_rule in SPEED_BOOST_RULES:
        return "_SPEED_BOOST_"

    if base_rule in SPEED_PENALTY_RULES:
        return "_SPEED_PENALTY_"

    # Normalize defensive rules
    if base_rule in DEFENSE_BOOST_RULES:
        return "_DEFENSE_BOOST_"

    # Keep rules with values (like Tough(6), Fear(2)) but normalize format
    # Extract the value if present for important combat rules
    if base_rule in ("tough", "fear", "regeneration", "caster"):
        # Keep these with their values as they significantly affect combat
        return rule_lower

    # Default: return lowercase version
    return rule_lower


def normalize_rules_list(rules: Tuple[str, ...]) -> Tuple[str, ...]:
    """
    Normalize a list of rules for bucketing.
    Groups similar rules and removes ignorable ones.
    """
    normalized = set()
    for rule in rules:
        norm = normalize_rule_for_bucket(rule)
        if norm is not None:
            normalized.add(norm)

    return tuple(sorted(normalized))


@dataclass
class Weapon:
    """Parsed weapon data."""
    name: str
    count: int
    range_inches: Optional[int]  # None = melee
    attacks: int
    ap: Optional[int]
    special: Tuple[str, ...]

    def effective_key(self) -> str:
        """Generate key based on effective combat characteristics."""
        rng = range_bucket(self.range_inches)
        ap_str = str(self.ap) if self.ap is not None else "-"
        specials = ";".join(sorted(self.special, key=str.lower))
        # Include attacks in the key since they matter for combat
        return f"R={rng}|A={self.attacks}|AP={ap_str}|S={specials}"

    def __str__(self) -> str:
        """Format weapon for output in OPR format."""
        inner_parts = [f"A{self.attacks}"]
        if self.ap is not None:
            inner_parts.append(f"AP({self.ap})")
        inner_parts.extend(self.special)
        inner_str = ", ".join(inner_parts)

        if self.range_inches is not None and self.range_inches > 0:
            weapon_str = f'{self.range_inches}" {self.name} ({inner_str})'
        else:
            weapon_str = f'{self.name} ({inner_str})'

        if self.count > 1:
            weapon_str = f"{self.count}x {weapon_str}"

        return weapon_str


@dataclass
class UnitLoadout:
    """Parsed unit loadout."""
    name: str
    size: int
    quality: int
    defense: int
    points: int
    rules: Tuple[str, ...]
    weapons: List[Weapon]
    raw_header: str
    raw_weapons: str

    def effective_key(self) -> str:
        """Generate key for bucketing effectively identical units."""
        # Unit stats
        stats = f"Q{self.quality}+|D{self.defense}+|S={self.size}"

        # Normalize and sort rules (groups similar rules like Flying/Strider)
        rules_normalized = normalize_rules_list(self.rules)
        rules_str = ",".join(rules_normalized)

        # Build weapon multiset (count of each effective weapon type)
        weapon_counts: Dict[str, int] = defaultdict(int)
        for w in self.weapons:
            wkey = w.effective_key()
            weapon_counts[wkey] += w.count

        # Sort weapon keys for consistent hashing
        weapons_str = "|".join(f"{k}*{c}" for k, c in sorted(weapon_counts.items()))

        return f"{stats}||RULES={rules_str}||W={weapons_str}"

    def bucket_hash(self) -> str:
        """Short hash for bucket identification."""
        return hashlib.sha1(self.effective_key().encode()).hexdigest()[:8].upper()


# Regex patterns for parsing
HEADER_RE = re.compile(
    r"^(?P<name>.+?)\s+\[(?P<size>\d+)\]\s+"
    r"Q(?P<q>\d)\+\s+D(?P<d>\d)\+\s+\|\s+"
    r"(?P<pts>\d+)\s*pts\s+\|\s+"
    r"(?P<rules>.*)$"
)

WEAPON_RE = re.compile(
    r"(?:(?P<count>\d+)x\s+)?"
    r"(?:(?P<range>\d+)\"\s+)?"
    r"(?P<name>[^(]+?)\s*"
    r"\((?P<stats>.+)\)$"  # Match to end, capturing nested parens
)


def parse_rules(rules_str: str) -> Tuple[str, ...]:
    """Parse rules string into tuple, handling nested parentheses."""
    if not rules_str or rules_str.strip() == "-":
        return ()

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

    # Don't forget the last rule
    rule = current.strip()
    if rule:
        rules.append(rule)

    return tuple(rules)


def parse_weapon_stats(stats_str: str) -> Tuple[int, Optional[int], Tuple[str, ...]]:
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

        # Check for attacks (A#)
        m = re.match(r"^A(\d+)$", part, re.IGNORECASE)
        if m:
            attacks = int(m.group(1))
            continue

        # Check for AP
        m = re.match(r"^AP\((-?\d+)\)$", part, re.IGNORECASE)
        if m:
            ap = int(m.group(1))
            continue

        # Everything else is a special rule
        specials.append(part)

    return attacks, ap, tuple(specials)


def parse_weapons_line(weapons_str: str) -> List[Weapon]:
    """Parse weapons line into list of Weapon objects."""
    weapons = []

    if not weapons_str or weapons_str.strip() == "-":
        return weapons

    # Split by comma, but respect parentheses
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

            weapons.append(Weapon(
                name=name,
                count=count,
                range_inches=range_inches,
                attacks=attacks,
                ap=ap,
                special=specials,
            ))

    return weapons


def parse_loadout_file(filepath: Path) -> List[UnitLoadout]:
    """Parse a .txt file containing unit loadouts."""
    loadouts = []
    lines = filepath.read_text(encoding="utf-8").splitlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Skip empty lines
        if not line:
            i += 1
            continue

        # Try to match header
        m = HEADER_RE.match(line)
        if m:
            header_line = line
            name = m.group("name").strip()
            size = int(m.group("size"))
            quality = int(m.group("q"))
            defense = int(m.group("d"))
            points = int(m.group("pts"))
            rules = parse_rules(m.group("rules"))

            # Next line should be weapons
            i += 1
            weapons_line = ""
            if i < len(lines):
                weapons_line = lines[i].strip()

            weapons = parse_weapons_line(weapons_line)

            loadouts.append(UnitLoadout(
                name=name,
                size=size,
                quality=quality,
                defense=defense,
                points=points,
                rules=rules,
                weapons=weapons,
                raw_header=header_line,
                raw_weapons=weapons_line,
            ))

        i += 1

    return loadouts


def bucket_loadouts(loadouts: List[UnitLoadout]) -> Dict[str, List[UnitLoadout]]:
    """Group loadouts by their effective key."""
    buckets: Dict[str, List[UnitLoadout]] = defaultdict(list)

    for lo in loadouts:
        key = lo.effective_key()
        buckets[key].append(lo)

    return buckets


def select_representative(bucket: List[UnitLoadout]) -> UnitLoadout:
    """Select a representative loadout from a bucket.

    Strategy: Pick the one with median points (or lowest if tied).
    This gives a "typical" loadout from the bucket.
    """
    sorted_bucket = sorted(bucket, key=lambda x: (x.points, x.name))
    mid = len(sorted_bucket) // 2
    return sorted_bucket[mid]


def format_bucket_header(rep: UnitLoadout, bucket_size: int, bucket_hash: str) -> str:
    """Format header line with bucket info."""
    # Add bucket info to the name
    rules_str = ", ".join(rep.rules) if rep.rules else "-"

    # Format: Name [BKT:hash] [size] Q#+ D#+ | pts | rules
    # Include points range if bucket has multiple loadouts
    return (f"{rep.name} [BKT:{bucket_hash}] [{rep.size}] "
            f"Q{rep.quality}+ D{rep.defense}+ | {rep.points}pts | {rules_str}")


def format_weapons_line(weapons: List[Weapon]) -> str:
    """Format weapons list for output."""
    if not weapons:
        return "-"
    return ", ".join(str(w) for w in weapons)


def reduce_loadouts(input_path: Path, output_path: Path) -> Dict[str, Any]:
    """Main reduction function."""
    print(f"Reading loadouts from: {input_path}")
    loadouts = parse_loadout_file(input_path)
    print(f"  Parsed {len(loadouts)} loadouts")

    if not loadouts:
        print("  No loadouts found!")
        return {"original": 0, "reduced": 0, "reduction_pct": 0}

    # Group by unit name first for better stats
    by_unit: Dict[str, List[UnitLoadout]] = defaultdict(list)
    for lo in loadouts:
        by_unit[lo.name].append(lo)

    # Bucket all loadouts
    print("\nBucketing loadouts by effective rules...")
    buckets = bucket_loadouts(loadouts)
    print(f"  Created {len(buckets)} buckets from {len(loadouts)} loadouts")

    # Select representatives and build output
    output_lines = []
    bucket_stats = []

    for key in sorted(buckets.keys()):
        bucket = buckets[key]
        rep = select_representative(bucket)
        bucket_hash = rep.bucket_hash()

        # Track stats
        points_range = (min(lo.points for lo in bucket), max(lo.points for lo in bucket))
        bucket_stats.append({
            "unit": rep.name,
            "bucket_hash": bucket_hash,
            "size": len(bucket),
            "points_range": points_range,
            "representative_points": rep.points,
        })

        # Format output
        header = format_bucket_header(rep, len(bucket), bucket_hash)
        weapons = format_weapons_line(rep.weapons)

        output_lines.append(header)
        output_lines.append(weapons)
        output_lines.append("")  # Blank line between units

    # Write output
    output_path.write_text("\n".join(output_lines).rstrip() + "\n", encoding="utf-8")
    print(f"\nWrote {len(buckets)} bucketed loadouts to: {output_path}")

    # Calculate and display stats
    reduction_pct = (1 - len(buckets) / len(loadouts)) * 100 if loadouts else 0

    print("\n" + "=" * 60)
    print("REDUCTION STATISTICS")
    print("=" * 60)
    print(f"  Original loadouts:  {len(loadouts):,}")
    print(f"  Bucketed loadouts:  {len(buckets):,}")
    print(f"  Reduction:          {reduction_pct:.1f}%")
    print()

    # Per-unit stats
    print("Per-unit breakdown:")
    print("-" * 60)
    unit_bucket_counts: Dict[str, Tuple[int, int]] = {}
    for stat in bucket_stats:
        unit = stat["unit"]
        if unit not in unit_bucket_counts:
            unit_bucket_counts[unit] = (0, 0)
        orig, buck = unit_bucket_counts[unit]
        unit_bucket_counts[unit] = (orig + stat["size"], buck + 1)

    for unit in sorted(unit_bucket_counts.keys()):
        orig, buck = unit_bucket_counts[unit]
        unit_reduction = (1 - buck / orig) * 100 if orig else 0
        print(f"  {unit}: {orig} -> {buck} ({unit_reduction:.1f}% reduction)")

    # Biggest buckets
    print()
    print("Largest buckets (most loadouts grouped together):")
    print("-" * 60)
    sorted_stats = sorted(bucket_stats, key=lambda x: -x["size"])[:10]
    for stat in sorted_stats:
        pts_min, pts_max = stat["points_range"]
        pts_str = f"{pts_min}pts" if pts_min == pts_max else f"{pts_min}-{pts_max}pts"
        print(f"  {stat['unit']} [BKT:{stat['bucket_hash']}]: "
              f"{stat['size']} loadouts, {pts_str}")

    return {
        "original": len(loadouts),
        "reduced": len(buckets),
        "reduction_pct": reduction_pct,
        "bucket_stats": bucket_stats,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Reduce unit loadouts by bucketing effectively identical units."
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input .txt file with unit loadouts (e.g., faction.final.merged.txt)"
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        help="Output file path (default: input.reduced.txt)"
    )

    args = parser.parse_args()

    input_path = args.input
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1

    output_path = args.output
    if output_path is None:
        stem = input_path.stem.replace(".final.merged", "").replace(".final", "")
        output_path = input_path.parent / f"{stem}.reduced.txt"

    reduce_loadouts(input_path, output_path)
    return 0


if __name__ == "__main__":
    exit(main())
