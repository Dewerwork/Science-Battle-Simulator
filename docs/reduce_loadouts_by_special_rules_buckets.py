#!/usr/bin/env python3
"""
Reduce unit loadouts by mapping special rules to their bucket supersets.

Uses special_rules_buckets.xlsx to map faction-specific rules (like "Devout",
"Piercing Assault", "Regeneration") to mechanical bucket equivalents. This allows
units with functionally similar rules to be grouped together.

Example: "Precise", "Good Shot", and other +1 hit modifier rules all map to BUCKET_001,
allowing units differing only in these rules to be grouped.

Input formats:
- JSON: all_factions_merged.json from merge_all_factions.py
- TXT: Legacy .txt format with unit loadouts

Modes:
- Normal: Maps rules to buckets, keeps parameterized values distinct
- Aggressive (-a): Groups similar buckets into super-categories for maximum reduction

Output: One representative per bucket in .final.merged.txt format
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

try:
    import pandas as pd
except ImportError:
    print("Error: pandas is required. Install with: pip install pandas openpyxl")
    exit(1)


# Default path to the special rules buckets file
DEFAULT_BUCKETS_FILE = Path(__file__).parent / "special_rules_buckets.xlsx"

# Range buckets for grouping weapons
RANGE_BUCKETS = [0, 6, 12, 18, 24, 30, 36]

# Pattern to detect weapon strings that got incorrectly parsed as rules
# These typically have range like 12" or attack stats like (A3)
WEAPON_PATTERN = re.compile(
    r'^\d+x\s+|'  # Starts with count like "3x "
    r'^\d+"\s+|'  # Starts with range like '12" '
    r'\(A\d+|'     # Contains (A# attack stats
    r'\s+\(A\d+|'  # Contains (A# attack stats
    r'^Take one\s+|'  # Upgrade instruction
    r'^\d+x\s+\d+"\s+'  # Pattern like "2x 24"
)

# Truncated rule names and their full versions
TRUNCATED_RULES = {
    "AIRCRA": "AIRCRAFT",
    "COURAGE_BU": "COURAGE_BUFF",
    "GUARDED_BU": "GUARDED_BUFF",
    "PRECISION_SHOOTER_BU": "PRECISION_SHOOTER_BUFF",
    "CASTING_BU": "CASTING_BUFF",
    "DANGEROUS_TERRAIN_DEBU": "DANGEROUS_TERRAIN_DEBUFF",
    "MORALE_DEBU": "MORALE_DEBUFF",
}

# Super-categories for aggressive grouping mode
# These group multiple buckets that have similar mechanical effects
SUPER_CATEGORY_MAP = {
    # Hit modifiers (various +1 to hit effects)
    "BUCKET_001": "CAT_HIT_BOOST",
    "GOOD_SHOT": "CAT_HIT_BOOST",
    "BUCKET_036": "CAT_HIT_BOOST",  # Targeting Visor, etc.

    # Morale buffs
    "BUCKET_004": "CAT_MORALE",  # Hold the Line, Morale Boost, etc.
    "FEARLESS": "CAT_MORALE",
    "NO_RETREAT": "CAT_MORALE",

    # Damage boosts on 6s (Surge, Butcher, etc.)
    "BUCKET_005": "CAT_DAMAGE_ON_6",

    # Defense bonuses
    "STEALTH": "CAT_DEFENSE",
    "PROTECTED": "CAT_DEFENSE",
    "SHIELDED": "CAT_DEFENSE",
    "BUCKET_042": "CAT_DEFENSE",  # Evasive
    "BUCKET_048": "CAT_DEFENSE",  # Guardian

    # Movement advantages
    "FAST": "CAT_MOVEMENT",
    "STRIDER": "CAT_MOVEMENT",
    "FLYING": "CAT_MOVEMENT",
    "BUCKET_044": "CAT_MOVEMENT",  # Highborn, speed boosts

    # Deployment advantages
    "BUCKET_006": "CAT_DEPLOYMENT",  # Scout
    "BUCKET_009": "CAT_DEPLOYMENT",  # Ambush
    "AMBUSH": "CAT_DEPLOYMENT",

    # Aura/buff rules (often don't directly affect combat stats)
    "BUCKET_021": "CAT_AURA",  # Various buff auras
    "BUCKET_010": "CAT_AURA",  # Weapon boosters, etc.

    # Faction flavor rules (minimal combat impact)
    "DEVOUT": "CAT_FACTION",
    "STURDY": "CAT_FACTION",
    "FORTIFIED": "CAT_FACTION",
    "BUCKET_029": "CAT_FACTION",  # Mischievous, Rapid Blink, etc.

    # Regen-type effects
    "REGENERATION": "CAT_REGEN",
    "BUCKET_058": "CAT_REGEN",  # Self-repair, etc.
}

# Rules to strip parameter values in aggressive mode
# These rules differ only by their parameter value, which we want to ignore
STRIP_PARAMS_IN_AGGRESSIVE = {
    "TOUGH",   # Tough(3), Tough(6), Tough(12) -> just TOUGH
    "FEAR",    # Fear(2), Fear(4) -> just FEAR
    "IMPACT",  # Impact(3), Impact(6), Impact(12) -> just IMPACT
}


def range_bucket(rng: Optional[int]) -> str:
    """Convert range to bucket string."""
    if rng is None or rng == 0:
        return "Melee"
    for b in RANGE_BUCKETS:
        if rng <= b:
            return f"{b}\""
    return "36+\""


class SpecialRulesBucketMapper:
    """Maps special rules to their mechanical bucket equivalents."""

    def __init__(self, buckets_file: Path, aggressive: bool = False):
        self.rule_to_bucket: Dict[str, str] = {}
        self.bucket_to_rules: Dict[str, List[str]] = defaultdict(list)
        self.aggressive = aggressive
        self._load_buckets(buckets_file)

    def _normalize_rule_name(self, rule: str) -> str:
        """Normalize rule name for matching."""
        # Remove parenthetical values: "Tough(6)" -> "Tough"
        # But keep (X) pattern intact for template matching
        rule = rule.strip()
        # Replace (number) with (X) for matching
        normalized = re.sub(r'\(\d+\)', '(X)', rule)
        normalized = normalized.upper().replace(' ', '_').replace('-', '_')

        # Handle truncated rule names
        for truncated, full in TRUNCATED_RULES.items():
            if normalized == truncated or normalized.startswith(truncated):
                return full

        return normalized

    def _load_buckets(self, buckets_file: Path):
        """Load rule->bucket mappings from Excel file."""
        if not buckets_file.exists():
            print(f"Warning: Buckets file not found: {buckets_file}")
            return

        try:
            df = pd.read_excel(buckets_file, sheet_name='Rules_With_Buckets')
        except Exception as e:
            print(f"Warning: Could not read buckets file: {e}")
            return

        for _, row in df.iterrows():
            rule_name = row.get('Rule Name', '')
            bucket = row.get('Mechanical_Bucket', '')

            if pd.isna(rule_name) or pd.isna(bucket):
                continue

            rule_name = str(rule_name).strip()
            bucket = str(bucket).strip()

            if rule_name and bucket:
                # Store both the normalized and original versions
                normalized = self._normalize_rule_name(rule_name)
                self.rule_to_bucket[normalized] = bucket
                self.bucket_to_rules[bucket].append(rule_name)

                # Also store without (X) for rules that don't have parameters
                base_name = normalized.replace('(X)', '').strip('_')
                if base_name != normalized:
                    self.rule_to_bucket[base_name] = bucket

        print(f"Loaded {len(self.rule_to_bucket)} rule->bucket mappings")
        print(f"Total unique buckets: {len(self.bucket_to_rules)}")
        if self.aggressive:
            print(f"Aggressive mode: grouping {len(SUPER_CATEGORY_MAP)} buckets into super-categories")

    def is_weapon_string(self, rule: str) -> bool:
        """Check if a rule string is actually weapon data that was misparsed."""
        return bool(WEAPON_PATTERN.search(rule))

    def get_bucket(self, rule: str) -> str:
        """
        Get the bucket for a rule.

        Returns the bucket ID if found, otherwise returns the normalized rule name.
        This ensures rules not in the mapping are still handled consistently.
        """
        # Normalize the input rule
        normalized = self._normalize_rule_name(rule)

        # Try exact match first
        if normalized in self.rule_to_bucket:
            bucket = self.rule_to_bucket[normalized]
        else:
            # Try without the (X) parameter
            base_name = re.sub(r'\(X\)$', '', normalized).strip('_')
            if base_name in self.rule_to_bucket:
                bucket = self.rule_to_bucket[base_name]
            else:
                # Return normalized rule name if no bucket found
                bucket = f"UNMAPPED:{normalized}"

        # In aggressive mode, apply super-categories and strip parameter values
        if self.aggressive:
            # Strip parameter values for specified rules
            if bucket in STRIP_PARAMS_IN_AGGRESSIVE:
                bucket = bucket  # Keep as-is (already stripped)
            elif any(bucket.startswith(rule + "(") for rule in STRIP_PARAMS_IN_AGGRESSIVE):
                # Shouldn't happen since we normalize to (X), but just in case
                for rule in STRIP_PARAMS_IN_AGGRESSIVE:
                    if bucket.startswith(rule):
                        bucket = rule
                        break

            # Map buckets to super-categories
            if bucket in SUPER_CATEGORY_MAP:
                return SUPER_CATEGORY_MAP[bucket]

        return bucket

    def normalize_rules_to_buckets(self, rules: Tuple[str, ...]) -> Tuple[str, ...]:
        """Map a list of rules to their bucket equivalents."""
        buckets = set()
        for rule in rules:
            # Skip weapon strings that got misparsed as rules
            if self.is_weapon_string(rule):
                continue
            bucket = self.get_bucket(rule)
            buckets.add(bucket)
        return tuple(sorted(buckets))


# Rules to completely ignore (don't affect combat simulation significantly)
IGNORABLE_RULES = {
    "TRANSPORT",
    "AIRCRAFT",
    "ARTILLERY",
    "HERO",
    "UNIQUE",
}


@dataclass
class Weapon:
    """Parsed weapon data."""
    name: str
    count: int
    range_inches: Optional[int]  # None = melee
    attacks: int
    ap: Optional[int]
    special: Tuple[str, ...]

    def effective_key(self, mapper: Optional[SpecialRulesBucketMapper] = None) -> str:
        """Generate key based on effective combat characteristics.

        Includes weapon name and exact range - no cross-weapon or range bucketing.
        """
        # Use exact range, not bucketed
        if self.range_inches is None or self.range_inches == 0:
            rng_str = "Melee"
        else:
            rng_str = f"{self.range_inches}\""

        ap_str = str(self.ap) if self.ap is not None else "-"

        # Map weapon special rules to buckets if mapper provided
        if mapper:
            specials = mapper.normalize_rules_to_buckets(self.special)
        else:
            specials = tuple(sorted(self.special, key=str.lower))

        specials_str = ";".join(specials)
        # Include weapon name to prevent cross-weapon bucketing
        return f"N={self.name}|R={rng_str}|A={self.attacks}|AP={ap_str}|S={specials_str}"

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

    def base_name(self) -> str:
        """Get unit name without UID suffix for bucketing purposes.

        Strips patterns like [UID:XXXXXXXX] from unit names so that
        identical units with different UIDs can be grouped together.
        """
        # Remove [UID:XXXXXXXX] pattern (8 hex chars)
        name = re.sub(r'\s*\[UID:[0-9A-Fa-f]+\]', '', self.name)
        # Also remove [BKT:XXXXXXXX] pattern if present from prior processing
        name = re.sub(r'\s*\[BKT:[0-9A-Fa-f]+\]', '', name)
        return name.strip()

    def effective_key(self, mapper: Optional[SpecialRulesBucketMapper] = None) -> str:
        """Generate key for bucketing effectively identical units.

        Includes unit name (without UID) to prevent cross-unit and cross-faction bucketing.
        Only loadouts of the SAME unit with equivalent rules/weapons are grouped.
        """
        # Include unit name WITHOUT UID - this prevents cross-unit and cross-faction bucketing
        # while still allowing identical loadouts of the same unit to be grouped
        unit_id = f"UNIT={self.base_name()}"

        # Unit stats
        stats = f"Q{self.quality}+|D{self.defense}+|S={self.size}"

        # Map rules to buckets
        if mapper:
            rules_bucketed = mapper.normalize_rules_to_buckets(self.rules)
            # Filter out ignorable rules
            rules_bucketed = tuple(r for r in rules_bucketed
                                   if not any(ig in r.upper() for ig in IGNORABLE_RULES))
        else:
            rules_bucketed = tuple(sorted(self.rules, key=str.lower))

        rules_str = ",".join(rules_bucketed)

        # Build weapon multiset (count of each effective weapon type)
        weapon_counts: Dict[str, int] = defaultdict(int)
        for w in self.weapons:
            wkey = w.effective_key(mapper)
            weapon_counts[wkey] += w.count

        # Sort weapon keys for consistent hashing
        weapons_str = "|".join(f"{k}*{c}" for k, c in sorted(weapon_counts.items()))

        return f"{unit_id}||{stats}||RULES={rules_str}||W={weapons_str}"

    def bucket_hash(self, mapper: Optional[SpecialRulesBucketMapper] = None) -> str:
        """Short hash for bucket identification."""
        return hashlib.sha1(self.effective_key(mapper).encode()).hexdigest()[:8].upper()


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
    r"\((?P<stats>.+)\)$"
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


def parse_lines_to_loadouts(lines: List[str]) -> List[UnitLoadout]:
    """Parse a list of lines into UnitLoadout objects."""
    loadouts = []

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


def parse_loadout_file(filepath: Path) -> List[UnitLoadout]:
    """Parse a .json or .txt file containing unit loadouts."""
    loadouts = []

    if filepath.suffix.lower() == ".json":
        # Parse JSON format from merge_all_factions.py
        with open(filepath, "r", encoding="utf-8") as f:
            data = json.load(f)

        for faction in data.get("factions", []):
            faction_name = faction.get("name", "Unknown")
            lines = faction.get("units", [])
            faction_loadouts = parse_lines_to_loadouts(lines)
            loadouts.extend(faction_loadouts)
            if faction_loadouts:
                print(f"    {faction_name}: {len(faction_loadouts)} loadouts")
    else:
        # Parse TXT format
        lines = filepath.read_text(encoding="utf-8").splitlines()
        loadouts = parse_lines_to_loadouts(lines)

    return loadouts


def bucket_loadouts(loadouts: List[UnitLoadout],
                    mapper: Optional[SpecialRulesBucketMapper] = None) -> Dict[str, List[UnitLoadout]]:
    """Group loadouts by their effective key."""
    buckets: Dict[str, List[UnitLoadout]] = defaultdict(list)

    for lo in loadouts:
        key = lo.effective_key(mapper)
        buckets[key].append(lo)

    return buckets


def select_representative(bucket: List[UnitLoadout]) -> UnitLoadout:
    """Select a representative loadout from a bucket.

    Strategy: Pick the one with median points (or lowest if tied).
    """
    sorted_bucket = sorted(bucket, key=lambda x: (x.points, x.name))
    mid = len(sorted_bucket) // 2
    return sorted_bucket[mid]


def format_bucket_header(rep: UnitLoadout, bucket_size: int, bucket_hash: str) -> str:
    """Format header line with bucket info."""
    rules_str = ", ".join(rep.rules) if rep.rules else "-"
    return (f"{rep.name} [BKT:{bucket_hash}] [{rep.size}] "
            f"Q{rep.quality}+ D{rep.defense}+ | {rep.points}pts | {rules_str}")


def format_weapons_line(weapons: List[Weapon]) -> str:
    """Format weapons list for output."""
    if not weapons:
        return "-"
    return ", ".join(str(w) for w in weapons)


def reduce_loadouts(input_path: Path, output_path: Path,
                    buckets_file: Path,
                    show_mapping: bool = False,
                    aggressive: bool = False) -> Dict[str, Any]:
    """Main reduction function."""
    print(f"Loading special rules buckets from: {buckets_file}")
    mapper = SpecialRulesBucketMapper(buckets_file, aggressive=aggressive)

    print(f"\nReading loadouts from: {input_path}")
    loadouts = parse_loadout_file(input_path)
    print(f"  Parsed {len(loadouts)} loadouts")

    if not loadouts:
        print("  No loadouts found!")
        return {"original": 0, "reduced": 0, "reduction_pct": 0}

    # Show example mappings if requested
    if show_mapping and loadouts:
        print("\n=== Example Rule Mappings ===")
        sample_rules = set()
        for lo in loadouts[:50]:
            sample_rules.update(lo.rules)
        for rule in sorted(sample_rules)[:20]:
            bucket = mapper.get_bucket(rule)
            print(f"  {rule} -> {bucket}")
        print()

    # Bucket all loadouts
    print("\nBucketing loadouts by mapped special rules...")
    buckets = bucket_loadouts(loadouts, mapper)
    print(f"  Created {len(buckets)} buckets from {len(loadouts)} loadouts")

    # Also get stats without mapping for comparison
    buckets_no_map = bucket_loadouts(loadouts, None)
    print(f"  (Without bucket mapping: {len(buckets_no_map)} buckets)")

    # Select representatives and build output
    output_lines = []
    bucket_stats = []

    for key in sorted(buckets.keys()):
        bucket = buckets[key]
        rep = select_representative(bucket)
        bucket_hash = rep.bucket_hash(mapper)

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
    reduction_from_no_map = (1 - len(buckets) / len(buckets_no_map)) * 100 if buckets_no_map else 0

    print("\n" + "=" * 60)
    print("REDUCTION STATISTICS")
    print("=" * 60)
    print(f"  Original loadouts:         {len(loadouts):,}")
    print(f"  Without bucket mapping:    {len(buckets_no_map):,}")
    print(f"  With bucket mapping:       {len(buckets):,}")
    print(f"  Total reduction:           {reduction_pct:.1f}%")
    print(f"  Additional from mapping:   {reduction_from_no_map:.1f}%")
    print()

    # Per-unit stats
    print("Per-unit breakdown (top 20 by reduction):")
    print("-" * 60)
    unit_bucket_counts: Dict[str, Tuple[int, int]] = {}
    for stat in bucket_stats:
        unit = stat["unit"]
        if unit not in unit_bucket_counts:
            unit_bucket_counts[unit] = (0, 0)
        orig, buck = unit_bucket_counts[unit]
        unit_bucket_counts[unit] = (orig + stat["size"], buck + 1)

    # Calculate reductions and sort
    unit_reductions = []
    for unit, (orig, buck) in unit_bucket_counts.items():
        unit_reduction = (1 - buck / orig) * 100 if orig else 0
        unit_reductions.append((unit, orig, buck, unit_reduction))

    unit_reductions.sort(key=lambda x: -x[3])  # Sort by reduction %

    for unit, orig, buck, unit_reduction in unit_reductions[:20]:
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
        "without_mapping": len(buckets_no_map),
        "reduced": len(buckets),
        "reduction_pct": reduction_pct,
        "additional_reduction_pct": reduction_from_no_map,
        "bucket_stats": bucket_stats,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Reduce unit loadouts by mapping special rules to bucket supersets."
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input .json or .txt file with unit loadouts (e.g., all_factions_merged.json)"
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        help="Output file path (default: input.bucket_reduced.txt)"
    )
    parser.add_argument(
        "-b", "--buckets",
        type=Path,
        default=DEFAULT_BUCKETS_FILE,
        help="Path to special_rules_buckets.xlsx"
    )
    parser.add_argument(
        "-m", "--show-mapping",
        action="store_true",
        help="Show sample rule->bucket mappings"
    )
    parser.add_argument(
        "-a", "--aggressive",
        action="store_true",
        help="Use aggressive grouping mode (groups similar buckets into super-categories)"
    )

    args = parser.parse_args()

    input_path = args.input
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        return 1

    output_path = args.output
    if output_path is None:
        stem = input_path.stem.replace(".final.merged", "").replace(".final", "")
        output_path = input_path.parent / f"{stem}.bucket_reduced.txt"

    reduce_loadouts(input_path, output_path, args.buckets, args.show_mapping, args.aggressive)
    return 0


if __name__ == "__main__":
    exit(main())
