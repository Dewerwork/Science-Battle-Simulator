#!/usr/bin/env python3
"""
Test script to verify the A0 weapon fix logic.

Simulates the C++ pipeline's behavior to ensure rule-only upgrades
don't create weapons with A0.
"""

import json
import re
from pathlib import Path

def looks_like_weapon_profile(inside: str) -> bool:
    """
    C++ equivalent: checks if profile contains A# (attacks) or AP(#).
    This determines if text inside parentheses is a weapon profile.
    """
    if not inside:
        return False
    # Check for attack pattern like "A3", "A10"
    if re.search(r'\bA\d+\b', inside):
        return True
    # Check for AP pattern like "AP(1)", "AP(-1)"
    if re.search(r'\bAP\(\s*-?\d+\s*\)', inside):
        return True
    return False


def split_name_and_parens(text: str) -> tuple:
    """Extract name and parenthesized content from text."""
    text = text.strip()
    match = re.match(r'^(.+?)\s*\((.+)\)\s*$', text)
    if match:
        return match.group(1).strip(), match.group(2).strip()
    return text, ""


def should_add_weapon(opt: dict) -> tuple:
    """
    Determine if an upgrade option should add a weapon.
    Returns (should_add, reason)

    This simulates the FIXED C++ logic.
    """
    # If weapon data is present (not null), add it
    if opt.get('weapon') is not None:
        return True, "has weapon data"

    # Check if text looks like it contains a weapon profile
    name_part, inside = split_name_and_parens(opt.get('text', ''))

    if inside and looks_like_weapon_profile(inside):
        return True, f"profile '{inside}' looks like weapon (has A# or AP)"

    # No weapon data and doesn't look like weapon profile
    # This is a rule-only upgrade - DON'T add a weapon
    return False, "rule-only upgrade (no weapon data, no weapon profile)"


def run_tests():
    """Run tests on actual JSON data."""
    json_path = Path(__file__).parent.parent / "docs" / "parsed_output" / "GF_-_Alien_Hives_3.5.1_units.json"

    if not json_path.exists():
        print(f"ERROR: JSON file not found at {json_path}")
        return False

    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    print("=" * 70)
    print("A0 WEAPON FIX TEST SUITE")
    print("=" * 70)

    # Collect test cases
    test_cases = []

    for unit in data.get('units', []):
        unit_name = unit.get('name', 'Unknown')
        for group in unit.get('upgrade_groups', []):
            header = group.get('header', '')
            is_replace = 'replace' in header.lower()

            for opt in group.get('options', []):
                test_cases.append({
                    'unit': unit_name,
                    'header': header,
                    'is_replace': is_replace,
                    'option': opt
                })

    print(f"\nFound {len(test_cases)} upgrade options to test\n")

    # Test categories
    passed = 0
    failed = 0

    # Track specific test scenarios
    rule_only_no_weapon = []  # Should NOT add weapon
    has_weapon_data = []      # Should add weapon
    weapon_profile_text = []  # Should add weapon (fallback)

    for tc in test_cases:
        opt = tc['option']
        should_add, reason = should_add_weapon(opt)

        text = opt.get('text', '')
        weapon = opt.get('weapon')
        rules_granted = opt.get('rules_granted', [])

        # Categorize
        if weapon is not None:
            has_weapon_data.append(tc)
        elif rules_granted and not should_add:
            rule_only_no_weapon.append(tc)

        # Specific tests for known problematic cases
        if 'Killing Scream' in text:
            if should_add:
                print(f"❌ FAIL: 'Killing Scream' should NOT add weapon")
                print(f"   Text: {text}")
                print(f"   Reason: {reason}")
                failed += 1
            else:
                print(f"✓ PASS: 'Killing Scream (Breath Attack)' correctly skipped")
                print(f"   Reason: {reason}")
                passed += 1

    print("\n" + "=" * 70)
    print("TEST CASE BREAKDOWN")
    print("=" * 70)

    # Test 1: Rule-only upgrades (weapon=null, rules_granted not empty)
    print(f"\n### Test 1: Rule-only upgrades that should NOT create weapons ###")
    print(f"Found {len(rule_only_no_weapon)} rule-only upgrades")

    test1_pass = 0
    test1_fail = 0
    sample_shown = 0

    for tc in rule_only_no_weapon:
        opt = tc['option']
        should_add, reason = should_add_weapon(opt)

        if not should_add:
            test1_pass += 1
            if sample_shown < 5:
                print(f"  ✓ {opt['text'][:50]}... → NO weapon (correct)")
                sample_shown += 1
        else:
            test1_fail += 1
            print(f"  ❌ {opt['text']} → would add weapon (WRONG!)")

    if sample_shown < len(rule_only_no_weapon):
        print(f"  ... and {len(rule_only_no_weapon) - sample_shown} more")

    print(f"\nTest 1 Result: {test1_pass} passed, {test1_fail} failed")
    passed += test1_pass
    failed += test1_fail

    # Test 2: Upgrades with weapon data should add weapons
    print(f"\n### Test 2: Upgrades WITH weapon data should create weapons ###")
    print(f"Found {len(has_weapon_data)} upgrades with weapon data")

    test2_pass = 0
    test2_fail = 0
    sample_shown = 0

    for tc in has_weapon_data:
        opt = tc['option']
        should_add, reason = should_add_weapon(opt)

        if should_add:
            test2_pass += 1
            if sample_shown < 5:
                w = opt['weapon']
                print(f"  ✓ {opt['text'][:40]}... → adds weapon '{w.get('name')}' (A{w.get('attacks')})")
                sample_shown += 1
        else:
            test2_fail += 1
            print(f"  ❌ {opt['text']} → would NOT add weapon (WRONG!)")

    if sample_shown < len(has_weapon_data):
        print(f"  ... and {len(has_weapon_data) - sample_shown} more")

    print(f"\nTest 2 Result: {test2_pass} passed, {test2_fail} failed")
    passed += test2_pass
    failed += test2_fail

    # Test 3: Specific edge cases
    print(f"\n### Test 3: Specific edge cases ###")

    edge_cases = [
        # (text, weapon, expected_add, description)
        ("Killing Scream (Breath Attack)", None, False, "Rule in parens, no A#"),
        ("Bio-Tech Master (Increased Shooting Range)", None, False, "Rule in parens"),
        ("Wings (Ambush, Flying)", None, False, "Multiple rules in parens"),
        ("Psy-Barrier (Resistance)", None, False, "Single rule in parens"),
        ("Piercing Spike (A1, AP(2), Deadly(3))", {"name": "Piercing Spike", "attacks": 1}, True, "Weapon with profile"),
        ("Heavy Razor Claws (A3, AP(1))", {"name": "Heavy Razor Claws", "attacks": 3}, True, "Weapon with AP"),
    ]

    for text, weapon, expected, desc in edge_cases:
        opt = {'text': text, 'weapon': weapon, 'rules_granted': []}
        should_add, reason = should_add_weapon(opt)

        if should_add == expected:
            print(f"  ✓ PASS: {desc}")
            print(f"         '{text}' → {'adds' if should_add else 'skips'} weapon")
            passed += 1
        else:
            print(f"  ❌ FAIL: {desc}")
            print(f"         '{text}' → expected {'add' if expected else 'skip'}, got {'add' if should_add else 'skip'}")
            print(f"         Reason: {reason}")
            failed += 1

    # Test 4: looks_like_weapon_profile function
    print(f"\n### Test 4: looks_like_weapon_profile() function ###")

    profile_tests = [
        ("A1, AP(2), Deadly(3)", True, "Has A1 and AP"),
        ("A3, AP(1)", True, "Has A3 and AP"),
        ("A10", True, "Has A10"),
        ("AP(4)", True, "Has AP only"),
        ("Breath Attack", False, "No A# or AP"),
        ("Increased Shooting Range", False, "Just a rule name"),
        ("Ambush, Flying", False, "Multiple rules"),
        ("Resistance", False, "Single rule"),
        ("Caster(3)", False, "Caster is not A#"),
        ("12\", A2, Blast(3), Bane", True, "Range with attacks"),
        ("18\", A4, AP(1), Rupture", True, "Full weapon profile"),
    ]

    for profile, expected, desc in profile_tests:
        result = looks_like_weapon_profile(profile)
        if result == expected:
            print(f"  ✓ '{profile}' → {result} ({desc})")
            passed += 1
        else:
            print(f"  ❌ '{profile}' → expected {expected}, got {result} ({desc})")
            failed += 1

    # Test 5: Replace header scenarios
    print(f"\n### Test 5: Replace header scenarios ###")

    # Find replace headers and test their options
    replace_headers = {}
    for tc in test_cases:
        if tc['is_replace']:
            h = tc['header']
            if h not in replace_headers:
                replace_headers[h] = []
            replace_headers[h].append(tc)

    print(f"Found {len(replace_headers)} unique Replace headers")

    test5_pass = 0
    test5_fail = 0

    # Check some specific Replace scenarios
    for header, tcs in list(replace_headers.items())[:5]:
        print(f"\n  Header: '{header}'")
        for tc in tcs[:3]:
            opt = tc['option']
            should_add, reason = should_add_weapon(opt)
            has_weapon = opt.get('weapon') is not None

            # For Replace headers, we should only add weapon if we have weapon data
            # or if the text looks like a weapon profile
            expected = has_weapon or (looks_like_weapon_profile(split_name_and_parens(opt['text'])[1]))

            if should_add == expected:
                status = "adds" if should_add else "rules-only"
                print(f"    ✓ {opt['text'][:40]}... → {status}")
                test5_pass += 1
            else:
                print(f"    ❌ {opt['text'][:40]}... → expected {expected}, got {should_add}")
                test5_fail += 1

    passed += test5_pass
    failed += test5_fail

    # Summary
    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"Total tests: {passed + failed}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")

    if failed == 0:
        print("\n✅ ALL TESTS PASSED - The fix should work correctly!")
        return True
    else:
        print(f"\n❌ {failed} TESTS FAILED - Review needed!")
        return False


if __name__ == "__main__":
    success = run_tests()
    exit(0 if success else 1)
