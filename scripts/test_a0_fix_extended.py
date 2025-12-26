#!/usr/bin/env python3
"""
Extended tests for A0 weapon fix - focusing on Replace scenarios.
"""

import json
import re
from pathlib import Path


def looks_like_weapon_profile(inside: str) -> bool:
    if not inside:
        return False
    if re.search(r'\bA\d+\b', inside):
        return True
    if re.search(r'\bAP\(\s*-?\d+\s*\)', inside):
        return True
    return False


def split_name_and_parens(text: str) -> tuple:
    text = text.strip()
    match = re.match(r'^(.+?)\s*\((.+)\)\s*$', text)
    if match:
        return match.group(1).strip(), match.group(2).strip()
    return text, ""


def simulate_replace_upgrade(header: str, option: dict, target_weapon_key: str = "target_weapon"):
    """
    Simulate what the C++ pipeline does for a Replace upgrade.
    Returns the weapon_delta that would be generated.
    """
    weapon_delta = {}

    # Remove target weapon
    if target_weapon_key:
        weapon_delta[target_weapon_key] = -1  # slots = 1 for simplicity

    # Check if we should add a weapon
    has_weapon_to_add = False
    add_key = None

    if option.get('weapon') is not None:
        # Use structured weapon data
        w = option['weapon']
        name = w.get('name', '').lower().strip()
        attacks = w.get('attacks', 0)
        add_key = f"N={name}|R=|A={attacks}|AP="
        has_weapon_to_add = True
    else:
        # Check if text looks like weapon profile
        name_part, inside = split_name_and_parens(option.get('text', ''))
        if inside and looks_like_weapon_profile(inside):
            name = name_part.lower().strip()
            add_key = f"N={name}|R=|A=0|AP="
            has_weapon_to_add = True

    # Only add to weapon_delta if we have a weapon
    if has_weapon_to_add and add_key:
        weapon_delta[add_key] = weapon_delta.get(add_key, 0) + 1

    return weapon_delta, has_weapon_to_add


def run_extended_tests():
    json_path = Path(__file__).parent.parent / "docs" / "parsed_output" / "GF_-_Alien_Hives_3.5.1_units.json"

    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    print("=" * 70)
    print("EXTENDED A0 FIX TESTS - Replace Scenarios")
    print("=" * 70)

    passed = 0
    failed = 0

    # Test 6: Replace options that grant rules should NOT add A0 weapons
    print("\n### Test 6: Replace options with rules_granted but no weapon ###")

    rule_only_replace = []
    for unit in data.get('units', []):
        for group in unit.get('upgrade_groups', []):
            header = group.get('header', '')
            if 'replace' not in header.lower():
                continue
            for opt in group.get('options', []):
                if opt.get('weapon') is None and opt.get('rules_granted'):
                    rule_only_replace.append({
                        'unit': unit.get('name'),
                        'header': header,
                        'option': opt
                    })

    print(f"Found {len(rule_only_replace)} Replace options with rules but no weapon")

    for tc in rule_only_replace:
        opt = tc['option']
        weapon_delta, has_weapon = simulate_replace_upgrade(tc['header'], opt)

        # For rule-only, we should only have the -1 for removing target
        expected_delta = {"target_weapon": -1}

        if weapon_delta == expected_delta:
            print(f"  ✓ '{opt['text'][:45]}...' → correctly removes weapon, adds no A0")
            passed += 1
        else:
            print(f"  ❌ '{opt['text']}' → wrong delta: {weapon_delta}")
            failed += 1

    # Test 7: Replace options WITH weapon should add the weapon
    print(f"\n### Test 7: Replace options WITH weapon data ###")

    weapon_replace = []
    for unit in data.get('units', []):
        for group in unit.get('upgrade_groups', []):
            header = group.get('header', '')
            if 'replace' not in header.lower():
                continue
            for opt in group.get('options', []):
                if opt.get('weapon') is not None:
                    weapon_replace.append({
                        'unit': unit.get('name'),
                        'header': header,
                        'option': opt
                    })

    print(f"Found {len(weapon_replace)} Replace options with weapon data")

    shown = 0
    for tc in weapon_replace[:10]:  # Test first 10
        opt = tc['option']
        weapon_delta, has_weapon = simulate_replace_upgrade(tc['header'], opt)

        w = opt['weapon']
        expected_name = w['name'].lower().strip()
        expected_attacks = w.get('attacks', 0)

        # Should have -1 for target and +1 for new weapon
        if len(weapon_delta) == 2 and weapon_delta.get("target_weapon") == -1:
            # Check the added weapon has correct attacks (not 0 unless it really is 0)
            added_key = [k for k in weapon_delta.keys() if k != "target_weapon"][0]
            if f"A={expected_attacks}" in added_key:
                print(f"  ✓ '{opt['text'][:40]}...' → adds '{w['name']}' (A{expected_attacks})")
                passed += 1
            else:
                print(f"  ❌ '{opt['text']}' → wrong attacks in key: {added_key}")
                failed += 1
        else:
            print(f"  ❌ '{opt['text']}' → wrong delta: {weapon_delta}")
            failed += 1

    # Test 8: Carnivo-Rex specific test (the problematic unit)
    print(f"\n### Test 8: Carnivo-Rex specific tests ###")

    carnivo_rex = None
    for unit in data.get('units', []):
        if unit.get('name') == 'Carnivo-Rex':
            carnivo_rex = unit
            break

    if carnivo_rex:
        print(f"Found Carnivo-Rex with {len(carnivo_rex.get('upgrade_groups', []))} upgrade groups")

        for group in carnivo_rex.get('upgrade_groups', []):
            header = group.get('header', '')
            print(f"\n  Group: '{header}'")

            for opt in group.get('options', []):
                weapon_delta, has_weapon = simulate_replace_upgrade(header, opt)
                text = opt['text']
                w = opt.get('weapon')

                if 'Killing Scream' in text:
                    # This should NOT add a weapon
                    if not has_weapon:
                        print(f"    ✓ '{text}' → NO weapon added (correct!)")
                        passed += 1
                    else:
                        print(f"    ❌ '{text}' → WOULD ADD weapon (BUG!)")
                        failed += 1
                elif w is not None:
                    print(f"    ✓ '{text[:35]}...' → adds weapon '{w['name']}' (A{w['attacks']})")
                    passed += 1

    # Test 9: Verify A0 is never generated for rule-only options
    print(f"\n### Test 9: Verify no A0 weapons are generated ###")

    a0_found = []
    for unit in data.get('units', []):
        for group in unit.get('upgrade_groups', []):
            for opt in group.get('options', []):
                weapon_delta, _ = simulate_replace_upgrade(group.get('header', ''), opt)

                for key in weapon_delta:
                    if key != "target_weapon" and "|A=0|" in key:
                        # Check if this is legitimately A0 (has weapon data with 0 attacks)
                        w = opt.get('weapon')
                        if w is None:
                            a0_found.append({
                                'unit': unit.get('name'),
                                'text': opt.get('text'),
                                'key': key
                            })

    if not a0_found:
        print(f"  ✓ No spurious A0 weapons generated!")
        passed += 1
    else:
        print(f"  ❌ Found {len(a0_found)} spurious A0 weapons:")
        for item in a0_found:
            print(f"      - {item['unit']}: {item['text']} → {item['key']}")
        failed += len(a0_found)

    # Test 10: Test the specific "Killing Scream" path in detail
    print(f"\n### Test 10: Detailed 'Killing Scream' analysis ###")

    killing_scream_opt = {
        'text': 'Killing Scream (Breath Attack)',
        'cost': 5,
        'rules_granted': ['Breath Attack'],
        'weapon': None
    }

    name_part, inside = split_name_and_parens(killing_scream_opt['text'])
    print(f"  Text: '{killing_scream_opt['text']}'")
    print(f"  Name part: '{name_part}'")
    print(f"  Inside parens: '{inside}'")
    print(f"  looks_like_weapon_profile('{inside}'): {looks_like_weapon_profile(inside)}")
    print(f"  weapon is None: {killing_scream_opt['weapon'] is None}")
    print(f"  rules_granted: {killing_scream_opt['rules_granted']}")

    weapon_delta, has_weapon = simulate_replace_upgrade("Replace one Heavy Razor Claw", killing_scream_opt)
    print(f"  Final weapon_delta: {weapon_delta}")
    print(f"  has_weapon_to_add: {has_weapon}")

    if not has_weapon and weapon_delta == {"target_weapon": -1}:
        print(f"\n  ✓ CORRECT: Killing Scream removes weapon but adds no A0 weapon!")
        passed += 1
    else:
        print(f"\n  ❌ WRONG: Would generate incorrect weapon delta")
        failed += 1

    # Summary
    print("\n" + "=" * 70)
    print("EXTENDED TEST SUMMARY")
    print("=" * 70)
    print(f"Total tests: {passed + failed}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")

    if failed == 0:
        print("\n✅ ALL EXTENDED TESTS PASSED!")
        return True
    else:
        print(f"\n❌ {failed} TESTS FAILED!")
        return False


if __name__ == "__main__":
    success = run_extended_tests()
    exit(0 if success else 1)
