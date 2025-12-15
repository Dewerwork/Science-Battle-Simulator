from __future__ import annotations

import hashlib
import itertools
import json
import math
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

from concurrent.futures import ProcessPoolExecutor, as_completed

# =========================================================
# HARD-CODED SETTINGS (edit these in PyCharm)
# =========================================================
# PDF input: Can be a single PDF file OR a folder containing multiple PDFs
PDF_INPUT_PATH = r"C:\Users\David\Documents\Army Factions"
OUTPUT_DIR = r"C:\Users\David\Documents\Army Factions\pipeline_output"

# 0 = no limit (can explode!)
MAX_LOADOUTS_PER_UNIT = 0

# Within-unit parallelism (processes, not threads)
WORKERS_PER_UNIT = 32         # set 24 or 32 for your i9
TASKS_PER_UNIT = 256 # good load balancing for uneven work

# Write huge ungrouped loadouts? (usually NO)
WRITE_UNGROUPED_LOADOUTS_TXT = False

INCLUDE_POINTS_IN_STAGE1_SIGNATURE = True  # stage-2 ignores points anyway, but can help lineage/debug

# Stage-2 weapon condensation settings
RANGE_BUCKETS = [6, 12, 18, 24]     # anything above -> "32+"
RANGE_BUCKET_HIGH = "32+"

# Attack-agnostic grouping: If True, weapons with same range/AP/tags are grouped
# even if they have different attack counts. This dramatically reduces output size.
ATTACK_AGNOSTIC_GROUPING = True

# Rule-agnostic grouping: If True, units are grouped by weapon sets only,
# ignoring unit special rules. This provides maximum reduction while maintaining
# full traceability via the lineage chain in JSON output.
RULE_AGNOSTIC_GROUPING = True

# RAW LOADOUT MODE: If True, outputs each unique loadout combo separately
# with a UID in the unit name (no Stage-1/Stage-2 grouping). Format: "UnitName [UID:xxxx]"
RAW_LOADOUT_MODE = True

# TXT formatting
ADD_BLANK_LINE_BETWEEN_UNITS = True

# Merge settings: After processing all units, merge *.final.txt into one file
MERGE_FINAL_TXTS = True
STRIP_SG_LABELS = True  # Remove " - SG#### (...)" from header lines
ADD_BLANK_LINE_BETWEEN_FILES = True

# =========================================================
# Utilities
# =========================================================
def sha1(s: str) -> str:
    return hashlib.sha1(s.encode("utf-8")).hexdigest()

def safe_filename(name: str) -> str:
    name = name.strip()
    return re.sub(r"[^A-Za-z0-9._-]+", "_", name).strip("_") or "unit"

def ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)

def norm_ws(s: str) -> str:
    return " ".join(str(s).strip().split())

# =========================================================
# UID Generation for Raw Loadouts
# =========================================================
_UID_COUNTER: Dict[str, int] = {}  # Per-unit UID counters

def generate_uid(unit_name: str, combo_idx: int, signature: str) -> str:
    """
    Generate a unique ID for a loadout.
    Format: first 4 chars of sha1 of (unit_name + combo_idx + signature)
    """
    uid_input = f"{unit_name}|{combo_idx}|{signature}"
    return sha1(uid_input)[:8].upper()

def reset_uid_counter() -> None:
    """Reset UID counter (call at start of each unit)."""
    global _UID_COUNTER
    _UID_COUNTER = {}

# =========================================================
# 1) PDF -> units JSON
# =========================================================
UNIT_HEADER_RE = re.compile(r"^(?P<name>.+?)\s+\[(?P<size>\d+)\]\s*-\s*(?P<pts>\d+)pts$")
STATS_RE = re.compile(r"^Quality\s+(?P<qua>\d)\+\s+Defense\s+(?P<def>\d)\+(?:\s+Tough\s+(?P<tough>\d+))?$")
WEAPON_HEADER = "Weapon RNG ATK AP SPE"
WEAPON_RE = re.compile(
    r"^(?:(?P<count>\d+)x\s+)?"
    r"(?P<wname>.+?)\s+"
    r"(?P<rng>\d+\"\s*|-)\s+"
    r"(?P<atk>A\d+)\s+"
    r"(?P<ap>-|\d+)\s+"
    r"(?P<spe>.+)$"
)
COST_RE = re.compile(r"^(?P<text>.+?)\s+(?P<cost>(?:\+\d+pts|Free))$")

def _is_group_header(line: str) -> bool:
    return bool(re.match(r"^(Upgrade|Replace|Any model)", line))

def _parse_attacks(atk: str) -> int:
    return int(atk.lstrip("A"))

def _parse_ap(ap: str) -> Optional[int]:
    if ap == "-":
        return None
    return int(ap)

def _split_rules(text: str) -> List[str]:
    text = text.replace(" ,", ",").strip().strip(",")
    return [p.strip() for p in text.split(",") if p.strip()]

def extract_lines(pdf_path: str) -> List[str]:
    try:
        import pdfplumber  # type: ignore
    except Exception as e:
        raise RuntimeError("Missing dependency pdfplumber. Install with: pip install pdfplumber") from e

    lines: List[str] = []
    with pdfplumber.open(pdf_path) as pdf:
        for page in pdf.pages:
            text = page.extract_text(x_tolerance=1.5, y_tolerance=2, use_text_flow=True) or ""
            for raw in text.splitlines():
                line = raw.strip()
                if not line:
                    continue
                if line.startswith("GF - "):
                    continue
                if re.fullmatch(r"V\d+(\.\d+)*", line):
                    continue
                if re.fullmatch(r"\d+", line):
                    continue
                if re.fullmatch(r"\.{2,}", line):
                    continue
                lines.append(line)
    return lines

def parse_units(lines: List[str]) -> List[Dict[str, Any]]:
    units: List[Dict[str, Any]] = []
    i = 0
    n = len(lines)

    while i < n:
        m = UNIT_HEADER_RE.match(lines[i])
        if not m:
            i += 1
            continue

        unit: Dict[str, Any] = {
            "name": m.group("name").strip(),
            "size": int(m.group("size")),
            "base_points": int(m.group("pts")),
            "quality": None,
            "defense": None,
            "tough": None,
            "special_rules": [],
            "weapons": [],
            "options": [],
        }
        i += 1

        if i < n:
            ms = STATS_RE.match(lines[i])
            if ms:
                unit["quality"] = int(ms.group("qua"))
                unit["defense"] = int(ms.group("def"))
                if ms.group("tough"):
                    unit["tough"] = int(ms.group("tough"))
                i += 1

        rules_lines: List[str] = []
        while i < n and lines[i] != WEAPON_HEADER and not UNIT_HEADER_RE.match(lines[i]):
            rules_lines.append(lines[i])
            i += 1
        if rules_lines:
            unit["special_rules"] = _split_rules(" ".join(rules_lines))

        if i < n and lines[i] == WEAPON_HEADER:
            i += 1

        while i < n:
            line = lines[i]
            if UNIT_HEADER_RE.match(line) or _is_group_header(line) or line == WEAPON_HEADER:
                break
            wm = WEAPON_RE.match(line)
            if wm:
                spe = wm.group("spe").strip()
                weapon = {
                    "count": int(wm.group("count") or 1),
                    "name": wm.group("wname").strip(),
                    "range": wm.group("rng").replace(" ", ""),
                    "attacks": _parse_attacks(wm.group("atk")),
                    "ap": _parse_ap(wm.group("ap")),
                    "special": [] if spe == "-" else [s.strip() for s in spe.split(",") if s.strip() and s.strip() != "-"],
                }
                unit["weapons"].append(weapon)
            i += 1

        while i < n and not UNIT_HEADER_RE.match(lines[i]):
            if lines[i] == WEAPON_HEADER:
                i += 1
                continue

            if _is_group_header(lines[i]):
                header = lines[i]
                group = {"header": header, "options": []}
                i += 1

                while i < n and not UNIT_HEADER_RE.match(lines[i]) and not _is_group_header(lines[i]) and lines[i] != WEAPON_HEADER:
                    opt_line = lines[i].strip()
                    if not opt_line:
                        i += 1
                        continue
                    cm = COST_RE.match(opt_line)
                    if cm:
                        text_part = cm.group("text").strip()
                        cost_part = cm.group("cost")
                        pts = 0 if cost_part == "Free" else int(cost_part.lstrip("+").rstrip("pts"))
                        group["options"].append({"text": text_part, "pts": pts})
                    else:
                        group["options"].append({"text": opt_line, "pts": None})
                    i += 1

                unit["options"].append(group)
            else:
                i += 1

        units.append(unit)

    return units

# Patterns for filtering invalid/artifact rules
_COST_MODIFIER_RE = re.compile(r"^\+\d+pts$", re.IGNORECASE)
_TRAILING_PAREN_RE = re.compile(r"^(.+)\)$")  # Rules ending with unmatched )
_WEAPON_PROFILE_IN_RULE_RE = re.compile(r'\(\s*\d+"\s*,\s*A\d+')  # Weapon profiles like (24", A1, ...)
_TRUNCATED_RULE_FIXES = {
    "casting debu": "Casting(1)",  # Common truncation fix
    "casting debug": "Casting(1)",
}

def _clean_rule(rule: str) -> Optional[str]:
    """
    Clean up a rule string, returning None if it should be filtered out.
    Fixes common parsing artifacts.
    """
    r = norm_ws(rule)
    if not r:
        return None

    # Filter out cost modifiers (+20pts, +55pts, etc.)
    if _COST_MODIFIER_RE.match(r):
        return None

    # Filter out rules that contain weapon profiles (parsing artifacts)
    if _WEAPON_PROFILE_IN_RULE_RE.search(r):
        return None

    # Filter out standalone "(A1)" type artifacts
    if re.match(r"^\(A\d+\)$", r):
        return None

    # Fix truncated rules
    r_lower = r.lower()
    for truncated, fixed in _TRUNCATED_RULE_FIXES.items():
        if r_lower.startswith(truncated):
            r = fixed + r[len(truncated):]
            break

    # Fix trailing unmatched parenthesis
    if r.count(")") > r.count("("):
        r = r.rstrip(")")

    # Fix leading unmatched parenthesis
    if r.count("(") > r.count(")"):
        r = r.lstrip("(")

    r = r.strip()
    return r if r else None

def normalize_rules_for_signature(rules_in: List[str]) -> Tuple[str, ...]:
    """Clean and normalize rules for signature generation (no exclusion).

    Rules are deduplicated case-insensitively - e.g., "Fearless", "fearless", "FEARLESS"
    all collapse to a single entry, preserving the first encountered form.
    """
    out: List[str] = []
    seen_lower: Dict[str, str] = {}  # lowercase -> first encountered form

    for r in rules_in:
        cleaned = _clean_rule(r)
        if cleaned:
            key = cleaned.lower()
            if key not in seen_lower:
                seen_lower[key] = cleaned
                out.append(cleaned)

    out = sorted(out, key=lambda x: x.lower())
    return tuple(out)

# =========================================================
# Stage-1 signature helpers (no text parsing)
# =========================================================
_ATTACKS_RE = re.compile(r"^A(?P<a>\d+)$", re.IGNORECASE)
_AP_RE = re.compile(r"^AP\((?P<ap>-?\d+)\)$", re.IGNORECASE)
_RNG_QUOTE_RE = re.compile(r'^(?P<r>\d+)"$')

def _split_top_level_commas(s: str) -> List[str]:
    out: List[str] = []
    buf: List[str] = []
    depth = 0
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth = max(0, depth - 1)
        if ch == "," and depth == 0:
            piece = "".join(buf).strip()
            if piece:
                out.append(piece)
            buf = []
        else:
            buf.append(ch)
    tail = "".join(buf).strip()
    if tail:
        out.append(tail)
    return out

def looks_like_weapon_profile(inside_parens: Optional[str]) -> bool:
    if not inside_parens:
        return False
    if re.search(r"\bA\d+\b", inside_parens):
        return True
    if re.search(r"\bAP\(\s*-?\d+\s*\)", inside_parens):
        return True
    return False

def split_name_and_parens(text: str) -> Tuple[str, Optional[str]]:
    t = text.strip()
    m = re.match(r"^(?P<name>.+?)\s*\((?P<inside>.+)\)\s*$", t)
    if m:
        return m.group("name").strip(), m.group("inside").strip()
    return t, None

def parse_count_prefix(text: str) -> Tuple[int, str]:
    t = text.strip()
    m = re.match(r"^(?P<n>\d+)\s*[x×]\s*(?P<rest>.+)$", t, flags=re.I)
    if m:
        return int(m.group("n")), m.group("rest").strip()
    return 1, t

def weapon_key_from_profile(profile: str, weapon_name: str, rng_fallback: Optional[int] = None) -> Tuple[str, Optional[int], int, Optional[int], Tuple[str, ...]]:
    """
    Parse profile like: 18", A4, AP(1), Rending
    Returns:
      key_str like: N=Sword|R=18|A=4|AP=1|T=Rending;Reliable

    Note: weapon_name is normalized (lowercased, whitespace-collapsed) for consistent
    key generation across different sources (base weapons vs option text).
    """
    tokens = [t.strip() for t in _split_top_level_commas(profile)]
    rng: Optional[int] = rng_fallback
    attacks = 0
    ap: Optional[int] = None
    tags: List[str] = []

    for t in tokens:
        if not t:
            continue
        mq = _RNG_QUOTE_RE.match(t)
        if mq:
            rng = int(mq.group("r"))
            continue
        ma = _ATTACKS_RE.match(t)
        if ma:
            attacks = int(ma.group("a"))
            continue
        map_ = _AP_RE.match(t)
        if map_:
            ap = int(map_.group("ap"))
            continue
        tags.append(t)

    tags_sorted = tuple(sorted([x for x in tags if x], key=lambda x: x.lower()))
    # Normalize weapon name for consistent key generation across different sources
    # This ensures "Heavy Sword" and "Heavy sword" generate the same key
    normalized_name = norm_ws(weapon_name).lower()
    key = f"N={normalized_name}|R={'' if rng is None else rng}|A={attacks}|AP={'' if ap is None else ap}"
    if tags_sorted:
        key += "|T=" + ";".join(tags_sorted)
    return key, rng, attacks, ap, tags_sorted

def build_stage1_signature(points: int, rules: Tuple[str, ...], weapon_multiset: Dict[str, int]) -> str:
    items = [(k, c) for k, c in weapon_multiset.items() if c > 0]
    items.sort(key=lambda kv: kv[0])
    weapons_key = "|".join([f"{k}*{c}" for k, c in items])

    parts: List[str] = []
    if INCLUDE_POINTS_IN_STAGE1_SIGNATURE:
        parts.append(f"PTS={points}")
    parts.append(f"RULES={','.join(rules)}")
    parts.append(f"W={weapons_key}")
    return "||".join(parts)

# =========================================================
# Variant generation with precomputed deltas
# =========================================================
_WORD_TO_INT = {"one": 1, "two": 2, "three": 3, "four": 4, "five": 5,
                "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10}

def norm_name(s: str) -> str:
    s = s.strip().lower()
    s = re.sub(r"\s+", " ", s)
    if s.endswith("s") and not s.endswith("ss"):
        s2 = s[:-1]
        if len(s2) > 2:
            s = s2
    return s

def weapon_occurrences(unit: Dict[str, Any], weapon_name: str) -> int:
    wn = norm_name(weapon_name)
    occ = 0
    for w in unit.get("weapons", []):
        if norm_name(w.get("name", "")) == wn:
            occ += int(w.get("count", 1) or 1)
    return occ

def guess_upgrade_multiplier(header: str, unit_size: int) -> int:
    h = header.lower()
    if "all models" in h:
        return unit_size
    return 1

def header_pick_limit(header: str) -> Optional[int]:
    h = header.lower()
    m = re.search(r"\b(up to|upto)\s+(?P<n>\d+)\b", h)
    if m:
        return int(m.group("n"))
    m = re.search(r"\b(up to|upto)\s+(?P<w>one|two|three|four|five|six|seven|eight|nine|ten)\b", h)
    if m:
        return _WORD_TO_INT[m.group("w")]
    m = re.search(r"\bwith\s+(?P<n>\d+)\b", h)
    if m:
        return int(m.group("n"))
    m = re.search(r"\bwith\s+(?P<w>one|two|three|four|five|six|seven|eight|nine|ten)\b", h)
    if m:
        return _WORD_TO_INT[m.group("w")]
    return None

TREAT_UPGRADE_WITH_NO_QUANTITY_AS_ANY_SUBSET = True
ALLOW_EMPTY_FOR_ONE_SELECTION_GROUPS = True
ALLOW_MIXED_REPLACEMENTS_FOR_UP_TO = True

@dataclass(frozen=True)
class Variant:
    pts_delta: int
    add_rules: Tuple[str, ...]                 # raw rules (un-normalized)
    weapon_delta: Tuple[Tuple[str, int], ...]  # key->delta count (now includes weapon name)

def _extract_rules_from_choice(choice_text: str) -> List[str]:
    name_part, inside = split_name_and_parens(choice_text)
    if inside is not None:
        # If parentheses do NOT look like weapon profile, treat as rule payload.
        if not looks_like_weapon_profile(inside):
            # Split by comma (respecting nested parentheses) to handle multi-rule options
            # e.g., "Fear(2), Relentless" -> ["Fear(2)", "Relentless"]
            rules = _split_top_level_commas(inside)
            return [r.strip() for r in rules if r.strip()]
        return []
    # No parentheses: treat whole thing as rule-ish
    t = name_part.strip()
    return [t] if t else []

def _compute_option_weapon_key(opt: Dict[str, Any]) -> Tuple[str, int, int, List[str]]:
    """
    Compute the weapon key for an option to enable deduplication.
    Returns: (weapon_key, pts, count, rules)
    """
    pts = int(opt.get("pts", 0) or 0)
    txt = str(opt.get("text", "")).strip()
    name_part, inside = split_name_and_parens(txt)
    c, item_name = parse_count_prefix(name_part)

    if inside and looks_like_weapon_profile(inside):
        add_key = weapon_key_from_profile(inside, item_name)[0]
    else:
        # Normalize weapon name for consistent key generation
        normalized_name = norm_ws(item_name).lower()
        add_key = f"N={normalized_name}|R=|A=0|AP="

    rules = _extract_rules_from_choice(txt)
    return (add_key, pts, c, rules)

def _dedupe_options_by_weapon_key(opts: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """
    Deduplicate options that produce the same weapon key.
    Keeps the first occurrence (lowest pts if tied).
    """
    seen: Dict[str, Dict[str, Any]] = {}
    for o in opts:
        key, pts, count, rules = _compute_option_weapon_key(o)
        # Create a composite key including pts and rules for full deduplication
        composite_key = (key, pts, count, tuple(rules))
        if composite_key not in seen:
            seen[composite_key] = o
    return list(seen.values())

def build_base_weapon_multiset(unit: Dict[str, Any]) -> Tuple[Dict[str, int], Dict[str, str]]:
    """
    Returns:
      base_multiset: weapon_key -> count (now includes weapon name in key)
      name_to_key: normalized weapon name -> weapon_key (first match)

    Note: Weapon names in keys are normalized (lowercased) for consistent matching
    across different sources (base weapons vs option text).
    """
    base: Dict[str, int] = {}
    name_to_key: Dict[str, str] = {}

    for w in unit.get("weapons", []) or []:
        name = str(w.get("name", "")).strip()
        count = int(w.get("count", 1) or 1)
        rng_raw = str(w.get("range", "")).strip()
        rng: Optional[int] = None
        if rng_raw and rng_raw not in ("-", "—"):
            m = _RNG_QUOTE_RE.match(rng_raw)
            if m:
                rng = int(m.group("r"))
        attacks = int(w.get("attacks", 0) or 0)
        ap = w.get("ap", None)
        tags = tuple(sorted([str(s).strip() for s in (w.get("special") or []) if str(s).strip() and str(s).strip() != "-"], key=lambda x: x.lower()))

        # Normalize weapon name for consistent key generation (same as weapon_key_from_profile)
        normalized_name = norm_ws(name).lower()
        key = f"N={normalized_name}|R={'' if rng is None else rng}|A={attacks}|AP={'' if ap is None else ap}"
        if tags:
            key += "|T=" + ";".join(tags)

        base[key] = base.get(key, 0) + count
        nn = norm_name(name)
        name_to_key.setdefault(nn, key)

    return base, name_to_key

def group_variants(unit: Dict[str, Any], group: Dict[str, Any], name_to_key: Dict[str, str]) -> List[Variant]:
    header = group.get("header", "").strip()
    opts = group.get("options", []) or []
    unit_size = int(unit.get("size", 1) or 1)

    h = header.lower()
    is_upgrade = h.startswith("upgrade")
    is_replace = h.startswith("replace") or h.startswith("any model may replace") or " replace " in h
    multiplier = guess_upgrade_multiplier(header, unit_size)

    out: List[Variant] = []

    def make(pts: int, add_rules: List[str], weapon_delta: Dict[str, int]) -> Variant:
        # compress
        wd = tuple(sorted([(k, v) for k, v in weapon_delta.items() if v != 0], key=lambda kv: kv[0]))
        ar = tuple([r for r in (x.strip() for x in add_rules) if r])
        return Variant(pts_delta=pts, add_rules=ar, weapon_delta=wd)

    # UPGRADE groups (rules-only in current model)
    if is_upgrade and not h.startswith("replace"):
        max_pick = header_pick_limit(header)
        if max_pick is None:
            if TREAT_UPGRADE_WITH_NO_QUANTITY_AS_ANY_SUBSET:
                indices = range(len(opts))
                for r in range(0, len(opts) + 1):
                    for combo in itertools.combinations(indices, r):
                        add_rules: List[str] = []
                        pts = 0
                        for i in combo:
                            o = opts[i]
                            pts_each = int(o.get("pts", 0) or 0)
                            pts += pts_each * multiplier
                            add_rules.extend(_extract_rules_from_choice(str(o.get("text", ""))))
                        out.append(make(pts, add_rules, {}))
                return out
            else:
                max_pick = 1

        indices = range(len(opts))
        start_r = 0 if ALLOW_EMPTY_FOR_ONE_SELECTION_GROUPS else (1 if max_pick == 1 else 0)
        for r in range(start_r, min(max_pick, len(opts)) + 1):
            for combo in itertools.combinations(indices, r):
                add_rules: List[str] = []
                pts = 0
                for i in combo:
                    o = opts[i]
                    pts_each = int(o.get("pts", 0) or 0)
                    pts += pts_each * multiplier
                    add_rules.extend(_extract_rules_from_choice(str(o.get("text", ""))))
                out.append(make(pts, add_rules, {}))
        return out

    # REPLACE groups
    if is_replace:
        mode = "bundle"
        slots = 1
        target_name = header

        m = re.match(r"^any model may replace\s+(?P<rest>.+)$", h)
        if m:
            mode = "per_slot"
            slots = unit_size
            target_name = m.group("rest").strip()

        m = re.match(r"^replace all\s+(?P<weapon>.+)$", h)
        if m:
            mode = "bundle_all"
            target_name = m.group("weapon").strip()
            slots = max(1, weapon_occurrences(unit, target_name))

        m = re.match(r"^replace any\s+(?P<weapon>.+)$", h)
        if m:
            mode = "per_slot"
            target_name = m.group("weapon").strip()
            slots = max(1, weapon_occurrences(unit, target_name))

        m = re.match(r"^replace one\s+(?P<weapon>.+)$", h)
        if m:
            mode = "bundle"
            target_name = m.group("weapon").strip()
            slots = 1

        m = re.match(r"^replace\s+(?:up to|upto)\s+(?P<n>\d+|one|two|three|four|five)\s+(?P<weapon>.+)$", h)
        if m:
            nraw = m.group("n")
            n = int(nraw) if nraw.isdigit() else _WORD_TO_INT.get(nraw, 2)
            target_name = m.group("weapon").strip()
            if ALLOW_MIXED_REPLACEMENTS_FOR_UP_TO:
                mode = "per_slot"
                slots = n
            else:
                mode = "bundle"
                slots = n

        m = re.match(r"^replace\s+(?P<n>\d+)\s*[x×]\s+(?P<weapon>.+)$", h)
        if m:
            mode = "bundle"
            slots = int(m.group("n"))
            target_name = m.group("weapon").strip()

        m = re.match(r"^replace\s+(?P<weapon>.+)$", h)
        if m and target_name == header:
            target_name = m.group("weapon").strip()
            occ = weapon_occurrences(unit, target_name)
            slots = max(1, occ)
            mode = "bundle"

        target_key = name_to_key.get(norm_name(target_name), "")
        def none_var() -> Variant:
            return make(0, [], {})

        if not opts:
            return [none_var()]

        # OPTIMIZATION 2: Deduplicate options that produce the same weapon key
        deduped_opts = _dedupe_options_by_weapon_key(opts)

        if mode in ("bundle", "bundle_all"):
            out.append(none_var())
            for o in deduped_opts:
                pts = int(o.get("pts", 0) or 0)
                txt = str(o.get("text", "")).strip()
                name_part, inside = split_name_and_parens(txt)
                c, item_name = parse_count_prefix(name_part)

                weapon_delta: Dict[str, int] = {}
                if target_key:
                    weapon_delta[target_key] = weapon_delta.get(target_key, 0) - int(slots)

                # Use the actual weapon name from the option text instead of a placeholder
                add_key = None
                if inside and looks_like_weapon_profile(inside):
                    add_key = weapon_key_from_profile(inside, item_name)[0]
                else:
                    # For melee weapons without explicit profile, use the weapon name
                    # Normalize name for consistent key generation
                    normalized_name = norm_ws(item_name).lower()
                    add_key = f"N={normalized_name}|R=|A=0|AP="

                # OPTIMIZATION 3: Skip self-replacements (replacing weapon with itself)
                if add_key == target_key:
                    continue  # No actual change - skip this variant

                weapon_delta[add_key] = weapon_delta.get(add_key, 0) + int(c)

                # rules added (if any non-weapon payload)
                add_rules = _extract_rules_from_choice(txt)

                out.append(make(pts, add_rules, weapon_delta))
            return out

        if mode == "per_slot":
            # OPTIMIZATION 1: Use combinations_with_replacement instead of product
            # This reduces combinations since order doesn't matter
            # e.g., (plasma, melta, plasma) is same as (plasma, plasma, melta)
            choices = [None] + deduped_opts
            out.append(none_var())

            # Use combinations_with_replacement for symmetry reduction
            # This generates unordered selections with repetition
            for pick_combo in itertools.combinations_with_replacement(choices, slots):
                pts_delta = 0
                weapon_delta: Dict[str, int] = {}
                add_rules: List[str] = []
                has_self_replacement = False

                for pick in pick_combo:
                    if pick is None:
                        continue
                    pts_delta += int(pick.get("pts", 0) or 0)
                    txt = str(pick.get("text", "")).strip()
                    name_part, inside = split_name_and_parens(txt)
                    c, item_name = parse_count_prefix(name_part)

                    if target_key:
                        weapon_delta[target_key] = weapon_delta.get(target_key, 0) - 1

                    # Use the actual weapon name instead of a placeholder
                    if inside and looks_like_weapon_profile(inside):
                        add_key = weapon_key_from_profile(inside, item_name)[0]
                    else:
                        # For melee weapons without explicit profile, use the weapon name
                        # Normalize name for consistent key generation
                        normalized_name = norm_ws(item_name).lower()
                        add_key = f"N={normalized_name}|R=|A=0|AP="

                    # OPTIMIZATION 3: Track if this is a self-replacement
                    if add_key == target_key:
                        has_self_replacement = True

                    weapon_delta[add_key] = weapon_delta.get(add_key, 0) + int(c)
                    add_rules.extend(_extract_rules_from_choice(txt))

                # Skip variants that only contain self-replacements (no net change)
                # Check if weapon_delta results in no actual change
                net_change = {k: v for k, v in weapon_delta.items() if v != 0}
                if not net_change and has_self_replacement:
                    continue  # Skip - this is effectively a no-op

                out.append(make(pts_delta, add_rules, weapon_delta))
            return out

    # fallback: treat each option as rule
    out.append(make(0, [], {}))
    for o in opts:
        pts = int(o.get("pts", 0) or 0)
        txt = str(o.get("text", "")).strip()
        out.append(make(pts, _extract_rules_from_choice(txt), {}))
    return out

# =========================================================
# Mixed-radix indexing (deterministic combo_index lineage)
# =========================================================
def total_combinations(radices: List[int]) -> int:
    t = 1
    for r in radices:
        t *= r
    return t

def index_to_choice_indices(idx: int, radices: List[int]) -> List[int]:
    out = [0] * len(radices)
    for pos in range(len(radices) - 1, -1, -1):
        r = radices[pos]
        idx, rem = divmod(idx, r)
        out[pos] = rem
    return out

# =========================================================
# Stage-1 parallel worker (globals to avoid re-pickling per task)
# =========================================================
_G_UNIT: Dict[str, Any] = {}
_G_GROUP_VARS: List[List[Variant]] = []
_G_RADICES: List[int] = []
_G_BASE_PTS: int = 0
_G_BASE_RULES: List[str] = []
_G_BASE_W: Dict[str, int] = {}

def _init_worker(unit: Dict[str, Any],
                 group_vars: List[List[Variant]],
                 base_pts: int,
                 base_rules: List[str],
                 base_weapon_multiset: Dict[str, int]) -> None:
    global _G_UNIT, _G_GROUP_VARS, _G_RADICES, _G_BASE_PTS, _G_BASE_RULES, _G_BASE_W
    _G_UNIT = unit
    _G_GROUP_VARS = group_vars
    _G_RADICES = [len(vs) for vs in group_vars]
    _G_BASE_PTS = base_pts
    _G_BASE_RULES = base_rules
    _G_BASE_W = base_weapon_multiset

def _header_for(points: int, rules_sig: Tuple[str, ...]) -> str:
    name = str(_G_UNIT.get("name", "")).strip()
    size = int(_G_UNIT.get("size", 1) or 1)
    q = int(_G_UNIT.get("quality", 0) or 0)
    d = int(_G_UNIT.get("defense", 0) or 0)
    rules_str = ", ".join(rules_sig)
    return f"{name} [{size}] Q{q}+ D{d}+ | {points}pts | {rules_str}"

def _worker_stage1_range(start_idx: int, end_idx: int) -> Dict[str, Any]:
    """
    Returns a dict:
      sig -> {count:int, rep_idx:int, rep_header:str, points:int}
    """
    out: Dict[str, Dict[str, Any]] = {}

    for combo_idx in range(start_idx, end_idx):
        # Build combo by indexing variants
        if _G_RADICES:
            choice_idxs = index_to_choice_indices(combo_idx, _G_RADICES)
        else:
            choice_idxs = []

        pts_delta = 0
        add_rules: List[str] = []
        weapon_delta_acc: Dict[str, int] = {}

        for g_i, v_i in enumerate(choice_idxs):
            v = _G_GROUP_VARS[g_i][v_i]
            pts_delta += int(v.pts_delta)
            add_rules.extend(list(v.add_rules))
            for k, dv in v.weapon_delta:
                weapon_delta_acc[k] = weapon_delta_acc.get(k, 0) + int(dv)

        points = int(_G_BASE_PTS + pts_delta)

        # Rules -> normalize (clean artifacts)
        rules_raw = _G_BASE_RULES + add_rules
        rules_sig = normalize_rules_for_signature(rules_raw)

        # Weapons multiset -> base + deltas
        w: Dict[str, int] = dict(_G_BASE_W)
        for k, dv in weapon_delta_acc.items():
            w[k] = w.get(k, 0) + dv
            if w[k] < 0:
                w[k] = 0

        sig = build_stage1_signature(points, rules_sig, w)

        if sig not in out:
            out[sig] = {"count": 1, "rep_idx": combo_idx, "rep_header": _header_for(points, rules_sig), "points": points}
        else:
            out[sig]["count"] += 1
            # deterministic representative: lowest combo index wins
            if combo_idx < out[sig]["rep_idx"]:
                out[sig]["rep_idx"] = combo_idx
                out[sig]["rep_header"] = _header_for(points, rules_sig)
                out[sig]["points"] = points

    return out

# =========================================================
# Stage-2 reduce (no weapon grouping - preserve original names)
# =========================================================
def parse_signature_parts(sig: str) -> List[str]:
    return [p for p in sig.split("||") if p.strip()]

def split_sig_kv(parts: List[str]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for p in parts:
        if "=" in p:
            k, v = p.split("=", 1)
            out[k.strip()] = v
    return out

_ITEM_END_RE = re.compile(r"^(?P<field>.+?)\*(?P<count>\d+)$")

def _range_bucket(rng: Optional[int]) -> str:
    if rng is None:
        return "Melee"
    for b in RANGE_BUCKETS:
        if rng <= b:
            return str(b)
    return RANGE_BUCKET_HIGH

def parse_stage1_W_items(w_value: str) -> List[Tuple[str, Optional[int], Optional[int], Tuple[str, ...], int, int]]:
    """
    Parse weapon items from signature.
    Returns list of: (name, rng, ap, tags, attacks, count)
    """
    if not w_value:
        return []
    tokens = w_value.split("|")
    cur: List[str] = []
    out: List[Tuple[str, Optional[int], Optional[int], Tuple[str, ...], int, int]] = []

    def flush(fields: List[str], count: int) -> None:
        d: Dict[str, str] = {}
        for f in fields:
            if "=" in f:
                k, v = f.split("=", 1)
                d[k.strip()] = v.strip()

        # Extract weapon name
        name = d.get("N", "Unknown Weapon")

        r_raw = d.get("R", "")
        rng: Optional[int] = None
        if r_raw:
            try:
                rng = int(r_raw)
            except Exception:
                rng = None

        a_raw = d.get("A", "")
        attacks = int(a_raw) if a_raw.isdigit() else 0

        ap_raw = d.get("AP", "")
        ap: Optional[int] = None
        if ap_raw:
            try:
                ap = int(ap_raw)
            except Exception:
                ap = None

        t_raw = d.get("T", "")
        tags: Tuple[str, ...] = tuple(sorted([t for t in (t_raw.split(";") if t_raw else []) if t], key=lambda x: x.lower()))
        out.append((name, rng, ap, tags, attacks, count))

    for t in tokens:
        m = _ITEM_END_RE.match(t)
        if m:
            field = m.group("field")
            count = int(m.group("count"))
            cur.append(field)
            flush(cur, count)
            cur = []
        else:
            cur.append(t)

    return out

# =========================================================
# Weapon Grouping System - Groups by matching rules (range, AP, tags)
# =========================================================
@dataclass
class WeaponGroup:
    """Represents a group of weapons with identical rules but potentially different attacks."""
    group_id: str
    rng: Optional[int]
    ap: Optional[int]
    tags: Tuple[str, ...]
    total_attacks: int
    total_count: int
    source_weapons: List[Tuple[str, int, int]]  # List of (name, attacks, count)

def _weapon_rules_key(rng: Optional[int], ap: Optional[int], tags: Tuple[str, ...]) -> str:
    """Create a grouping key based on range, AP, and special rules."""
    r_str = "" if rng is None else str(rng)
    ap_str = "" if ap is None else str(ap)
    tags_str = ";".join(tags)
    return f"R={r_str}|AP={ap_str}|T={tags_str}"

def group_weapons_by_rules(items: List[Tuple[str, Optional[int], Optional[int], Tuple[str, ...], int, int]]) -> Tuple[List[WeaponGroup], Dict[str, List[str]]]:
    """
    Group weapons that have identical rules (range, AP, special rules).
    Weapons can differ in attack values - attacks are summed.

    Returns:
        - List of WeaponGroup objects
        - Dict mapping group_id -> list of source weapon names
    """
    # Group by rules key
    groups_map: Dict[str, List[Tuple[str, Optional[int], Optional[int], Tuple[str, ...], int, int]]] = {}

    for item in items:
        name, rng, ap, tags, attacks, count = item
        key = _weapon_rules_key(rng, ap, tags)
        groups_map.setdefault(key, []).append(item)

    # Build WeaponGroup objects
    weapon_groups: List[WeaponGroup] = []
    lineage: Dict[str, List[str]] = {}

    # Sort keys for deterministic ordering (melee first, then by range, then by AP)
    def sort_group_key(key: str) -> Tuple[int, int, str]:
        parts = dict(p.split("=", 1) for p in key.split("|") if "=" in p)
        r = parts.get("R", "")
        ap = parts.get("AP", "")
        rng_val = -1 if r == "" else int(r)
        ap_val = 0 if ap == "" else int(ap)
        return (rng_val, ap_val, parts.get("T", ""))

    sorted_keys = sorted(groups_map.keys(), key=sort_group_key)

    for idx, key in enumerate(sorted_keys, start=1):
        group_items = groups_map[key]

        # Extract common properties from first item
        _, rng, ap, tags, _, _ = group_items[0]

        # Sum attacks and collect source weapons
        total_attacks = 0
        total_count = 0
        source_weapons: List[Tuple[str, int, int]] = []
        source_names: List[str] = []

        for name, _, _, _, attacks, count in group_items:
            total_attacks += attacks * count
            total_count += count
            source_weapons.append((name, attacks, count))
            source_names.append(name)

        # Generate group ID
        group_id = f"WG{idx:03d}"

        weapon_groups.append(WeaponGroup(
            group_id=group_id,
            rng=rng,
            ap=ap,
            tags=tags,
            total_attacks=total_attacks,
            total_count=total_count,
            source_weapons=source_weapons
        ))

        lineage[group_id] = source_names

    return weapon_groups, lineage

def condensed_weapons_key_from_stage1_signature(stage1_sig: str) -> str:
    """
    Build a weapons key that groups weapons by rules (range, AP, tags).

    If ATTACK_AGNOSTIC_GROUPING is True, attacks are excluded from the key,
    allowing units with different attack totals but same weapon rules to be grouped.
    """
    kv = split_sig_kv(parse_signature_parts(stage1_sig))
    items = parse_stage1_W_items(kv.get("W", ""))

    weapon_groups, _ = group_weapons_by_rules(items)

    # Build condensed key from groups
    parts_out: List[str] = []
    for wg in weapon_groups:
        r_str = "" if wg.rng is None else str(wg.rng)
        ap_str = "" if wg.ap is None else str(wg.ap)
        tags_str = ";".join(wg.tags)

        if ATTACK_AGNOSTIC_GROUPING:
            # Exclude attack count from key - group by rules only
            parts_out.append(f"R={r_str},AP={ap_str},T={tags_str}")
        else:
            # Include attack count in key
            parts_out.append(f"R={r_str},AP={ap_str},T={tags_str},A={wg.total_attacks},C={wg.total_count}")

    # Sort for deterministic key generation
    parts_out.sort()
    return " | ".join(parts_out)

def _get_weapon_display_name(wg: WeaponGroup) -> str:
    """
    Get a display name for a weapon group.
    Handles empty names and provides sensible fallbacks.
    """
    if len(wg.source_weapons) == 1:
        name = wg.source_weapons[0][0]
        # If name is empty or just whitespace, generate a descriptive name
        if not name or not name.strip():
            if wg.rng is None:
                return "Melee"
            else:
                return f"Ranged"
        return name.strip()
    else:
        # Multiple weapons grouped - collect unique non-empty names
        names = [w[0].strip() for w in wg.source_weapons if w[0] and w[0].strip()]
        if names:
            # Use first unique name or group ID if too many
            unique_names = list(dict.fromkeys(names))  # Preserve order, remove dupes
            if len(unique_names) <= 2:
                return " + ".join(unique_names)
        return wg.group_id

def condensed_weapons_line(stage1_sig: str) -> str:
    """
    Generate human-readable weapons line in OPR format.
    Format: WeaponName (A#, AP(#), SpecialRules)

    Groups weapons by rules - weapons with same range/AP/tags are combined.
    """
    kv = split_sig_kv(parse_signature_parts(stage1_sig))
    items = parse_stage1_W_items(kv.get("W", ""))

    weapon_groups, lineage = group_weapons_by_rules(items)

    chunks: List[str] = []
    for wg in weapon_groups:
        display_name = _get_weapon_display_name(wg)

        # Build stats in OPR format: (A#, AP(#), rules)
        inner: List[str] = [f"A{wg.total_attacks}"]
        if wg.ap is not None:
            inner.append(f"AP({wg.ap})")
        inner.extend(list(wg.tags))

        # Format based on range
        if wg.rng is not None:
            # Ranged weapon: show range before name
            weapon_str = f'{wg.rng}" {display_name} ({", ".join(inner)})'
        else:
            # Melee weapon: just name and stats
            weapon_str = f'{display_name} ({", ".join(inner)})'

        # Add count prefix if multiple of same weapon
        if wg.total_count > 1 and len(wg.source_weapons) == 1:
            weapon_str = f"{wg.total_count}x {weapon_str}"

        chunks.append(weapon_str)

    return ", ".join(chunks)

def get_weapon_lineage(stage1_sig: str) -> Dict[str, List[str]]:
    """
    Get the mapping of weapon group IDs to source weapon names.
    """
    kv = split_sig_kv(parse_signature_parts(stage1_sig))
    items = parse_stage1_W_items(kv.get("W", ""))
    _, lineage = group_weapons_by_rules(items)
    return lineage

def inject_sg_into_header(header: str, sg_id: str, meta: str) -> str:
    # Header is already in expected format; just inject after unit name
    m = re.match(r"^(?P<name>.+?)\s+\[(?P<size>\d+)\]\s+Q(?P<q>\d\+)\s+D(?P<d>\d\+)\s+\|\s+(?P<pts>\d+)\s*pts\s+\|\s*(?P<rules>.*)\s*$", header.strip())
    label = f"{sg_id} ({meta})"
    if not m:
        return f"{label} {header}".strip()
    name = m.group("name").strip()
    rest = header[len(name):]
    return f"{name} - {label}{rest}"

def stage2_reduce(stage1_groups: List[Dict[str, Any]],
                 out_txt: Path,
                 out_stage2_json: Path,
                 out_index_json: Path) -> None:
    super_map: Dict[str, List[Dict[str, Any]]] = {}

    for g in stage1_groups:
        stage1_sig = g.get("signature")
        if not isinstance(stage1_sig, str):
            continue

        kv = split_sig_kv([p for p in parse_signature_parts(stage1_sig) if not p.startswith("PTS=")])  # ignore points
        rules = kv.get("RULES", "")
        wcond = condensed_weapons_key_from_stage1_signature(stage1_sig)

        # Build supergroup signature based on grouping settings
        if RULE_AGNOSTIC_GROUPING:
            # Group by weapons only - ignore rules entirely for maximum reduction
            super_sig = f"W={wcond}"
        else:
            # Group by rules + weapons
            super_sig = f"RULES={rules}||W={wcond}"

        super_map.setdefault(super_sig, []).append(g)

    sorted_super_sigs = sorted(super_map.keys(), key=lambda s: sha1(s))

    out_supergroups: List[Dict[str, Any]] = []
    lineage_index: Dict[str, Dict[str, Any]] = {}
    txt_lines: List[str] = []

    for idx, super_sig in enumerate(sorted_super_sigs, start=1):
        sg_id = f"SG{idx:04d}"
        children = super_map[super_sig]
        rep_child = children[0]  # deterministic due to sorting below

        # compute totals
        child_group_ids = [str(c.get("group_id")) for c in children if c.get("group_id") is not None]
        child_count = len(children)
        members_total = sum(int(c.get("count", 0) or 0) for c in children)

        # Compute points range for traceability
        points_list = [int(c.get("points", 0) or 0) for c in children]
        points_min = min(points_list) if points_list else 0
        points_max = max(points_list) if points_list else 0

        # Collect all unique rules variations for traceability
        rules_variations: Set[str] = set()
        for c in children:
            c_sig = c.get("signature", "")
            if isinstance(c_sig, str):
                c_kv = split_sig_kv(parse_signature_parts(c_sig))
                rules_str = c_kv.get("RULES", "")
                if rules_str:
                    rules_variations.add(rules_str)

        # pick rep within children: smallest rep_combo_index wins
        rep_child = min(children, key=lambda c: int((c.get("representative", {}) or {}).get("combo_index_0based", 10**18)))

        rep_header = ((rep_child.get("representative") or {}) or {}).get("header", "")
        rep_sig = str(rep_child.get("signature", ""))

        # Get weapon lineage for this supergroup
        weapon_lineage = get_weapon_lineage(rep_sig)

        out_supergroups.append({
            "sg_id": sg_id,
            "supergroup_hash": sha1(super_sig)[:10],
            "signature": super_sig,
            "unit": rep_child.get("unit"),
            "count_child_groups": child_count,
            "count_members": members_total,
            "points_range": {"min": points_min, "max": points_max},
            "rules_variations": sorted(rules_variations),
            "child_group_ids": child_group_ids,
            "child_groups": [{
                "group_id": c.get("group_id"),
                "points": c.get("points"),
                "count": c.get("count"),
                "representative_combo_index_0based": ((c.get("representative") or {}) or {}).get("combo_index_0based"),
            } for c in children],
            "representative_child_group": {
                "group_id": rep_child.get("group_id"),
                "signature": rep_sig,
                "representative": rep_child.get("representative"),
                "condensed_weapons_line": condensed_weapons_line(rep_sig),
                "weapon_group_lineage": weapon_lineage,
            },
        })

        lineage_index[sg_id] = {
            "supergroup_hash": sha1(super_sig)[:10],
            "unit": rep_child.get("unit"),
            "points_range": {"min": points_min, "max": points_max},
            "rules_variations_count": len(rules_variations),
            "child_group_ids": child_group_ids,
            "members_total": members_total,
            "weapon_group_lineage": weapon_lineage,
            "child_groups": [{
                "group_id": c.get("group_id"),
                "points": c.get("points"),
                "count": c.get("count"),
                "representative_combo_index_0based": ((c.get("representative") or {}) or {}).get("combo_index_0based"),
            } for c in children],
        }

        if isinstance(rep_header, str) and rep_header.strip():
            meta = f"child_groups={child_count} members={members_total}"
            txt_lines.append(inject_sg_into_header(rep_header, sg_id, meta))
            txt_lines.append(condensed_weapons_line(rep_sig))
            if ADD_BLANK_LINE_BETWEEN_UNITS:
                txt_lines.append("")

    out_stage2_json.write_text(json.dumps({
        "settings": {
            "IGNORE_POINTS": True,
            "ATTACK_AGNOSTIC_GROUPING": ATTACK_AGNOSTIC_GROUPING,
            "RULE_AGNOSTIC_GROUPING": RULE_AGNOSTIC_GROUPING,
            "WEAPON_GROUPING": "by_weapons_only" if RULE_AGNOSTIC_GROUPING else (
                "by_rules_attack_agnostic" if ATTACK_AGNOSTIC_GROUPING else "by_rules"
            ),
            "WEAPON_GROUP_ID_FORMAT": "WG001..",
            "SG_ID_FORMAT": "SG0001..",
            "RULE_CLEANUP": {
                "FILTER_COST_MODIFIERS": True,
                "FILTER_WEAPON_PROFILES_IN_RULES": True,
                "FIX_TRUNCATED_RULES": True,
            },
            "TRACEABILITY": {
                "points_range": "Each supergroup includes min/max points of child groups",
                "rules_variations": "All unique rule sets within the supergroup are listed",
                "child_groups": "Each child group includes points and combo_index for exact loadout reconstruction",
                "weapon_group_lineage": "Maps weapon group IDs to source weapon names",
            },
            "NOTE": "Full traceability maintained. Use child_groups[].group_id + combo_index to trace "
                    "simulation results back to specific ungrouped unit loadouts.",
        },
        "total_stage1_groups": len(stage1_groups),
        "total_supergroups": len(out_supergroups),
        "supergroups": out_supergroups,
    }, indent=2), encoding="utf-8")

    out_index_json.write_text(json.dumps({
        "total_supergroups": len(out_supergroups),
        "index": lineage_index
    }, indent=2), encoding="utf-8")

    out_txt.write_text("\n".join(txt_lines).rstrip() + "\n", encoding="utf-8")

# =========================================================
# Raw Loadout Mode: No grouping, each combo gets a UID
# =========================================================
@dataclass
class RawLoadout:
    """Represents a single unit loadout with UID."""
    uid: str
    combo_index: int
    unit_name: str
    points: int
    quality: int
    defense: int
    size: int
    tough: Optional[int]
    rules: Tuple[str, ...]
    weapons: List[Dict[str, Any]]  # Full weapon details

def _format_weapon_for_display(w: Dict[str, Any]) -> str:
    """Format a single weapon for display in OPR format."""
    name = w.get("name", "Unknown")
    count = w.get("count", 1)
    rng = w.get("range", "-")
    attacks = w.get("attacks", 0)
    ap = w.get("ap")
    special = w.get("special", [])

    # Build inner stats
    inner: List[str] = [f"A{attacks}"]
    if ap is not None:
        inner.append(f"AP({ap})")
    if special:
        inner.extend(special)

    # Format based on range
    if rng and rng != "-":
        weapon_str = f'{rng} {name} ({", ".join(inner)})'
    else:
        weapon_str = f'{name} ({", ".join(inner)})'

    # Add count prefix
    if count > 1:
        weapon_str = f"{count}x {weapon_str}"

    return weapon_str

def _raw_loadout_header(loadout: RawLoadout) -> str:
    """Generate header line for a raw loadout with UID in unit name."""
    # Format: "UnitName [UID:XXXXXXXX] [size] Qn+ Dn+ | pts | rules"
    rules_str = ", ".join(loadout.rules) if loadout.rules else "-"
    return (f"{loadout.unit_name} [UID:{loadout.uid}] [{loadout.size}] "
            f"Q{loadout.quality}+ D{loadout.defense}+ | {loadout.points}pts | {rules_str}")

def _raw_loadout_weapons_line(loadout: RawLoadout) -> str:
    """Generate weapons line for a raw loadout."""
    if not loadout.weapons:
        return "-"
    return ", ".join(_format_weapon_for_display(w) for w in loadout.weapons)

def _worker_raw_loadouts_range(start_idx: int, end_idx: int) -> List[Dict[str, Any]]:
    """
    Generate raw loadouts for a range of combo indices.
    Returns list of loadout dicts (not grouped).
    """
    out: List[Dict[str, Any]] = []

    for combo_idx in range(start_idx, end_idx):
        # Build combo by indexing variants
        if _G_RADICES:
            choice_idxs = index_to_choice_indices(combo_idx, _G_RADICES)
        else:
            choice_idxs = []

        pts_delta = 0
        add_rules: List[str] = []
        weapon_delta_acc: Dict[str, int] = {}

        for g_i, v_i in enumerate(choice_idxs):
            v = _G_GROUP_VARS[g_i][v_i]
            pts_delta += int(v.pts_delta)
            add_rules.extend(list(v.add_rules))
            for k, dv in v.weapon_delta:
                weapon_delta_acc[k] = weapon_delta_acc.get(k, 0) + int(dv)

        points = int(_G_BASE_PTS + pts_delta)

        # Rules -> normalize (clean artifacts)
        rules_raw = _G_BASE_RULES + add_rules
        rules_sig = normalize_rules_for_signature(rules_raw)

        # Weapons multiset -> base + deltas
        w: Dict[str, int] = dict(_G_BASE_W)
        for k, dv in weapon_delta_acc.items():
            w[k] = w.get(k, 0) + dv
            if w[k] < 0:
                w[k] = 0

        # Convert weapon keys back to weapon dicts
        weapons: List[Dict[str, Any]] = []
        for wkey, count in w.items():
            if count <= 0:
                continue
            # Parse weapon key: N=name|R=range|A=attacks|AP=ap|T=tags
            wparts = {}
            for part in wkey.split("|"):
                if "=" in part:
                    pk, pv = part.split("=", 1)
                    wparts[pk] = pv

            name = wparts.get("N", "Unknown")
            rng_str = wparts.get("R", "")
            attacks_str = wparts.get("A", "0")
            ap_str = wparts.get("AP", "")
            tags_str = wparts.get("T", "")

            rng = f'{rng_str}"' if rng_str else "-"
            attacks = int(attacks_str) if attacks_str.isdigit() else 0
            ap = int(ap_str) if ap_str and ap_str.lstrip("-").isdigit() else None
            tags = [t for t in tags_str.split(";") if t] if tags_str else []

            weapons.append({
                "name": name,
                "count": count,
                "range": rng,
                "attacks": attacks,
                "ap": ap,
                "special": tags,
            })

        # Sort weapons for consistent output (melee first, then by range)
        weapons.sort(key=lambda x: (x["range"] != "-", x["range"], x["name"]))

        # Build signature for UID generation
        sig = build_stage1_signature(points, rules_sig, w)
        uid = generate_uid(str(_G_UNIT.get("name", "")), combo_idx, sig)

        out.append({
            "uid": uid,
            "combo_index": combo_idx,
            "points": points,
            "rules": list(rules_sig),
            "weapons": weapons,
            "signature": sig,
        })

    return out

def generate_raw_loadouts_parallel(unit: Dict[str, Any],
                                   limit: int = 0) -> Dict[str, Any]:
    """
    Generate all raw loadouts for a unit with UIDs (no grouping).
    """
    base_pts = int(unit.get("base_points", 0) or 0)
    base_rules = [str(x).strip() for x in (unit.get("special_rules") or []) if str(x).strip()]

    base_weapon_multiset, name_to_key = build_base_weapon_multiset(unit)

    groups = unit.get("options", []) or []
    group_vars: List[List[Variant]] = [group_variants(unit, g, name_to_key) for g in groups]
    radices = [len(vs) for vs in group_vars]
    total = total_combinations(radices) if radices else 1
    if limit and total > limit:
        total = limit

    if total <= 1 or WORKERS_PER_UNIT <= 1:
        _init_worker(unit, group_vars, base_pts, base_rules, base_weapon_multiset)
        all_loadouts = _worker_raw_loadouts_range(0, total)
    else:
        n_tasks = min(TASKS_PER_UNIT, total)
        chunk = math.ceil(total / n_tasks)

        all_loadouts: List[Dict[str, Any]] = []
        with ProcessPoolExecutor(
            max_workers=WORKERS_PER_UNIT,
            initializer=_init_worker,
            initargs=(unit, group_vars, base_pts, base_rules, base_weapon_multiset)
        ) as ex:
            futures = []
            for t in range(n_tasks):
                s = t * chunk
                e = min(total, (t + 1) * chunk)
                if s >= e:
                    continue
                futures.append(ex.submit(_worker_raw_loadouts_range, s, e))

            for fut in as_completed(futures):
                all_loadouts.extend(fut.result())

    # Sort by combo_index for deterministic output
    all_loadouts.sort(key=lambda x: x["combo_index"])

    # Build RawLoadout objects and output
    unit_name = str(unit.get("name", "")).strip()
    quality = int(unit.get("quality", 0) or 0)
    defense = int(unit.get("defense", 0) or 0)
    size = int(unit.get("size", 1) or 1)
    tough = unit.get("tough")

    raw_loadouts: List[RawLoadout] = []
    for lo in all_loadouts:
        raw_loadouts.append(RawLoadout(
            uid=lo["uid"],
            combo_index=lo["combo_index"],
            unit_name=unit_name,
            points=lo["points"],
            quality=quality,
            defense=defense,
            size=size,
            tough=tough,
            rules=tuple(lo["rules"]),
            weapons=lo["weapons"],
        ))

    return {
        "unit": unit_name,
        "total_loadouts": len(raw_loadouts),
        "total_combinations_processed": total,
        "loadouts": [
            {
                "uid": lo.uid,
                "combo_index": lo.combo_index,
                "points": lo.points,
                "rules": list(lo.rules),
                "weapons": lo.weapons,
            }
            for lo in raw_loadouts
        ],
        "raw_loadout_objects": raw_loadouts,  # For TXT generation
    }

def write_raw_loadouts_txt(payload: Dict[str, Any], out_txt: Path) -> None:
    """Write raw loadouts to TXT file."""
    raw_loadouts: List[RawLoadout] = payload.get("raw_loadout_objects", [])
    txt_lines: List[str] = []

    for lo in raw_loadouts:
        txt_lines.append(_raw_loadout_header(lo))
        txt_lines.append(_raw_loadout_weapons_line(lo))
        if ADD_BLANK_LINE_BETWEEN_UNITS:
            txt_lines.append("")

    out_txt.write_text("\n".join(txt_lines).rstrip() + "\n", encoding="utf-8")

def write_raw_loadouts_json(payload: Dict[str, Any], out_json: Path) -> None:
    """Write raw loadouts to JSON file (without RawLoadout objects)."""
    output = {
        "unit": payload["unit"],
        "total_loadouts": payload["total_loadouts"],
        "total_combinations_processed": payload["total_combinations_processed"],
        "settings": {
            "RAW_LOADOUT_MODE": True,
            "UID_FORMAT": "8-char hex hash",
        },
        "loadouts": payload["loadouts"],
    }
    out_json.write_text(json.dumps(output, indent=2), encoding="utf-8")

# =========================================================
# Unit pipeline: inline stage-1 with within-unit parallel generation
# =========================================================
def stage1_reduce_inline_parallel(unit: Dict[str, Any],
                                 limit: int = 0) -> Dict[str, Any]:
    base_pts = int(unit.get("base_points", 0) or 0)
    base_rules = [str(x).strip() for x in (unit.get("special_rules") or []) if str(x).strip()]

    base_weapon_multiset, name_to_key = build_base_weapon_multiset(unit)

    groups = unit.get("options", []) or []
    group_vars: List[List[Variant]] = [group_variants(unit, g, name_to_key) for g in groups]
    radices = [len(vs) for vs in group_vars]
    total = total_combinations(radices) if radices else 1
    if limit and total > limit:
        total = limit

    if total <= 1 or WORKERS_PER_UNIT <= 1:
        _init_worker(unit, group_vars, base_pts, base_rules, base_weapon_multiset)
        partial = _worker_stage1_range(0, total)
        merged = partial
    else:
        n_tasks = min(TASKS_PER_UNIT, total)
        chunk = math.ceil(total / n_tasks)

        merged: Dict[str, Dict[str, Any]] = {}
        with ProcessPoolExecutor(
            max_workers=WORKERS_PER_UNIT,
            initializer=_init_worker,
            initargs=(unit, group_vars, base_pts, base_rules, base_weapon_multiset)
        ) as ex:
            futures = []
            for t in range(n_tasks):
                s = t * chunk
                e = min(total, (t + 1) * chunk)
                if s >= e:
                    continue
                futures.append(ex.submit(_worker_stage1_range, s, e))

            for fut in as_completed(futures):
                part = fut.result()
                for sig, info in part.items():
                    if sig not in merged:
                        merged[sig] = dict(info)
                    else:
                        merged[sig]["count"] += int(info["count"])
                        # representative: lowest index wins
                        if int(info["rep_idx"]) < int(merged[sig]["rep_idx"]):
                            merged[sig]["rep_idx"] = int(info["rep_idx"])
                            merged[sig]["rep_header"] = info["rep_header"]
                            merged[sig]["points"] = info["points"]

    # Build stage-1 groups list
    out_groups: List[Dict[str, Any]] = []
    for sig in sorted(merged.keys(), key=lambda s: sha1(s)):
        info = merged[sig]
        group_id = sha1(sig)[:10]
        rep_idx = int(info["rep_idx"])
        rep = {
            "combo_index_0based": rep_idx,
            "combo_index_1based": rep_idx + 1,
            "header": info["rep_header"],
        }
        out_groups.append({
            "group_id": group_id,
            "signature": sig,
            "unit": unit.get("name"),
            "points": int(info.get("points", 0) or 0),
            "count": int(info["count"]),
            "representative": rep,
        })

    payload = {
        "unit": unit.get("name"),
        "total_combinations_processed": total,
        "total_groups": len(out_groups),
        "settings": {
            "INCLUDE_POINTS_IN_STAGE1_SIGNATURE": INCLUDE_POINTS_IN_STAGE1_SIGNATURE,
            "WORKERS_PER_UNIT": WORKERS_PER_UNIT,
            "TASKS_PER_UNIT": TASKS_PER_UNIT,
            "PRESERVE_WEAPON_NAMES": True,
            "OPTIMIZATIONS": {
                "SYMMETRY_REDUCTION": True,  # combinations_with_replacement instead of product
                "OPTION_DEDUPLICATION": True,  # dedupe options with same weapon key
                "SKIP_SELF_REPLACEMENTS": True,  # skip replacing weapon with itself
            },
            "NOTE": "Lineage uses combo_index (mixed-radix) instead of storing every member record.",
        },
        "groups": out_groups,
    }
    return payload

# =========================================================
# Runner
# =========================================================
def parse_faction_from_filename(filename: str) -> Tuple[str, str]:
    """
    Parse faction name and version from PDF filename.
    Expected formats:
      - "GF - Blessed Sisters 3.5.1.pdf" -> ("Blessed Sisters", "3.5.1")
      - "AoF - Dark Elves 2.0.pdf" -> ("Dark Elves", "2.0")
      - "Blessed Sisters.pdf" -> ("Blessed Sisters", "unknown")
    """
    name = filename
    # Remove .pdf extension
    if name.lower().endswith(".pdf"):
        name = name[:-4]

    # Remove common prefixes like "GF - ", "AoF - ", "AoFS - ", "GFF - "
    prefix_match = re.match(r"^(GF|AoF|AoFS|GFF|FF)\s*-\s*", name, re.IGNORECASE)
    if prefix_match:
        name = name[prefix_match.end():]

    # Try to extract version number at the end (e.g., "3.5.1", "2.0", "v2.1")
    version_match = re.search(r"\s+v?(\d+(?:\.\d+)+)\s*$", name, re.IGNORECASE)
    if version_match:
        version = version_match.group(1)
        faction_name = name[:version_match.start()].strip()
    else:
        version = "unknown"
        faction_name = name.strip()

    return faction_name, version

def find_pdf_files(input_path: Path) -> List[Path]:
    """
    Find all PDF files to process.
    If input_path is a file, return it as a single-item list.
    If input_path is a directory, return all PDF files in it.
    """
    if input_path.is_file():
        if input_path.suffix.lower() == ".pdf":
            return [input_path]
        else:
            raise ValueError(f"Input file is not a PDF: {input_path}")
    elif input_path.is_dir():
        pdf_files = sorted(input_path.glob("*.pdf"))
        if not pdf_files:
            raise FileNotFoundError(f"No PDF files found in directory: {input_path}")
        return pdf_files
    else:
        raise FileNotFoundError(f"Input path not found: {input_path}")

# =========================================================
# Merge all *.final.txt files into one army TXT
# =========================================================
# Matches: " - SG0001 (child_groups=... members=...)"
_SG_LABEL_RE = re.compile(r"\s+-\s+SG\d{4,}\s+\([^)]*\)")

def _looks_like_header(line: str) -> bool:
    """Check if a line looks like a unit header."""
    s = line.strip()
    # Headers have "Q" "D" and "pts |"
    return (" Q" in s) and (" D" in s) and ("pts |" in s)

def _maybe_strip_sg(line: str) -> str:
    """Optionally strip SG labels from header lines."""
    if STRIP_SG_LABELS and _looks_like_header(line):
        return _SG_LABEL_RE.sub("", line).rstrip()
    return line.rstrip()

def merge_final_txts(faction_dir: Path, faction_name: str) -> Optional[Path]:
    """
    Merge all *.final.txt files in faction_dir into one merged file.
    Returns the path to the merged file, or None if no files found.
    """
    if not MERGE_FINAL_TXTS:
        return None

    # Find all *.final.txt files (recursively)
    final_files = sorted(
        [p for p in faction_dir.rglob("*.final.txt") if p.is_file()],
        key=lambda p: (p.parent.name.lower(), p.name.lower())
    )

    if not final_files:
        return None

    out_file = faction_dir / f"{safe_filename(faction_name)}.final.merged.txt"

    # Exclude the output file itself if it exists
    final_files = [f for f in final_files if f.resolve() != out_file.resolve()]

    if not final_files:
        return None

    out_lines: List[str] = []
    for f in final_files:
        lines = f.read_text(encoding="utf-8", errors="replace").splitlines()

        # Strip BOM if present
        if lines and lines[0].startswith("\ufeff"):
            lines[0] = lines[0].lstrip("\ufeff")

        # Apply optional SG stripping to header lines
        lines = [_maybe_strip_sg(ln) for ln in lines]

        # Append with blank line separation
        if out_lines and ADD_BLANK_LINE_BETWEEN_FILES:
            if out_lines[-1].strip() != "":
                out_lines.append("")
            # Skip leading blanks in the next file
            while lines and lines[0].strip() == "":
                lines.pop(0)

        out_lines.extend(lines)

    out_file.write_text("\n".join(out_lines).rstrip() + "\n", encoding="utf-8")
    return out_file

def process_single_pdf(pdf_path: Path,
                       out_dir: Path) -> None:
    """Process a single PDF file and generate all outputs."""
    faction_name, faction_version = parse_faction_from_filename(pdf_path.name)
    faction_dir = out_dir / safe_filename(f"{faction_name}_{faction_version}")
    ensure_dir(faction_dir)

    print(f"\n{'='*60}")
    print(f"Processing: {pdf_path.name}")
    print(f"Faction: {faction_name} | Version: {faction_version}")
    print(f"Output: {faction_dir}")
    print(f"{'='*60}")

    # Parse PDF
    lines = extract_lines(str(pdf_path))
    units = parse_units(lines)

    if not units:
        print(f"[WARN] No units found in {pdf_path.name}, skipping...")
        return

    units_payload = {"faction": faction_name, "version": faction_version, "units": units}
    units_json_path = faction_dir / f"{safe_filename(faction_name)}_{faction_version}_units.json"
    units_json_path.write_text(json.dumps(units_payload, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"[OK] Parsed {len(units)} units -> {units_json_path}")

    for u in units:
        unit_name = str(u.get("name", "")).strip()
        if not unit_name:
            continue

        unit_dir = faction_dir / safe_filename(unit_name)
        ensure_dir(unit_dir)

        print(f"\n  --- {unit_name} ---")

        if RAW_LOADOUT_MODE:
            # Raw loadout mode: each combo gets a UID, no grouping
            raw_json_path = unit_dir / f"{safe_filename(unit_name)}.raw_loadouts.json"
            raw_txt_path  = unit_dir / f"{safe_filename(unit_name)}.final.txt"

            raw_payload = generate_raw_loadouts_parallel(u, limit=MAX_LOADOUTS_PER_UNIT)
            write_raw_loadouts_json(raw_payload, raw_json_path)
            write_raw_loadouts_txt(raw_payload, raw_txt_path)
            print(f"  [OK] Raw loadouts: {raw_payload.get('total_loadouts'):,} loadouts "
                  f"(from {raw_payload.get('total_combinations_processed'):,} combos)")
            print(f"  [OK] Output: -> {raw_txt_path.name}")
        else:
            # Grouped mode: Stage-1 + Stage-2 reduction
            stage1_json_path = unit_dir / f"{safe_filename(unit_name)}.loadouts.reduced.json"
            final_txt_path  = unit_dir / f"{safe_filename(unit_name)}.final.txt"
            final_json_path = unit_dir / f"{safe_filename(unit_name)}.final.supergroups.json"
            final_idx_path  = unit_dir / f"{safe_filename(unit_name)}.final.lineage_index.json"

            # Stage-1 inline
            stage1_payload = stage1_reduce_inline_parallel(u, limit=MAX_LOADOUTS_PER_UNIT)
            stage1_json_path.write_text(json.dumps(stage1_payload, indent=2), encoding="utf-8")
            print(f"  [OK] Stage-1: {stage1_payload.get('total_groups'):,} groups "
                  f"(from {stage1_payload.get('total_combinations_processed'):,} combos)")

            # Stage-2 final
            stage2_reduce(stage1_payload["groups"], final_txt_path, final_json_path, final_idx_path)
            print(f"  [OK] Stage-2: -> {final_txt_path.name}")

    # Merge all *.final.txt files into one
    merged_path = merge_final_txts(faction_dir, faction_name)
    if merged_path:
        print(f"\n  [OK] Merged all units -> {merged_path.name}")

def run() -> None:
    input_path = Path(PDF_INPUT_PATH).expanduser()
    out_dir = Path(OUTPUT_DIR).expanduser()

    if not input_path.exists():
        raise FileNotFoundError(f"PDF input path not found: {input_path}")

    ensure_dir(out_dir)

    # Find all PDF files to process
    pdf_files = find_pdf_files(input_path)
    print(f"[INFO] Found {len(pdf_files)} PDF file(s) to process")
    print(f"[INFO] WORKERS_PER_UNIT={WORKERS_PER_UNIT}, TASKS_PER_UNIT={TASKS_PER_UNIT}")
    print(f"[INFO] RAW_LOADOUT_MODE={RAW_LOADOUT_MODE}")
    if not RAW_LOADOUT_MODE:
        print(f"[INFO] ATTACK_AGNOSTIC_GROUPING={ATTACK_AGNOSTIC_GROUPING}, RULE_AGNOSTIC_GROUPING={RULE_AGNOSTIC_GROUPING}")

    # Process each PDF
    for pdf_path in pdf_files:
        try:
            process_single_pdf(pdf_path, out_dir)
        except Exception as e:
            print(f"[ERROR] Failed to process {pdf_path.name}: {e}")
            import traceback
            traceback.print_exc()
            continue

    print(f"\n{'='*60}")
    print(f"DONE. Processed {len(pdf_files)} PDF file(s).")
    print(f"Output directory: {out_dir}")
    print(f"{'='*60}")

if __name__ == "__main__":
    run()
