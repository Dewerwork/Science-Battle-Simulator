#!/usr/bin/env python3
"""
Create effect-based supergrouping CSV for special rules.

Groups rules by their actual mechanical effect, not just category.
Rules with identical effects get the same effect_bucket.
"""

import openpyxl
from collections import defaultdict
import re
import csv
import hashlib

wb = openpyxl.load_workbook('Faction Specific Army Rules.xlsx', data_only=True)
ws = wb['Sheet1']
rows = list(ws.iter_rows(values_only=True))

def normalize_text(t):
    """Fix encoding issues and normalize whitespace."""
    if not t:
        return ''
    t = str(t)
    t = t.replace('â€™', "'").replace('â€œ', '"').replace('â€', '"')
    t = t.replace('â€"', '-').replace('â€"', '-')
    t = re.sub(r'\s+', ' ', t)
    return t.strip()

def effect_hash(text):
    """Generate short hash for effect grouping."""
    return hashlib.sha1(text.lower().encode()).hexdigest()[:6].upper()

# Collect all rules
rules_data = {}
for r in rows[1:]:
    rule_name = r[2]
    rule_text = normalize_text(r[3])
    original_type = r[4] if r[4] else ''
    once_per_game = bool(r[5])
    once_per_activation = bool(r[6])

    if not rule_name or not rule_text:
        continue

    rule_name = rule_name.strip()
    if rule_name not in rules_data:
        rules_data[rule_name] = {
            'text': rule_text,
            'text_lower': rule_text.lower(),
            'original_type': original_type,
            'once_per_game': once_per_game,
            'once_per_activation': once_per_activation
        }

# ============================================================
# EFFECT PATTERN MATCHING
# ============================================================

def classify_effect(name, text_lower, text, once_per_game):
    """
    Classify a rule by its mechanical effect.
    Returns (effect_bucket, should_ignore, ignore_reason)
    """

    # === RULES TO IGNORE ===

    # Once per game - ignore
    if once_per_game or 'once per game' in text_lower:
        return ('ONCE_PER_GAME', True, 'Once per game effect')

    # "Pick X friendly units within Y" - these are buff spells, ignore
    if re.search(r'pick (one|up to \w+) friendly unit', text_lower):
        return ('PICK_FRIENDLY_UNIT_BUFF', True, 'Targets friendly units (buff spell)')

    # "Pick X enemy units within Y" that deal hits - these are damage spells
    if re.search(r'pick .* enemy .* which takes? \d+ hit', text_lower):
        return ('ENEMY_DAMAGE_SPELL', True, 'Damage spell targeting enemies')

    # === MOVEMENT EFFECTS ===

    # +X" to advance/rush/charge
    m = re.search(r'moves?\s*\+(\d+)["\']?\s*when using (advance|rush|charge)', text_lower)
    if m:
        bonus = m.group(1)
        action = m.group(2)
        return (f'MOVE_+{bonus}_ON_{action.upper()}', False, None)

    # Generic movement bonus patterns
    if re.search(r'moves?\s*\+(\d+)["\']', text_lower):
        # Try to extract the pattern
        m = re.search(r'moves?\s*\+(\d+)["\']', text_lower)
        if '+1"' in text_lower and 'advance' in text_lower and '+2"' in text_lower and 'rush' in text_lower:
            return ('MOVE_+1_ADV_+2_RUSH_CHARGE', False, None)
        if '+2"' in text_lower and 'advance' in text_lower and '+2"' in text_lower:
            return ('MOVE_+2_ADV_+2_RUSH_CHARGE', False, None)
        if '+4"' in text_lower and 'charge' in text_lower:
            return ('MOVE_+4_CHARGE', False, None)

    # === 6 TO HIT EFFECTS ===

    if re.search(r'(unmodified )?(roll|result).* of 6.* to hit', text_lower):
        if '+1 attack' in text_lower and 'melee' in text_lower:
            return ('ON_6_TO_HIT_+1_ATTACK_MELEE', False, None)
        if '+1 attack' in text_lower:
            return ('ON_6_TO_HIT_+1_ATTACK_ANY', False, None)
        if 'extra hit' in text_lower and 'melee' in text_lower:
            return ('ON_6_TO_HIT_EXTRA_HIT_MELEE', False, None)
        if 'extra hit' in text_lower:
            return ('ON_6_TO_HIT_EXTRA_HIT_ANY', False, None)
        if 'ap (+2)' in text_lower:
            return ('ON_6_TO_HIT_AP+2', False, None)
        if 'ap (+4)' in text_lower:
            return ('ON_6_TO_HIT_AP+4', False, None)
        if 'extra wound' in text_lower or 'deals 1 extra wound' in text_lower:
            return ('ON_6_TO_HIT_EXTRA_WOUND', False, None)

    # === DEFENSE EFFECTS ===

    if 'shaken at the beginning of the round' in text_lower and 'roll one die' in text_lower:
        return ('RECOVER_FROM_SHAKEN_ON_4+', False, None)

    if re.search(r'takes? wounds?.* roll one die.* on a 6\+.* ignored', text_lower):
        return ('IGNORE_WOUND_ON_6+', False, None)

    if '+1 to defense rolls' in text_lower:
        return ('DEFENSE_+1', False, None)

    if 'regeneration' in text_lower and 'ignores' not in text_lower:
        return ('HAS_REGENERATION', False, None)

    if 'stealth' in text_lower and 'gets' not in text_lower and 'pick' not in text_lower:
        return ('HAS_STEALTH', False, None)

    # === WEAPON EFFECTS ===

    if 'ignores regeneration' in text_lower:
        if 'extra wound' in text_lower:
            return ('IGNORES_REGEN_+_EXTRA_WOUND', False, None)
        if 'ap (+2)' in text_lower:
            return ('IGNORES_REGEN_+_AP+2', False, None)
        return ('IGNORES_REGENERATION', False, None)

    if 'ignores cover' in text_lower:
        if 'extra hit' in text_lower:
            return ('IGNORES_COVER_+_EXTRA_HIT', False, None)
        if 'ap (+2)' in text_lower:
            return ('IGNORES_COVER_+_AP+2', False, None)
        return ('IGNORES_COVER', False, None)

    if '+1 to hit' in text_lower:
        if 'shooting' in text_lower:
            return ('HIT_+1_SHOOTING', False, None)
        if 'melee' in text_lower:
            return ('HIT_+1_MELEE', False, None)
        return ('HIT_+1_ANY', False, None)

    if '-1 to hit' in text_lower:
        return ('HIT_-1', False, None)

    if '+1 to morale' in text_lower:
        return ('MORALE_+1', False, None)

    # === AURA EFFECTS ===

    if 'this model and its unit get' in text_lower:
        # Extract what they get
        m = re.search(r'this model and its unit get (.+?)\.', text_lower)
        if m:
            effect = m.group(1).strip()
            # Normalize common effects
            effect_upper = effect.upper().replace(' ', '_').replace('+', 'PLUS')
            return (f'AURA_{effect_upper[:30]}', False, None)

    # === FALLBACK: Group by exact text ===
    return (f'EFFECT_{effect_hash(text_lower)}', False, None)


# ============================================================
# PROCESS ALL RULES
# ============================================================

# First pass: classify all rules
classified = {}
for rule_name, data in rules_data.items():
    bucket, ignore, reason = classify_effect(
        rule_name,
        data['text_lower'],
        data['text'],
        data['once_per_game']
    )
    classified[rule_name] = {
        'effect_bucket': bucket,
        'should_ignore': ignore,
        'ignore_reason': reason or '',
        'text': data['text'],
        'original_type': data['original_type'],
        'once_per_game': data['once_per_game'],
        'once_per_activation': data['once_per_activation']
    }

# Second pass: group rules with IDENTICAL text (regardless of pattern match)
text_to_rules = defaultdict(list)
for rule_name, data in rules_data.items():
    text_to_rules[data['text_lower']].append(rule_name)

# For rules with identical text, make sure they have the same bucket
for text_lower, rule_names in text_to_rules.items():
    if len(rule_names) > 1:
        # Use the first rule's bucket for all
        first_bucket = classified[rule_names[0]]['effect_bucket']
        for rn in rule_names[1:]:
            if classified[rn]['effect_bucket'].startswith('EFFECT_'):
                # Override hash-based bucket with the grouped one
                classified[rn]['effect_bucket'] = first_bucket

# ============================================================
# WRITE CSV
# ============================================================

with open('docs/special_rules_supergrouping.csv', 'w', newline='', encoding='utf-8') as f:
    writer = csv.writer(f)
    writer.writerow([
        'rule_name',
        'effect_bucket',
        'should_ignore',
        'ignore_reason',
        'original_type',
        'once_per_game',
        'once_per_activation',
        'effect_text'
    ])

    for rule_name in sorted(classified.keys(), key=str.lower):
        c = classified[rule_name]
        writer.writerow([
            rule_name,
            c['effect_bucket'],
            'YES' if c['should_ignore'] else 'NO',
            c['ignore_reason'],
            c['original_type'],
            'YES' if c['once_per_game'] else '',
            'YES' if c['once_per_activation'] else '',
            c['text'][:300]
        ])

# ============================================================
# SUMMARY
# ============================================================

print(f"Created docs/special_rules_supergrouping.csv with {len(classified)} rules")
print()

# Count by bucket
bucket_counts = defaultdict(list)
for rule_name, c in classified.items():
    bucket_counts[c['effect_bucket']].append(rule_name)

# Show non-ignored buckets with multiple rules
print("=== EFFECT BUCKETS WITH MULTIPLE RULES (Not Ignored) ===")
multi_rule_buckets = [(b, rules) for b, rules in bucket_counts.items()
                       if len(rules) > 1 and not classified[rules[0]]['should_ignore']]
multi_rule_buckets.sort(key=lambda x: -len(x[1]))

for bucket, rules in multi_rule_buckets[:20]:
    print(f"\n{bucket} ({len(rules)} rules):")
    for r in rules[:5]:
        print(f"  - {r}")
    if len(rules) > 5:
        print(f"  ... and {len(rules) - 5} more")

print()
print("=== IGNORED CATEGORIES ===")
ignored_counts = defaultdict(int)
for rule_name, c in classified.items():
    if c['should_ignore']:
        ignored_counts[c['effect_bucket']] += 1

for bucket, count in sorted(ignored_counts.items(), key=lambda x: -x[1]):
    print(f"  {bucket}: {count} rules")

print()
print(f"Total rules: {len(classified)}")
print(f"Rules to IGNORE: {sum(1 for c in classified.values() if c['should_ignore'])}")
print(f"Rules to KEEP: {sum(1 for c in classified.values() if not c['should_ignore'])}")
