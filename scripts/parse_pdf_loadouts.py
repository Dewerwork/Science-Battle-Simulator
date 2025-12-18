#!/usr/bin/env python3
"""
PDF Unit Loadout Parser for OPR Grimdark Future

Parses faction PDF files and extracts unit data including:
- Unit stats (name, size, quality, defense, tough, points)
- Special rules
- Weapons with profiles
- Upgrade options with costs

Outputs:
- JSON: Structured data for programmatic use
- TXT: Loadout format compatible with the simulation pipeline

Usage:
    python parse_pdf_loadouts.py <pdf_path> [--output-dir <dir>]
    python parse_pdf_loadouts.py docs/GF\ -\ Alien\ Hives\ 3.5.1.pdf

Dependencies:
    pip install pymupdf  (preferred, more stable)
    OR pip install pdfplumber
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import List, Optional, Dict, Any, Tuple


# =============================================================================
# Data Classes
# =============================================================================

@dataclass
class Weapon:
    """Represents a weapon profile."""
    name: str
    count: int = 1
    range_inches: Optional[int] = None  # None = melee
    attacks: int = 1
    ap: Optional[int] = None
    special_rules: List[str] = field(default_factory=list)

    def to_string(self) -> str:
        """Convert to loadout format string."""
        parts = []

        # Count prefix
        if self.count > 1:
            parts.append(f"{self.count}x")

        # Range (if ranged)
        if self.range_inches is not None:
            parts.append(f'{self.range_inches}"')

        # Name
        parts.append(self.name)

        # Profile in parentheses
        profile_parts = [f"A{self.attacks}"]
        if self.ap is not None:
            profile_parts.append(f"AP({self.ap})")
        profile_parts.extend(self.special_rules)

        return " ".join(parts) + " (" + ", ".join(profile_parts) + ")"


@dataclass
class UpgradeOption:
    """Represents a single upgrade option."""
    text: str
    cost: int  # 0 for free
    rules_granted: List[str] = field(default_factory=list)
    weapon: Optional[Weapon] = None


@dataclass
class UpgradeGroup:
    """Represents a group of upgrade options (e.g., 'Replace any Heavy Razor Claw')."""
    header: str
    options: List[UpgradeOption] = field(default_factory=list)


@dataclass
class Unit:
    """Represents a complete unit entry from the PDF."""
    name: str
    size: int
    quality: int  # e.g., 3 for Q3+
    defense: int  # e.g., 2 for D2+
    base_points: int
    tough: Optional[int] = None
    special_rules: List[str] = field(default_factory=list)
    weapons: List[Weapon] = field(default_factory=list)
    upgrade_groups: List[UpgradeGroup] = field(default_factory=list)

    def to_loadout_string(self, total_points: Optional[int] = None) -> str:
        """Convert to loadout format string (header + weapons)."""
        pts = total_points if total_points is not None else self.base_points

        # Build rules string with model counts for multi-model units
        if self.size > 1:
            rules_str = ", ".join(f"{self.size}x {r}" if not r.startswith(f"{self.size}x")
                                  else r for r in self.special_rules)
        else:
            rules_str = ", ".join(self.special_rules)

        # Header line
        header = f"{self.name} [{self.size}] Q{self.quality}+ D{self.defense}+ | {pts}pts"
        if rules_str:
            header += f" | {rules_str}"

        # Weapons line
        weapon_strs = [w.to_string() for w in self.weapons]
        weapons_line = ", ".join(weapon_strs)

        return f"{header}\n{weapons_line}"


# =============================================================================
# PDF Text Extraction
# =============================================================================

def _filter_pdf_lines(text: str) -> List[str]:
    """Filter and clean PDF extracted text lines."""
    lines: List[str] = []
    prev_line = ""

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        # Skip headers/footers
        if line.startswith("GF - "):
            continue
        if re.fullmatch(r"V\d+(\.\d+)*", line):
            continue
        # Skip page numbers (standalone numbers at start/end of page blocks)
        # But keep single digits that are part of weapon stats (after A# lines)
        if re.fullmatch(r"\d+", line):
            # Keep if it's a small number (likely AP value) and previous line was an attack value
            if int(line) <= 10 and re.match(r"^A\d+$", prev_line):
                pass  # Keep this line - it's likely an AP value
            elif int(line) > 20:
                # Skip large numbers - likely page numbers
                continue
            # Small numbers in isolation might still be stats
        if re.fullmatch(r"\.{2,}", line):
            continue
        lines.append(line)
        prev_line = line
    return lines


def extract_text_from_pdf(pdf_path: str) -> List[str]:
    """Extract text lines from a PDF file using PyMuPDF (preferred) or pdfplumber."""

    # Try PyMuPDF first (more stable, faster)
    try:
        import fitz  # PyMuPDF
        all_lines: List[str] = []
        doc = fitz.open(pdf_path)
        for page in doc:
            # Extract text page by page to preserve table structure
            text = page.get_text("text")
            page_lines = _filter_pdf_lines(text)
            all_lines.extend(page_lines)
        doc.close()
        return all_lines
    except ImportError:
        pass  # Fall through to pdfplumber

    # Try pdfplumber as fallback
    try:
        import pdfplumber
        lines: List[str] = []
        with pdfplumber.open(pdf_path) as pdf:
            for page in pdf.pages:
                text = page.extract_text(x_tolerance=1.5, y_tolerance=2, use_text_flow=True) or ""
                lines.extend(_filter_pdf_lines(text))
        return lines
    except ImportError:
        pass

    raise RuntimeError(
        "Missing PDF library. Install one of:\n"
        "  pip install pymupdf  (recommended)\n"
        "  pip install pdfplumber"
    )


# =============================================================================
# Parsing Regexes
# =============================================================================

# Unit header: "Hive Lord [1] - 345pts"
UNIT_HEADER_RE = re.compile(
    r"^(?P<name>.+?)\s+\[(?P<size>\d+)\]\s*-\s*(?P<pts>\d+)pts$"
)

# Stats line: "Quality 3+ Defense 2+ Tough 12" (combined format)
STATS_RE = re.compile(
    r"^Quality\s+(?P<qua>\d)\+\s+Defense\s+(?P<def>\d)\+(?:\s+Tough\s+(?P<tough>\d+))?$"
)

# Split stats (PyMuPDF format): "Quality 3+", "Defense 2+", "Tough 12"
QUALITY_RE = re.compile(r"^Quality\s+(?P<qua>\d)\+$")
DEFENSE_RE = re.compile(r"^Defense\s+(?P<def>\d)\+$")
TOUGH_RE = re.compile(r"^Tough\s+(?P<tough>\d+)$")

# Weapon line: "2x Heavy Razor Claws - A3 1 -" or "Shredder Cannon 18" A4 - Rending"
WEAPON_TABLE_RE = re.compile(
    r"^(?:(?P<count>\d+)x\s+)?(?P<name>.+?)\s+"
    r"(?P<rng>\d+\"\s*|-)\s+"
    r"(?P<atk>A\d+)\s+"
    r"(?P<ap>-|\d+)\s+"
    r"(?P<spe>.+)$"
)

# Inline weapon in equipment column: "2x Heavy Razor Claws (A3, AP(1))"
INLINE_WEAPON_RE = re.compile(
    r"(?:(?P<count>\d+)x\s+)?(?P<name>[^(]+?)\s*\((?P<profile>[^)]+)\)"
)

# Cost pattern: "option text +20pts" or "option text Free"
COST_RE = re.compile(r"^(?P<text>.+?)\s+(?P<cost>(?:\+\d+pts|Free))$")

# Weapon profile pattern for inline parsing: "24", A4, AP(1), Rending"
WEAPON_PROFILE_RE = re.compile(
    r'^(?:(?P<rng>\d+)"\s*,\s*)?A(?P<atk>\d+)(?:\s*,\s*(?P<rest>.+))?$'
)


# =============================================================================
# Parsing Functions
# =============================================================================

def parse_weapon_profile(profile_str: str) -> Tuple[Optional[int], int, Optional[int], List[str]]:
    """
    Parse a weapon profile string like '24", A4, AP(1), Rending'.
    Returns: (range, attacks, ap, special_rules)
    """
    profile_str = profile_str.strip()
    parts = [p.strip() for p in profile_str.split(",")]

    range_inches: Optional[int] = None
    attacks: int = 1
    ap: Optional[int] = None
    special_rules: List[str] = []

    for part in parts:
        part = part.strip()
        if not part or part == "-":
            continue

        # Range: "24""
        if part.endswith('"'):
            try:
                range_inches = int(part.rstrip('"').strip())
            except ValueError:
                pass
            continue

        # Attacks: "A4"
        if part.startswith("A") and part[1:].isdigit():
            attacks = int(part[1:])
            continue

        # AP: "AP(1)" or just "1" in AP column
        ap_match = re.match(r"AP\((\d+)\)", part)
        if ap_match:
            ap = int(ap_match.group(1))
            continue

        # Everything else is a special rule
        special_rules.append(part)

    return range_inches, attacks, ap, special_rules


def parse_weapon_from_table_line(line: str) -> Optional[Weapon]:
    """Parse a weapon from table format line."""
    match = WEAPON_TABLE_RE.match(line)
    if not match:
        return None

    count = int(match.group("count") or 1)
    name = match.group("name").strip()
    rng = match.group("rng").strip()
    atk = match.group("atk")
    ap_str = match.group("ap")
    spe = match.group("spe").strip()

    # Parse range
    range_inches: Optional[int] = None
    if rng != "-":
        try:
            range_inches = int(rng.rstrip('" '))
        except ValueError:
            pass

    # Parse attacks
    attacks = int(atk.lstrip("A"))

    # Parse AP
    ap: Optional[int] = None
    if ap_str != "-":
        try:
            ap = int(ap_str)
        except ValueError:
            pass

    # Parse special rules
    special_rules: List[str] = []
    if spe and spe != "-":
        special_rules = [s.strip() for s in spe.split(",") if s.strip() and s.strip() != "-"]

    return Weapon(
        name=name,
        count=count,
        range_inches=range_inches,
        attacks=attacks,
        ap=ap,
        special_rules=special_rules
    )


def parse_inline_weapons(equipment_str: str) -> List[Weapon]:
    """Parse weapons from inline equipment string like '2x Heavy Claws (A3, AP(1))'.

    Only returns weapons if the profile contains an attack value (A#).
    Rule upgrades like 'Bio-Tech Master (Increased Shooting Range)' are not weapons.
    """
    weapons: List[Weapon] = []

    # Find weapon patterns with balanced parentheses
    # Look for: optional "2x", name, then balanced parentheses with profile
    i = 0
    while i < len(equipment_str):
        # Look for start of a potential weapon (optional count + name + open paren)
        match = re.match(r'(\d+x\s+)?([^(,]+?)\s*\(', equipment_str[i:])
        if not match:
            i += 1
            continue

        count_str = match.group(1)
        name = match.group(2).strip()
        paren_start = i + match.end() - 1  # Position of '('

        # Find matching closing parenthesis (handle nested parens)
        depth = 1
        j = paren_start + 1
        while j < len(equipment_str) and depth > 0:
            if equipment_str[j] == '(':
                depth += 1
            elif equipment_str[j] == ')':
                depth -= 1
            j += 1

        if depth == 0:
            profile = equipment_str[paren_start + 1:j - 1].strip()

            # Only parse as weapon if profile contains an attack value (A#)
            if re.search(r'\bA\d+\b', profile):
                count = 1
                if count_str:
                    count = int(count_str.strip().rstrip("x"))

                range_inches, attacks, ap, special_rules = parse_weapon_profile(profile)

                weapons.append(Weapon(
                    name=name,
                    count=count,
                    range_inches=range_inches,
                    attacks=attacks,
                    ap=ap,
                    special_rules=special_rules
                ))

            i = j
        else:
            i += 1

    return weapons


def parse_special_rules(rules_str: str) -> List[str]:
    """Parse special rules from a comma-separated string."""
    if not rules_str or rules_str == "-":
        return []

    rules: List[str] = []
    # Handle nested parentheses in rules like "Tough(12)"
    depth = 0
    current = ""

    for char in rules_str:
        if char == "(":
            depth += 1
            current += char
        elif char == ")":
            depth -= 1
            current += char
        elif char == "," and depth == 0:
            if current.strip():
                rules.append(current.strip())
            current = ""
        else:
            current += char

    if current.strip():
        rules.append(current.strip())

    return rules


def is_upgrade_header(line: str) -> bool:
    """Check if a line is an upgrade group header."""
    upgrade_patterns = [
        r"^Upgrade\s+",
        r"^Replace\s+",
        r"^Any\s+model\s+may",
        r"^Upgrade\s+all\s+models",
        r"^Upgrade\s+one\s+model",
        r"^Upgrade\s+with",
    ]
    for pattern in upgrade_patterns:
        if re.match(pattern, line, re.IGNORECASE):
            return True
    return False


def parse_upgrade_option(line: str) -> Optional[UpgradeOption]:
    """Parse an upgrade option line like 'Bio-Recovery (Regeneration) +40pts'."""
    match = COST_RE.match(line)
    if not match:
        return None

    text = match.group("text").strip()
    cost_str = match.group("cost")

    cost = 0
    if cost_str != "Free":
        cost = int(cost_str.lstrip("+").rstrip("pts"))

    # Extract rules granted from parentheses
    rules_granted: List[str] = []
    rules_match = re.search(r"\(([^)]+)\)", text)
    if rules_match:
        rules_str = rules_match.group(1)
        # Check if it's a weapon profile (has A# pattern)
        if not re.search(r"\bA\d+\b", rules_str):
            rules_granted = parse_special_rules(rules_str)

    # Check if option includes a weapon
    weapon: Optional[Weapon] = None
    weapons = parse_inline_weapons(text)
    if weapons:
        weapon = weapons[0]

    return UpgradeOption(
        text=text,
        cost=cost,
        rules_granted=rules_granted,
        weapon=weapon
    )


# =============================================================================
# Main Unit Parser
# =============================================================================

def is_weapon_table_header(line: str) -> bool:
    """Check if line is the weapon table header or column name."""
    return line in ("Weapon RNG ATK AP SPE", "Weapon", "RNG", "ATK", "AP", "SPE")


def is_stat_value(line: str) -> bool:
    """Check if line is a stat-like value (range, attacks, AP, etc)."""
    # Range: "18"", "-"
    if line == "-":
        return True
    if re.match(r'^\d+"?$', line):
        return True
    # Attacks: "A4"
    if re.match(r'^A\d+$', line):
        return True
    # AP value: single digit
    if re.match(r'^\d$', line):
        return True
    return False


def parse_weapon_from_multiline(lines: List[str], start_idx: int) -> Tuple[Optional[Weapon], int]:
    """
    Parse a weapon from multiple lines (PyMuPDF format).
    Returns (weapon, next_index) or (None, start_idx) if not a weapon.
    """
    if start_idx >= len(lines):
        return None, start_idx

    # First line should be weapon name (possibly with count)
    name_line = lines[start_idx]

    # Skip if it's a header or upgrade section
    if is_upgrade_header(name_line) or UNIT_HEADER_RE.match(name_line):
        return None, start_idx
    if is_weapon_table_header(name_line):
        return None, start_idx + 1

    # Parse weapon count and name
    count = 1
    name = name_line
    count_match = re.match(r'^(\d+)x\s+(.+)$', name_line)
    if count_match:
        count = int(count_match.group(1))
        name = count_match.group(2)

    # Look ahead to gather weapon stats on subsequent lines
    idx = start_idx + 1
    range_inches: Optional[int] = None
    attacks: int = 1
    ap: Optional[int] = None
    special_rules: List[str] = []

    # Collect up to 4 more values: RNG, ATK, AP, SPE
    collected = 0
    max_collect = 4

    while idx < len(lines) and collected < max_collect:
        val = lines[idx].strip()

        # Stop if we hit another unit or upgrade header
        if UNIT_HEADER_RE.match(val) or is_upgrade_header(val):
            break

        # Skip weapon table column headers
        if val in ("Weapon", "RNG", "ATK", "AP", "SPE", "Weapon RNG ATK AP SPE"):
            idx += 1
            continue

        # Range value: "18"" or "-"
        if collected == 0:
            if val == "-":
                range_inches = None
                collected += 1
                idx += 1
                continue
            elif val.endswith('"'):
                try:
                    range_inches = int(val.rstrip('"'))
                    collected += 1
                    idx += 1
                    continue
                except ValueError:
                    pass
            # Not a valid range - not a weapon line
            break

        # Attacks value: "A4"
        if collected == 1:
            if val.startswith("A") and val[1:].isdigit():
                attacks = int(val[1:])
            else:
                break
            collected += 1
            idx += 1
            continue

        # AP value: "1" or "-"
        if collected == 2:
            if val == "-":
                ap = None
            elif val.isdigit():
                ap = int(val)
            else:
                break
            collected += 1
            idx += 1
            continue

        # Special rules: "Rending" or "Blast(3), Rending" or "-"
        if collected == 3:
            if val != "-":
                special_rules = parse_special_rules(val)
            collected += 1
            idx += 1
            break

    # Only return a weapon if we collected at least RNG and ATK
    if collected >= 2:
        return Weapon(
            name=name,
            count=count,
            range_inches=range_inches,
            attacks=attacks,
            ap=ap,
            special_rules=special_rules
        ), idx

    return None, start_idx


def parse_upgrade_from_multiline(lines: List[str], start_idx: int) -> Tuple[Optional[UpgradeOption], int]:
    """
    Parse an upgrade option from multiple lines.
    Format: "Option Text (Rules)" followed by "+20pts" or "Free" on next line.
    """
    if start_idx >= len(lines):
        return None, start_idx

    text_line = lines[start_idx]

    # Skip headers
    if UNIT_HEADER_RE.match(text_line) or is_upgrade_header(text_line):
        return None, start_idx
    if is_weapon_table_header(text_line):
        return None, start_idx + 1

    # Look for cost on next line
    idx = start_idx + 1
    if idx < len(lines):
        cost_line = lines[idx]
        cost_match = re.match(r'^(\+\d+pts|Free)$', cost_line)
        if cost_match:
            cost_str = cost_match.group(1)
            cost = 0 if cost_str == "Free" else int(cost_str.lstrip("+").rstrip("pts"))

            # Extract rules from parentheses
            rules_granted: List[str] = []
            rules_match = re.search(r"\(([^)]+)\)", text_line)
            if rules_match:
                rules_str = rules_match.group(1)
                # Check if it's a weapon profile (has A# pattern)
                if not re.search(r'\bA\d+\b', rules_str):
                    rules_granted = parse_special_rules(rules_str)

            # Check if option includes a weapon
            weapon: Optional[Weapon] = None
            weapons = parse_inline_weapons(text_line)
            if weapons:
                weapon = weapons[0]

            return UpgradeOption(
                text=text_line,
                cost=cost,
                rules_granted=rules_granted,
                weapon=weapon
            ), idx + 1

    return None, start_idx


def parse_units_from_lines(lines: List[str]) -> List[Unit]:
    """Parse all units from extracted PDF lines."""
    units: List[Unit] = []
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]

        # Try to match unit header format: "Unit Name [1] - 345pts"
        header_match = UNIT_HEADER_RE.match(line)
        if not header_match:
            i += 1
            continue

        # Extract basic unit info
        unit_name = header_match.group("name").strip()
        unit_size = int(header_match.group("size"))
        base_points = int(header_match.group("pts"))

        i += 1

        # Parse stats - can be combined or split across lines
        quality = 4
        defense = 4
        tough: Optional[int] = None

        # Try combined format first
        if i < n:
            stats_match = STATS_RE.match(lines[i])
            if stats_match:
                quality = int(stats_match.group("qua"))
                defense = int(stats_match.group("def"))
                if stats_match.group("tough"):
                    tough = int(stats_match.group("tough"))
                i += 1
            else:
                # Try split format: "Quality 3+", "Defense 2+", "Tough 12"
                if i < n:
                    q_match = QUALITY_RE.match(lines[i])
                    if q_match:
                        quality = int(q_match.group("qua"))
                        i += 1

                if i < n:
                    d_match = DEFENSE_RE.match(lines[i])
                    if d_match:
                        defense = int(d_match.group("def"))
                        i += 1

                if i < n:
                    t_match = TOUGH_RE.match(lines[i])
                    if t_match:
                        tough = int(t_match.group("tough"))
                        i += 1

        # Parse special rules (lines before weapon table)
        special_rules: List[str] = []

        while i < n:
            current_line = lines[i]
            if is_weapon_table_header(current_line):
                break
            if UNIT_HEADER_RE.match(current_line):
                break
            if is_upgrade_header(current_line):
                break

            # This line contains special rules
            rules = parse_special_rules(current_line)
            if rules:
                special_rules.extend(rules)
            i += 1

        # Skip weapon table header lines
        while i < n and is_weapon_table_header(lines[i]):
            i += 1

        # Parse weapons (multi-line format)
        weapons: List[Weapon] = []
        while i < n:
            current_line = lines[i]
            if UNIT_HEADER_RE.match(current_line):
                break
            if is_upgrade_header(current_line):
                break

            weapon, next_idx = parse_weapon_from_multiline(lines, i)
            if weapon:
                weapons.append(weapon)
                i = next_idx
            else:
                break

        # Parse upgrade groups
        upgrade_groups: List[UpgradeGroup] = []

        while i < n:
            current_line = lines[i]

            if UNIT_HEADER_RE.match(current_line):
                break

            if is_upgrade_header(current_line):
                group = UpgradeGroup(header=current_line)
                i += 1

                # Parse options in this group
                while i < n:
                    if UNIT_HEADER_RE.match(lines[i]):
                        break
                    if is_upgrade_header(lines[i]):
                        break
                    if is_weapon_table_header(lines[i]):
                        i += 1
                        continue

                    option, next_idx = parse_upgrade_from_multiline(lines, i)
                    if option:
                        group.options.append(option)
                        i = next_idx
                    else:
                        # Try to skip unrecognized lines
                        i += 1
                        # But stop if next line looks like an option cost
                        if i < n and not re.match(r'^(\+\d+pts|Free)$', lines[i]):
                            continue
                        break

                if group.options:
                    upgrade_groups.append(group)
            else:
                i += 1

        # Create unit
        unit = Unit(
            name=unit_name,
            size=unit_size,
            quality=quality,
            defense=defense,
            base_points=base_points,
            tough=tough,
            special_rules=special_rules,
            weapons=weapons,
            upgrade_groups=upgrade_groups
        )

        units.append(unit)

    return units


# =============================================================================
# Output Functions
# =============================================================================

def unit_to_dict(unit: Unit) -> Dict[str, Any]:
    """Convert a Unit to a JSON-serializable dictionary."""
    return {
        "name": unit.name,
        "size": unit.size,
        "quality": unit.quality,
        "defense": unit.defense,
        "base_points": unit.base_points,
        "tough": unit.tough,
        "special_rules": unit.special_rules,
        "weapons": [
            {
                "name": w.name,
                "count": w.count,
                "range": w.range_inches,
                "attacks": w.attacks,
                "ap": w.ap,
                "special_rules": w.special_rules
            }
            for w in unit.weapons
        ],
        "upgrade_groups": [
            {
                "header": g.header,
                "options": [
                    {
                        "text": o.text,
                        "cost": o.cost,
                        "rules_granted": o.rules_granted,
                        "weapon": {
                            "name": o.weapon.name,
                            "count": o.weapon.count,
                            "range": o.weapon.range_inches,
                            "attacks": o.weapon.attacks,
                            "ap": o.weapon.ap,
                            "special_rules": o.weapon.special_rules
                        } if o.weapon else None
                    }
                    for o in g.options
                ]
            }
            for g in unit.upgrade_groups
        ]
    }


def write_json_output(units: List[Unit], output_path: Path) -> None:
    """Write units to JSON file."""
    data = {
        "units": [unit_to_dict(u) for u in units],
        "total_units": len(units)
    }

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

    print(f"Wrote {len(units)} units to {output_path}")


def write_txt_output(units: List[Unit], output_path: Path) -> None:
    """Write units to TXT file in loadout format."""
    with open(output_path, "w", encoding="utf-8") as f:
        for i, unit in enumerate(units):
            if i > 0:
                f.write("\n")
            f.write(unit.to_loadout_string() + "\n")

    print(f"Wrote {len(units)} units to {output_path}")


def write_base_units_csv(units: List[Unit], output_path: Path) -> None:
    """Write units to CSV for easy spreadsheet viewing."""
    import csv

    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "Name", "Size", "Quality", "Defense", "Tough", "Points",
            "Special Rules", "Weapons", "Upgrade Groups"
        ])

        for unit in units:
            weapons_str = "; ".join(w.to_string() for w in unit.weapons)
            rules_str = ", ".join(unit.special_rules)
            upgrades_str = "; ".join(g.header for g in unit.upgrade_groups)

            writer.writerow([
                unit.name, unit.size, f"{unit.quality}+", f"{unit.defense}+",
                unit.tough or "-", unit.base_points,
                rules_str, weapons_str, upgrades_str
            ])

    print(f"Wrote {len(units)} units to {output_path}")


# =============================================================================
# Main Entry Point
# =============================================================================

def parse_pdf(pdf_path: str, output_dir: Optional[str] = None) -> List[Unit]:
    """
    Parse a PDF file and output structured unit data.

    Args:
        pdf_path: Path to the PDF file
        output_dir: Optional output directory (defaults to same as PDF)

    Returns:
        List of parsed Unit objects
    """
    pdf_path = Path(pdf_path)
    if not pdf_path.exists():
        raise FileNotFoundError(f"PDF not found: {pdf_path}")

    if output_dir:
        out_dir = Path(output_dir)
    else:
        out_dir = pdf_path.parent

    out_dir.mkdir(parents=True, exist_ok=True)

    # Extract base name for output files
    base_name = pdf_path.stem.replace(" ", "_")

    print(f"Parsing: {pdf_path}")

    # Extract text from PDF
    lines = extract_text_from_pdf(str(pdf_path))
    print(f"Extracted {len(lines)} lines from PDF")

    # Parse units
    units = parse_units_from_lines(lines)
    print(f"Parsed {len(units)} units")

    # Write outputs
    json_path = out_dir / f"{base_name}_units.json"
    txt_path = out_dir / f"{base_name}_base_loadouts.txt"
    csv_path = out_dir / f"{base_name}_units.csv"

    write_json_output(units, json_path)
    write_txt_output(units, txt_path)
    write_base_units_csv(units, csv_path)

    return units


def main():
    """CLI entry point."""
    parser = argparse.ArgumentParser(
        description="Parse OPR faction PDFs into structured unit data",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python parse_pdf_loadouts.py "docs/GF - Alien Hives 3.5.1.pdf"
    python parse_pdf_loadouts.py army.pdf --output-dir ./output

Output files:
    <name>_units.json       - Full structured data with all upgrades
    <name>_base_loadouts.txt - Base loadouts in simulation format
    <name>_units.csv        - Spreadsheet-friendly summary
        """
    )

    parser.add_argument("pdf_path", help="Path to the faction PDF file")
    parser.add_argument(
        "--output-dir", "-o",
        help="Output directory (defaults to PDF directory)"
    )

    args = parser.parse_args()

    try:
        units = parse_pdf(args.pdf_path, args.output_dir)
        print(f"\nSuccessfully parsed {len(units)} units!")

        # Print summary
        print("\nUnit Summary:")
        print("-" * 60)
        for unit in units[:10]:  # Show first 10
            upgrades = sum(len(g.options) for g in unit.upgrade_groups)
            print(f"  {unit.name} [{unit.size}] Q{unit.quality}+ D{unit.defense}+ "
                  f"- {unit.base_points}pts ({len(unit.weapons)} weapons, {upgrades} upgrades)")

        if len(units) > 10:
            print(f"  ... and {len(units) - 10} more units")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
