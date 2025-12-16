#!/usr/bin/env python3
"""
Reduce unit loadouts by grouping effectively identical units.

Units are bucketed by combat-effective characteristics with aggressive normalization.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# =========================================================
# Bucketing Functions
# =========================================================

def range_bucket(rng: Optional[int]) -> str:
    """Convert range to bucket: Melee, Short(1-12), Medium(13-24), Long(25+)"""
    if rng is None or rng == 0:
        return "Melee"
    if rng <= 12:
        return "Short"
    if rng <= 24:
        return "Medium"
    return "Long"


def attack_bucket(attacks: int) -> str:
    """Convert attacks to bucket: Low(1-2), Med(3-5), High(6-9), VHigh(10+)"""
    if attacks <= 2:
        return "Low"
    if attacks <= 5:
        return "Med"
    if attacks <= 9:
        return "High"
    return "VHigh"


def ap_bucket(ap: Optional[int]) -> str:
    """Convert AP to bucket: None, Low(1-2), High(3+)"""
    if ap is None or ap == 0:
        return "None"
    if ap <= 2:
        return "Low"
    return "High"


# =========================================================
# Weapon Special Normalization
# =========================================================

# Weapon specials to normalize (base name -> canonical)
WEAPON_SPECIAL_BUCKETS = {
    "deadly": "_DEADLY_",
    "blast": "_BLAST_",
    "rending": "_RENDING_",
    "reliable": "_RELIABLE_",
    "indirect": "_INDIRECT_",
}

# Weapon specials to ignore entirely
IGNORABLE_WEAPON_SPECIALS = {
    "shred", "takedown", "strafing",
}


def normalize_weapon_special(special: str) -> Optional[str]:
    """Normalize a weapon special rule."""
    special_lower = special.lower().strip()
    base = re.sub(r'\([^)]*\)', '', special_lower).strip()

    if base in IGNORABLE_WEAPON_SPECIALS:
        return None

    if base in WEAPON_SPECIAL_BUCKETS:
        return WEAPON_SPECIAL_BUCKETS[base]

    return special_lower


def normalize_weapon_specials(specials: Tuple[str, ...]) -> Tuple[str, ...]:
    """Normalize weapon specials list."""
    normalized = set()
    for s in specials:
        norm = normalize_weapon_special(s)
        if norm:
            normalized.add(norm)
    return tuple(sorted(normalized))


# =========================================================
# Unit Rule Normalization
# =========================================================

TERRAIN_IGNORE_RULES = {"flying", "strider", "hover"}
DEPLOYMENT_RULES = {"scout", "ambush", "infiltrate"}
SPEED_BOOST_RULES = {"fast"}
SPEED_PENALTY_RULES = {"slow"}
DEFENSE_BOOST_RULES = {"stealth", "shrouded"}

IGNORABLE_RULES = {
    "transport", "aircraft", "artillery", "no retreat",
    "fearless", "hero", "unique", "relentless",
}


def normalize_rule(rule: str) -> Optional[str]:
    """Normalize a unit rule for bucketing."""
    rule_lower = rule.lower().strip()
    base = re.sub(r'\([^)]*\)', '', rule_lower).strip()

    if base in IGNORABLE_RULES:
        return None
    if base in TERRAIN_IGNORE_RULES:
        return "_TERRAIN_"
    if base in DEPLOYMENT_RULES:
        return "_DEPLOY_"
    if base in SPEED_BOOST_RULES:
        return "_FAST_"
    if base in SPEED_PENALTY_RULES:
        return "_SLOW_"
    if base in DEFENSE_BOOST_RULES:
        return "_STEALTH_"

    # Bucket parameterized rules
    if base in ("tough", "fear", "regeneration", "caster"):
        return base  # Keep base without value

    return rule_lower


def normalize_rules(rules: Tuple[str, ...]) -> Tuple[str, ...]:
    """Normalize rules list."""
    normalized = set()
    for r in rules:
        norm = normalize_rule(r)
        if norm:
            normalized.add(norm)
    return tuple(sorted(normalized))


# =========================================================
# Data Classes
# =========================================================

@dataclass
class Weapon:
    name: str
    count: int
    range_inches: Optional[int]
    attacks: int
    ap: Optional[int]
    special: Tuple[str, ...]

    def effective_key(self) -> str:
        """Bucketed weapon key."""
        rng = range_bucket(self.range_inches)
        atk = attack_bucket(self.attacks)
        ap = ap_bucket(self.ap)
        specials = normalize_weapon_specials(self.special)
        spec_str = ";".join(specials) if specials else "-"
        return f"R={rng}|A={atk}|AP={ap}|S={spec_str}"

    def __str__(self) -> str:
        inner = [f"A{self.attacks}"]
        if self.ap:
            inner.append(f"AP({self.ap})")
        inner.extend(self.special)

        if self.range_inches and self.range_inches > 0:
            s = f'{self.range_inches}" {self.name} ({", ".join(inner)})'
        else:
            s = f'{self.name} ({", ".join(inner)})'

        return f"{self.count}x {s}" if self.count > 1 else s


@dataclass
class UnitLoadout:
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
        """Bucketed unit key."""
        stats = f"Q{self.quality}+|D{self.defense}+|S={self.size}"
        rules = ",".join(normalize_rules(self.rules))

        weapon_counts: Dict[str, int] = defaultdict(int)
        for w in self.weapons:
            weapon_counts[w.effective_key()] += w.count

        weapons = "|".join(f"{k}*{c}" for k, c in sorted(weapon_counts.items()))
        return f"{stats}||R={rules}||W={weapons}"

    def bucket_hash(self) -> str:
        return hashlib.sha1(self.effective_key().encode()).hexdigest()[:8].upper()


# =========================================================
# Parsing
# =========================================================

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


def parse_rules_str(rules_str: str) -> Tuple[str, ...]:
    if not rules_str or rules_str.strip() == "-":
        return ()

    rules, current, depth = [], "", 0
    for char in rules_str:
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        if char == "," and depth == 0:
            if current.strip():
                rules.append(current.strip())
            current = ""
        else:
            current += char
    if current.strip():
        rules.append(current.strip())
    return tuple(rules)


def parse_weapon_stats(stats: str) -> Tuple[int, Optional[int], Tuple[str, ...]]:
    attacks, ap, specials = 0, None, []
    parts, current, depth = [], "", 0

    for char in stats:
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        if char == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char
    if current.strip():
        parts.append(current.strip())

    for p in parts:
        p = p.strip()
        if m := re.match(r"^A(\d+)$", p, re.I):
            attacks = int(m.group(1))
        elif m := re.match(r"^AP\((-?\d+)\)$", p, re.I):
            ap = int(m.group(1))
        elif p:
            specials.append(p)

    return attacks, ap, tuple(specials)


def parse_weapons_line(line: str) -> List[Weapon]:
    if not line or line.strip() == "-":
        return []

    weapons = []
    parts, current, depth = [], "", 0

    for char in line:
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        if char == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char
    if current.strip():
        parts.append(current.strip())

    for part in parts:
        if m := WEAPON_RE.match(part.strip()):
            count = int(m.group("count") or 1)
            rng = int(m.group("range")) if m.group("range") else None
            name = m.group("name").strip()
            attacks, ap, specials = parse_weapon_stats(m.group("stats"))
            weapons.append(Weapon(name, count, rng, attacks, ap, specials))

    return weapons


def parse_file(path: Path) -> List[UnitLoadout]:
    loadouts = []
    lines = path.read_text(encoding="utf-8").splitlines()
    i = 0

    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue

        if m := HEADER_RE.match(line):
            header = line
            weapons_line = lines[i + 1].strip() if i + 1 < len(lines) else ""

            loadouts.append(UnitLoadout(
                name=m.group("name").strip(),
                size=int(m.group("size")),
                quality=int(m.group("q")),
                defense=int(m.group("d")),
                points=int(m.group("pts")),
                rules=parse_rules_str(m.group("rules")),
                weapons=parse_weapons_line(weapons_line),
                raw_header=header,
                raw_weapons=weapons_line,
            ))
            i += 1
        i += 1

    return loadouts


# =========================================================
# Reduction
# =========================================================

def bucket_loadouts(loadouts: List[UnitLoadout]) -> Dict[str, List[UnitLoadout]]:
    buckets: Dict[str, List[UnitLoadout]] = defaultdict(list)
    for lo in loadouts:
        buckets[lo.effective_key()].append(lo)
    return buckets


def select_representative(bucket: List[UnitLoadout]) -> UnitLoadout:
    return sorted(bucket, key=lambda x: (x.points, x.name))[len(bucket) // 2]


def format_header(rep: UnitLoadout, bucket_hash: str) -> str:
    rules = ", ".join(rep.rules) if rep.rules else "-"
    return (f"{rep.name} [BKT:{bucket_hash}] [{rep.size}] "
            f"Q{rep.quality}+ D{rep.defense}+ | {rep.points}pts | {rules}")


def format_weapons(weapons: List[Weapon]) -> str:
    return ", ".join(str(w) for w in weapons) if weapons else "-"


def reduce_loadouts(input_path: Path, output_path: Path) -> Dict[str, Any]:
    loadouts = parse_file(input_path)
    if not loadouts:
        return {"original": 0, "reduced": 0, "reduction_pct": 0}

    buckets = bucket_loadouts(loadouts)

    lines = []
    for key in sorted(buckets.keys()):
        bucket = buckets[key]
        rep = select_representative(bucket)
        lines.append(format_header(rep, rep.bucket_hash()))
        lines.append(format_weapons(rep.weapons))
        lines.append("")

    output_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")

    reduction_pct = (1 - len(buckets) / len(loadouts)) * 100 if loadouts else 0
    return {
        "original": len(loadouts),
        "reduced": len(buckets),
        "reduction_pct": reduction_pct,
    }


def main():
    parser = argparse.ArgumentParser(description="Reduce unit loadouts by bucketing.")
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()

    output = args.output or args.input.with_suffix(".reduced.txt")
    reduce_loadouts(args.input, output)
    return 0


if __name__ == "__main__":
    exit(main())
