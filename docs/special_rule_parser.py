#!/usr/bin/env python3
"""
Special Rule Parser - Identifies rule effects by parsing rule text.

This module provides functions to classify special rules based on their
actual mechanical effects, not just keyword matching.
"""

import re
from dataclasses import dataclass
from enum import Enum, auto
from typing import Optional, Tuple, List


class TargetType(Enum):
    """Who/what the rule targets."""
    SELF = auto()           # "This model gets..."
    SELF_UNIT = auto()      # "This model and its unit get..."
    FRIENDLY_UNIT = auto()  # "Pick one friendly unit..."
    ENEMY_UNIT = auto()     # "Pick one enemy unit..."
    ENEMY_MODEL = auto()    # "Pick one enemy model..."
    ALL_FRIENDLY = auto()   # "Friendly units within X..."
    ALL_ENEMY = auto()      # "Enemy units within X..."
    NONE = auto()           # Passive or unclear


class EffectType(Enum):
    """What the rule does."""
    BUFF_STAT = auto()          # +1 to hit, +1 defense, etc.
    DEBUFF_STAT = auto()        # -1 to hit, -1 defense, etc.
    GRANT_ABILITY = auto()      # "gets Fearless", "gets Stealth"
    DEAL_DAMAGE = auto()        # "takes X hits"
    MOVEMENT_BONUS = auto()     # +X" movement
    ATTACK_BONUS = auto()       # Extra hits, extra attacks on 6s
    DEFENSE_BONUS = auto()      # Ignore wounds, regeneration
    MARK_ENEMY = auto()         # Mark enemy for friendly bonus against them
    AURA = auto()               # "This model and its unit get..."
    SPAWN = auto()              # Create new units
    TELEPORT = auto()           # Reposition
    OTHER = auto()


class ActionTiming(Enum):
    """When the rule can be used."""
    PASSIVE = auto()            # Always active
    ONCE_PER_ACTIVATION = auto()
    ONCE_PER_GAME = auto()
    ONCE_PER_ROUND = auto()
    ON_TRIGGER = auto()         # "When X happens..."


@dataclass
class ParsedRule:
    """Result of parsing a special rule."""
    rule_name: str
    target_type: TargetType
    effect_type: EffectType
    timing: ActionTiming
    should_ignore: bool
    ignore_reason: str
    effect_bucket: str
    raw_text: str

    # Extracted values
    bonus_value: Optional[int] = None       # e.g., +2 from "+2 to hit"
    range_inches: Optional[int] = None      # e.g., 12 from "within 12""
    granted_ability: Optional[str] = None   # e.g., "Fearless" from "gets Fearless"


def normalize_text(text: str) -> str:
    """Fix encoding issues and normalize whitespace."""
    if not text:
        return ''
    text = str(text)
    text = text.replace('â€™', "'").replace('â€œ', '"').replace('â€', '"')
    text = text.replace('â€"', '-').replace('â€"', '-')
    text = re.sub(r'\s+', ' ', text)
    return text.strip()


def extract_range(text: str) -> Optional[int]:
    """Extract range in inches from text like 'within 12"' or 'within 18"'."""
    m = re.search(r'within (\d+)["\']', text.lower())
    if m:
        return int(m.group(1))
    return None


def extract_bonus_value(text: str) -> Optional[int]:
    """Extract numeric bonus like +1, +2, -1 from text."""
    m = re.search(r'([+-]\d+)', text)
    if m:
        return int(m.group(1))
    return None


def extract_granted_ability(text: str) -> Optional[str]:
    """Extract ability name from 'gets X' or 'get X' patterns."""
    # Match "gets Fearless" or "get Stealth" etc.
    m = re.search(r'gets? ([A-Z][a-zA-Z\s]+?)(?:\s+once|\s*\.|\s*,|$)', text)
    if m:
        return m.group(1).strip()
    return None


def parse_timing(text: str, once_per_game: bool = False, once_per_activation: bool = False) -> ActionTiming:
    """Determine when the rule can be used."""
    text_lower = text.lower()

    if once_per_game or 'once per game' in text_lower:
        return ActionTiming.ONCE_PER_GAME
    if once_per_activation or 'once per activation' in text_lower:
        return ActionTiming.ONCE_PER_ACTIVATION
    if 'once per round' in text_lower:
        return ActionTiming.ONCE_PER_ROUND
    if re.search(r'^when ', text_lower) or 'when this' in text_lower:
        return ActionTiming.ON_TRIGGER
    return ActionTiming.PASSIVE


def parse_rule(rule_name: str, rule_text: str,
               once_per_game: bool = False,
               once_per_activation: bool = False) -> ParsedRule:
    """
    Parse a special rule and classify its effect.

    Returns a ParsedRule with target, effect, timing, and whether to ignore.
    """
    text = normalize_text(rule_text)
    text_lower = text.lower()

    timing = parse_timing(text, once_per_game, once_per_activation)
    range_inches = extract_range(text)
    bonus_value = extract_bonus_value(text)
    granted_ability = extract_granted_ability(text)

    # === PATTERN: Pick friendly unit(s) - BUFF SPELLS ===
    # "Pick one friendly unit within X", which gets..."
    # "Pick up to two friendly units within X", which get..."
    if re.search(r'pick (one|up to \w+) friendly unit', text_lower):
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.FRIENDLY_UNIT,
            effect_type=EffectType.GRANT_ABILITY,
            timing=timing,
            should_ignore=True,
            ignore_reason='Buff spell targeting friendly units',
            effect_bucket='BUFF_SPELL_FRIENDLY',
            raw_text=text,
            range_inches=range_inches,
            granted_ability=granted_ability
        )

    # === PATTERN: Pick enemy unit which takes hits - DAMAGE SPELLS ===
    # "Pick one enemy unit within X", which takes Y hits..."
    if re.search(r'pick .* enemy .* which takes? \d+ hit', text_lower):
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.ENEMY_UNIT,
            effect_type=EffectType.DEAL_DAMAGE,
            timing=timing,
            should_ignore=True,
            ignore_reason='Damage spell targeting enemies',
            effect_bucket='DAMAGE_SPELL_ENEMY',
            raw_text=text,
            range_inches=range_inches
        )

    # === PATTERN: Pick enemy unit which gets debuff - DEBUFF SPELLS ===
    # "Pick one enemy unit within X", which gets -1 to..."
    if re.search(r'pick .* enemy .* which gets? .*((-\d|counts as|difficult terrain))', text_lower):
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.ENEMY_UNIT,
            effect_type=EffectType.DEBUFF_STAT,
            timing=timing,
            should_ignore=True,
            ignore_reason='Debuff spell targeting enemies',
            effect_bucket='DEBUFF_SPELL_ENEMY',
            raw_text=text,
            range_inches=range_inches
        )

    # === PATTERN: Mark enemy for friendly bonus - MARK SPELLS ===
    # "Pick enemy unit, which friendly units gets X against"
    if re.search(r'pick .* enemy .* which friendly unit', text_lower):
        # Extract what friendly units get
        m = re.search(r'friendly units? gets? (\w+)', text_lower)
        ability = m.group(1).title() if m else None
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.ENEMY_UNIT,
            effect_type=EffectType.MARK_ENEMY,
            timing=timing,
            should_ignore=True,  # Still a spell effect
            ignore_reason='Mark spell - friendly units get bonus against marked enemy',
            effect_bucket='MARK_SPELL_ENEMY',
            raw_text=text,
            range_inches=range_inches,
            granted_ability=ability
        )

    # === PATTERN: Once per game - IGNORE ===
    if timing == ActionTiming.ONCE_PER_GAME:
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.NONE,
            effect_type=EffectType.OTHER,
            timing=timing,
            should_ignore=True,
            ignore_reason='Once per game effect',
            effect_bucket='ONCE_PER_GAME',
            raw_text=text
        )

    # === PATTERN: Aura - "This model and its unit get X" ===
    if 'this model and its unit get' in text_lower:
        m = re.search(r'this model and its unit get (.+?)\.', text_lower)
        ability = m.group(1).strip().title() if m else None
        bucket = f"AURA_{ability.upper().replace(' ', '_')[:20]}" if ability else "AURA_UNKNOWN"
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF_UNIT,
            effect_type=EffectType.AURA,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=bucket,
            raw_text=text,
            granted_ability=ability
        )

    # === PATTERN: Self hit bonus - "This model gets +1 to hit" ===
    if re.search(r'this model gets \+\d+ to hit', text_lower):
        bonus = extract_bonus_value(text)
        if 'shooting' in text_lower:
            bucket = f'SELF_HIT_+{bonus}_SHOOTING'
        elif 'melee' in text_lower:
            bucket = f'SELF_HIT_+{bonus}_MELEE'
        else:
            bucket = f'SELF_HIT_+{bonus}_ANY'
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.BUFF_STAT,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=bucket,
            raw_text=text,
            bonus_value=bonus
        )

    # === PATTERN: Self hit penalty - "This model gets -1 to hit" ===
    if re.search(r'this model gets -\d+ to hit', text_lower):
        bonus = extract_bonus_value(text)
        if 'shooting' in text_lower:
            bucket = f'SELF_HIT_{bonus}_SHOOTING'
        else:
            bucket = f'SELF_HIT_{bonus}_ANY'
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.DEBUFF_STAT,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=bucket,
            raw_text=text,
            bonus_value=bonus
        )

    # === PATTERN: Defense - "Enemies get -1 to hit" (when attacking this unit) ===
    if re.search(r'enem(y|ies).* get.* -\d+ to hit', text_lower):
        if 'shot or charged' in text_lower or 'from over' in text_lower:
            bucket = 'DEFENSE_ENEMY_HIT_-1_CONDITIONAL'
        elif 'in melee' in text_lower:
            bucket = 'DEFENSE_ENEMY_HIT_-1_MELEE'
        else:
            bucket = 'DEFENSE_ENEMY_HIT_-1_ALL'
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.DEFENSE_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=bucket,
            raw_text=text
        )

    # === PATTERN: Movement bonus - "Moves +X" when..." ===
    m = re.search(r'moves?\s*\+(\d+)["\']?\s*when using (\w+)', text_lower)
    if m:
        bonus = int(m.group(1))
        action = m.group(2).upper()
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.MOVEMENT_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=f'MOVE_+{bonus}_{action}',
            raw_text=text,
            bonus_value=bonus
        )

    # === PATTERN: On 6 to hit effects ===
    if re.search(r'(unmodified )?(roll|result).* of 6.* to hit', text_lower):
        if '+1 attack' in text_lower:
            if 'melee' in text_lower:
                bucket = 'ON_6_HIT_EXTRA_ATTACK_MELEE'
            else:
                bucket = 'ON_6_HIT_EXTRA_ATTACK_ANY'
        elif 'extra hit' in text_lower:
            if 'melee' in text_lower:
                bucket = 'ON_6_HIT_EXTRA_HIT_MELEE'
            else:
                bucket = 'ON_6_HIT_EXTRA_HIT_ANY'
        elif 'ap (+' in text_lower:
            m = re.search(r'ap \(\+(\d+)\)', text_lower)
            ap_bonus = m.group(1) if m else '?'
            bucket = f'ON_6_HIT_AP_+{ap_bonus}'
        elif 'extra wound' in text_lower:
            bucket = 'ON_6_HIT_EXTRA_WOUND'
        else:
            bucket = 'ON_6_HIT_OTHER'

        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.ATTACK_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=bucket,
            raw_text=text
        )

    # === PATTERN: Ignore wounds on 6+ ===
    if re.search(r'takes? wounds?.* on a 6\+.* ignored', text_lower):
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.DEFENSE_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket='DEFENSE_IGNORE_WOUND_6+',
            raw_text=text
        )

    # === PATTERN: Defense bonus - "+1 to defense rolls" ===
    if '+1 to defense' in text_lower:
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.DEFENSE_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket='DEFENSE_+1',
            raw_text=text,
            bonus_value=1
        )

    # === PATTERN: Ignores regeneration ===
    if 'ignores regeneration' in text_lower:
        if 'extra wound' in text_lower:
            bucket = 'WEAPON_IGNORES_REGEN_+_WOUND'
        elif 'ap (+' in text_lower:
            bucket = 'WEAPON_IGNORES_REGEN_+_AP'
        else:
            bucket = 'WEAPON_IGNORES_REGEN'
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.ATTACK_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=bucket,
            raw_text=text
        )

    # === PATTERN: Ignores cover ===
    if 'ignores cover' in text_lower:
        if 'extra hit' in text_lower:
            bucket = 'WEAPON_IGNORES_COVER_+_HIT'
        elif 'ap (+' in text_lower:
            bucket = 'WEAPON_IGNORES_COVER_+_AP'
        else:
            bucket = 'WEAPON_IGNORES_COVER'
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.ATTACK_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket=bucket,
            raw_text=text
        )

    # === PATTERN: Morale bonus ===
    if '+1 to morale' in text_lower:
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.BUFF_STAT,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket='MORALE_+1',
            raw_text=text,
            bonus_value=1
        )

    # === PATTERN: Recover from shaken ===
    if 'shaken' in text_lower and 'beginning of the round' in text_lower:
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF,
            effect_type=EffectType.DEFENSE_BONUS,
            timing=ActionTiming.PASSIVE,
            should_ignore=False,
            ignore_reason='',
            effect_bucket='DEFENSE_RECOVER_SHAKEN',
            raw_text=text
        )

    # === FALLBACK: Unknown/uncategorized ===
    # Generate a hash-based bucket for identical texts
    import hashlib
    text_hash = hashlib.sha1(text_lower.encode()).hexdigest()[:6].upper()

    return ParsedRule(
        rule_name=rule_name,
        target_type=TargetType.NONE,
        effect_type=EffectType.OTHER,
        timing=timing,
        should_ignore=False,
        ignore_reason='',
        effect_bucket=f'UNCATEGORIZED_{text_hash}',
        raw_text=text
    )


def is_select_friendly_unit_rule(rule_text: str) -> Tuple[bool, str]:
    """
    Check if a rule requires selecting friendly units.

    Returns:
        (is_select_friendly, reason)
    """
    text_lower = normalize_text(rule_text).lower()

    # Pattern 1: "Pick one friendly unit within X"
    if re.search(r'pick one friendly unit within', text_lower):
        return (True, 'Picks one friendly unit')

    # Pattern 2: "Pick up to X friendly units within"
    if re.search(r'pick up to \w+ friendly units? within', text_lower):
        return (True, 'Picks up to X friendly units')

    # Pattern 3: "pick one friendly model within"
    if re.search(r'pick one friendly model within', text_lower):
        return (True, 'Picks one friendly model')

    # NOT a select-friendly pattern:
    # "pick enemy unit, which friendly units gets X against"
    # This marks an ENEMY, doesn't select friendlies

    return (False, '')


if __name__ == '__main__':
    # Test the parser
    import openpyxl

    wb = openpyxl.load_workbook('Faction Specific Army Rules.xlsx', data_only=True)
    ws = wb['Sheet1']
    rows = list(ws.iter_rows(values_only=True))

    rules = {}
    for r in rows[1:]:
        name = r[2]
        text = r[3]
        once_per_game = bool(r[5])
        once_per_activation = bool(r[6])
        if name and text and name not in rules:
            rules[name] = (text, once_per_game, once_per_activation)

    # Parse all rules
    parsed = {}
    for name, (text, opg, opa) in rules.items():
        parsed[name] = parse_rule(name, text, opg, opa)

    # Summary
    from collections import Counter

    buckets = Counter(p.effect_bucket for p in parsed.values())
    ignored = sum(1 for p in parsed.values() if p.should_ignore)

    print(f"Total rules: {len(parsed)}")
    print(f"Rules to IGNORE: {ignored}")
    print(f"Rules to KEEP: {len(parsed) - ignored}")
    print()

    print("=== SELECT FRIENDLY UNIT RULES ===")
    select_friendly = [(n, p) for n, p in parsed.items()
                       if p.effect_bucket == 'BUFF_SPELL_FRIENDLY']
    print(f"Total: {len(select_friendly)}")
    for name, p in sorted(select_friendly)[:10]:
        print(f"  - {name}: {p.granted_ability or '?'}")

    print()
    print("=== TOP EFFECT BUCKETS (kept) ===")
    kept_buckets = Counter(p.effect_bucket for p in parsed.values() if not p.should_ignore)
    for bucket, count in kept_buckets.most_common(20):
        print(f"  {bucket}: {count}")
