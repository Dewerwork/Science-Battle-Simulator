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


def normalize_ability_name(ability: str) -> str:
    """
    Normalize an ability name to a bucket-friendly format.

    Examples:
        "Bane in melee" -> "BANE_MELEE"
        "Shred when shooting" -> "SHRED_SHOOTING"
        "+1 to hit rolls in melee" -> "HIT_+1_MELEE"
        "Fearless" -> "FEARLESS"
    """
    ability_lower = ability.lower().strip()

    # Extract condition (in melee, when shooting, when charging, etc.)
    condition = ''
    if ' in melee' in ability_lower:
        condition = '_MELEE'
        ability_lower = ability_lower.replace(' in melee', '')
    elif ' when shooting' in ability_lower:
        condition = '_SHOOTING'
        ability_lower = ability_lower.replace(' when shooting', '')
    elif ' when charging' in ability_lower:
        condition = '_CHARGING'
        ability_lower = ability_lower.replace(' when charging', '')
    elif ' when attacking' in ability_lower:
        condition = '_ATTACKING'
        ability_lower = ability_lower.replace(' when attacking', '')

    # Handle stat modifiers like "+1 to hit rolls"
    stat_match = re.match(r'([+-]\d+) to (hit|defense|morale|casting)( test)? rolls?', ability_lower)
    if stat_match:
        modifier = stat_match.group(1)
        stat = stat_match.group(2).upper()
        return f'{stat}_{modifier}{condition}'

    # Handle "+6" range when shooting"
    range_match = re.match(r'([+-]\d+)["\'] range', ability_lower)
    if range_match:
        modifier = range_match.group(1)
        return f'RANGE_{modifier}{condition}'

    # Handle "AP (+1)" patterns
    ap_match = re.match(r'ap \(\+?(\d+)\)', ability_lower)
    if ap_match:
        ap_val = ap_match.group(1)
        return f'AP_+{ap_val}{condition}'

    # Handle boost abilities (e.g., "Clan Warrior Boost" -> "CLAN_WARRIOR_BOOST")
    # Keep the full name for boost abilities since they're faction-specific
    if ' boost' in ability_lower:
        name = ability_lower.replace(' ', '_').upper()
        return f'{name}{condition}'

    # Standard ability: convert to uppercase, replace spaces with underscores
    ability_normalized = ability_lower.strip()
    # Handle '&' -> 'AND' before removing special chars
    ability_normalized = ability_normalized.replace('&', 'and')
    ability_normalized = re.sub(r'\s+', '_', ability_normalized)
    ability_normalized = re.sub(r'[^a-z0-9_+-]', '', ability_normalized)

    return f'{ability_normalized.upper()}{condition}'


def extract_grant_bucket(text: str, grant_type: str) -> str:
    """
    Extract a bucket name for rules that grant abilities.

    Args:
        text: The rule text (lowercase)
        grant_type: 'AURA', 'BUFF_SPELL', or 'MARK'

    Returns:
        Bucket name like 'AURA_GRANT_BANE_MELEE' or 'BUFF_SPELL_GRANT_FEARLESS'
    """
    text_lower = text.lower()

    # Pattern 1: "This model and its unit get X" (aura)
    m = re.search(r'this model and its unit gets? ([^.]+?)\.?$', text_lower)
    if m:
        ability = m.group(1).strip()
        normalized = normalize_ability_name(ability)
        return f'{grant_type}_GRANT_{normalized}'

    # Pattern 2: "which gets X once" or "which get X once" (buff spell)
    m = re.search(r'which gets? ([^.]+?)(?: once|\.)', text_lower)
    if m:
        ability = m.group(1).strip()
        # Check if this is a mark (grants bonus AGAINST enemy)
        if ' against' in ability:
            ability = ability.replace(' against', '')
            normalized = normalize_ability_name(ability)
            return f'MARK_GRANT_{normalized}'
        normalized = normalize_ability_name(ability)
        return f'{grant_type}_GRANT_{normalized}'

    # Pattern 3: "friendly units gets X against" (mark)
    m = re.search(r'friendly units? gets? ([^.]+?) against', text_lower)
    if m:
        ability = m.group(1).strip()
        normalized = normalize_ability_name(ability)
        return f'MARK_GRANT_{normalized}'

    # Pattern 4: "which moves +X" when using Y" (movement buff spell)
    m = re.search(r'which moves? \+(\d+)["\']?\s*when using (\w+)', text_lower)
    if m:
        bonus = m.group(1)
        action = m.group(2).upper()
        return f'{grant_type}_GRANT_MOVE_+{bonus}_{action}'

    return f'{grant_type}_GRANT_UNKNOWN'


def extract_enemy_effect_bucket(text: str) -> str:
    """
    Extract a normalized effect bucket for enemy-targeting rules.
    Groups rules by range + effect, ignoring target count (one vs two vs three).
    """
    text_lower = text.lower()

    # Extract range
    range_match = re.search(r'within (\d+)["\']', text_lower)
    range_val = range_match.group(1) if range_match else '0'

    # Extract effect (everything after "which")
    effect_match = re.search(r'which (.+)$', text_lower)
    if not effect_match:
        return f'ENEMY_R{range_val}_UNKNOWN'

    effect = effect_match.group(1).strip()

    # Normalize: remove "once (next time...)" suffix
    effect = re.sub(r'\s*once\s*\(next time.*$', '', effect)
    effect = effect.strip().rstrip('.')

    # Categorize by effect type
    if 'friendly units gets' in effect or 'friendly unit gets' in effect:
        # Mark spell - extract what friendly units get (full ability with condition)
        ability_match = re.search(r'friendly units? gets? ([^.]+?) against', effect)
        if ability_match:
            ability = ability_match.group(1).strip()
            normalized = normalize_ability_name(ability)
            return f'MARK_GRANT_{normalized}'
        # Fallback to old behavior
        ability_match = re.search(r'friendly units? gets? (\w+)', effect)
        ability = ability_match.group(1).upper() if ability_match else 'BONUS'
        return f'MARK_GRANT_{ability}'

    if 'takes' in effect and 'hit' in effect:
        # Damage spell - extract hit count and modifiers
        hit_match = re.search(r'takes? (\d+) hits?', effect)
        hits = hit_match.group(1) if hit_match else '?'

        # Extract AP if present
        ap_match = re.search(r'ap\s*\((\d+)\)', effect)
        ap = f'_AP{ap_match.group(1)}' if ap_match else ''

        # Extract special effects
        specials = []
        if 'blast' in effect:
            blast_match = re.search(r'blast\s*\((\d+)\)', effect)
            specials.append(f'BLAST{blast_match.group(1)}' if blast_match else 'BLAST')
        if 'deadly' in effect:
            deadly_match = re.search(r'deadly\s*\((\d+)\)', effect)
            specials.append(f'DEADLY{deadly_match.group(1)}' if deadly_match else 'DEADLY')
        if 'shred' in effect:
            specials.append('SHRED')
        if 'smash' in effect:
            specials.append('SMASH')
        if 'demolish' in effect:
            specials.append('DEMOLISH')
        if 'surge' in effect:
            specials.append('SURGE')

        special_str = '_' + '_'.join(specials) if specials else ''
        return f'DMG_ENEMY_R{range_val}_{hits}HIT{ap}{special_str}'

    if 'take' in effect and 'hits each' in effect:
        # Multi-target damage - "take X hits each"
        hit_match = re.search(r'take (\d+) hits each', effect)
        hits = hit_match.group(1) if hit_match else '?'
        ap_match = re.search(r'ap\s*\((\d+)\)', effect)
        ap = f'_AP{ap_match.group(1)}' if ap_match else ''
        return f'DMG_ENEMY_R{range_val}_{hits}HIT_EACH{ap}'

    if 'get -' in effect or 'gets -' in effect:
        # Debuff spell - extract what stat is reduced
        if 'hit roll' in effect:
            return f'DEBUFF_ENEMY_R{range_val}_HIT-1'
        if 'defense roll' in effect:
            return f'DEBUFF_ENEMY_R{range_val}_DEF-1'
        if 'casting roll' in effect:
            return f'DEBUFF_ENEMY_R{range_val}_CAST-1'
        return f'DEBUFF_ENEMY_R{range_val}_STAT'

    if 'difficult terrain' in effect or 'counts as being in difficult' in effect:
        return f'DEBUFF_ENEMY_R{range_val}_DIFFICULT_TERRAIN'

    # Fallback: hash the effect
    import hashlib
    effect_hash = hashlib.sha1(effect.encode()).hexdigest()[:4].upper()
    return f'ENEMY_R{range_val}_{effect_hash}'


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
        bucket = extract_grant_bucket(text, 'BUFF_SPELL')
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.FRIENDLY_UNIT,
            effect_type=EffectType.GRANT_ABILITY,
            timing=timing,
            should_ignore=True,
            ignore_reason='Buff spell targeting friendly units',
            effect_bucket=bucket,
            raw_text=text,
            range_inches=range_inches,
            granted_ability=granted_ability
        )

    # === PATTERN: Pick enemy unit - GROUP BY RANGE + EFFECT ===
    # These are NOT ignored - they are grouped by their actual effect
    if re.search(r'pick .* enemy', text_lower):
        effect_bucket = extract_enemy_effect_bucket(text)

        # Determine effect type
        if 'friendly units gets' in text_lower or 'friendly unit gets' in text_lower:
            effect_type = EffectType.MARK_ENEMY
        elif 'takes' in text_lower and 'hit' in text_lower:
            effect_type = EffectType.DEAL_DAMAGE
        elif 'take' in text_lower and 'hits each' in text_lower:
            effect_type = EffectType.DEAL_DAMAGE
        elif 'get -' in text_lower or 'gets -' in text_lower:
            effect_type = EffectType.DEBUFF_STAT
        elif 'difficult terrain' in text_lower:
            effect_type = EffectType.DEBUFF_STAT
        else:
            effect_type = EffectType.OTHER

        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.ENEMY_UNIT,
            effect_type=effect_type,
            timing=timing,
            should_ignore=False,  # NOT ignored - kept for simulation
            ignore_reason='',
            effect_bucket=effect_bucket,
            raw_text=text,
            range_inches=range_inches
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
        m = re.search(r'this model and its unit gets? (.+?)\.?$', text_lower)
        ability = m.group(1).strip() if m else None
        bucket = extract_grant_bucket(text, 'AURA')
        return ParsedRule(
            rule_name=rule_name,
            target_type=TargetType.SELF_UNIT,
            effect_type=EffectType.GRANT_ABILITY,
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


def load_rules_from_xlsx(xlsx_path: str) -> dict:
    """
    Load rules from an Excel file.

    Args:
        xlsx_path: Path to the Excel file (e.g., 'Faction Specific Army Rules.xlsx')

    Returns:
        Dict mapping rule_name -> (rule_text, once_per_game, once_per_activation)
    """
    import openpyxl

    wb = openpyxl.load_workbook(xlsx_path, data_only=True)
    ws = wb['Sheet1']
    rows = list(ws.iter_rows(values_only=True))

    rules = {}
    for r in rows[1:]:
        name = r[2]
        text = r[3]
        once_per_game = bool(r[5]) if len(r) > 5 else False
        once_per_activation = bool(r[6]) if len(r) > 6 else False
        if name and text and name not in rules:
            rules[name] = (text, once_per_game, once_per_activation)

    return rules


def parse_all_rules(rules: dict) -> dict:
    """
    Parse all rules from the loaded dictionary.

    Args:
        rules: Dict from load_rules_from_xlsx()

    Returns:
        Dict mapping rule_name -> ParsedRule
    """
    parsed = {}
    for name, (text, opg, opa) in rules.items():
        parsed[name] = parse_rule(name, text, opg, opa)
    return parsed


def export_to_csv(parsed_rules: dict, output_path: str) -> None:
    """
    Export parsed rules to a CSV file.

    Args:
        parsed_rules: Dict from parse_all_rules()
        output_path: Path for the output CSV file
    """
    import csv

    with open(output_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([
            'rule_name',
            'effect_bucket',
            'should_ignore',
            'ignore_reason',
            'target_type',
            'effect_type',
            'timing',
            'bonus_value',
            'range_inches',
            'granted_ability',
            'raw_text'
        ])

        for name, p in sorted(parsed_rules.items()):
            writer.writerow([
                p.rule_name,
                p.effect_bucket,
                p.should_ignore,
                p.ignore_reason,
                p.target_type.name,
                p.effect_type.name,
                p.timing.name,
                p.bonus_value if p.bonus_value is not None else '',
                p.range_inches if p.range_inches is not None else '',
                p.granted_ability or '',
                p.raw_text
            ])

    print(f"Exported {len(parsed_rules)} rules to {output_path}")


def print_summary(parsed_rules: dict) -> None:
    """Print a summary of parsed rules."""
    from collections import Counter

    ignored = sum(1 for p in parsed_rules.values() if p.should_ignore)

    print(f"Total rules: {len(parsed_rules)}")
    print(f"Rules to IGNORE: {ignored}")
    print(f"Rules to KEEP: {len(parsed_rules) - ignored}")
    print()

    # Show grant buckets by type
    print("=== AURA GRANT BUCKETS ===")
    aura_grants = Counter(p.effect_bucket for p in parsed_rules.values()
                          if p.effect_bucket.startswith('AURA_GRANT_'))
    for bucket, count in aura_grants.most_common(10):
        print(f"  {bucket}: {count}")

    print()
    print("=== BUFF SPELL GRANT BUCKETS (ignored) ===")
    buff_grants = Counter(p.effect_bucket for p in parsed_rules.values()
                          if p.effect_bucket.startswith('BUFF_SPELL_GRANT_'))
    for bucket, count in buff_grants.most_common(10):
        print(f"  {bucket}: {count}")

    print()
    print("=== MARK GRANT BUCKETS ===")
    mark_grants = Counter(p.effect_bucket for p in parsed_rules.values()
                          if p.effect_bucket.startswith('MARK_GRANT_'))
    for bucket, count in mark_grants.most_common(10):
        print(f"  {bucket}: {count}")

    print()
    print("=== TOP OTHER EFFECT BUCKETS (kept) ===")
    kept_buckets = Counter(p.effect_bucket for p in parsed_rules.values()
                           if not p.should_ignore
                           and not p.effect_bucket.startswith('AURA_GRANT_')
                           and not p.effect_bucket.startswith('MARK_GRANT_'))
    for bucket, count in kept_buckets.most_common(15):
        print(f"  {bucket}: {count}")


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(
        description='Parse special rules from Excel and optionally export to CSV',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Parse and show summary only
  python special_rule_parser.py rules.xlsx

  # Parse and export to CSV
  python special_rule_parser.py rules.xlsx --export output.csv

  # Export only (no summary)
  python special_rule_parser.py rules.xlsx --export output.csv --quiet
'''
    )
    parser.add_argument('xlsx_file', help='Path to the Excel file with special rules')
    parser.add_argument('--export', '-e', metavar='CSV_FILE',
                        help='Export parsed rules to CSV file')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Suppress summary output (useful with --export)')

    args = parser.parse_args()

    # Load and parse
    rules = load_rules_from_xlsx(args.xlsx_file)
    parsed = parse_all_rules(rules)

    # Export if requested
    if args.export:
        export_to_csv(parsed, args.export)

    # Print summary unless quiet
    if not args.quiet:
        print_summary(parsed)
