#!/usr/bin/env python3
"""
Test script to validate the unit loadout builder correctness.
Run from the docs directory: python test_loadout_builder.py
"""

import sys
import os

# Import the pipeline functions
from run_opr_pipeline_all_units_v3_mt import (
    _extract_rules_from_choice,
    split_name_and_parens,
    looks_like_weapon_profile,
    weapon_key_from_profile,
    norm_name,
    normalize_rules_for_signature,
    build_base_weapon_multiset,
    group_variants,
    _format_weapon_for_display,
    _worker_raw_loadouts_range,
    _init_worker,
    build_stage1_signature,
)

def test_rule_extraction():
    """Test that rules are properly extracted from option text."""
    print("=" * 60)
    print("TEST: Rule Extraction from Options")
    print("=" * 60)

    test_cases = [
        # (input, expected_rules)
        ("Fearless +10pts", ["Fearless"]),  # Simple rule
        ("Veteran (Fear(2)) +15pts", ["Fear(2)"]),  # Rule with value
        ("Veteran (Fear(2), Relentless) +20pts", ["Fear(2)", "Relentless"]),  # Multi-rule - POTENTIAL BUG
        ("Plasma Gun (24\", A2, AP(1)) +10pts", []),  # Weapon profile - no rules
        ("Shield Wall +5pts", ["Shield Wall"]),  # Two-word rule
    ]

    all_passed = True
    for text, expected in test_cases:
        # Strip pts part for extraction
        text_no_pts = text.rsplit("+", 1)[0].strip().rstrip("Free").strip()
        actual = _extract_rules_from_choice(text_no_pts)

        # Note: expected multi-rule case might fail due to bug
        passed = actual == expected
        status = "PASS" if passed else "FAIL"

        print(f"\n{status}: '{text_no_pts}'")
        print(f"  Expected: {expected}")
        print(f"  Actual:   {actual}")

        if not passed:
            all_passed = False

    return all_passed


def test_weapon_key_generation():
    """Test that weapon keys are correctly generated."""
    print("\n" + "=" * 60)
    print("TEST: Weapon Key Generation")
    print("=" * 60)

    test_cases = [
        # (profile, weapon_name, expected_key_parts)
        # Note: weapon names are normalized (lowercased) for case-insensitive matching
        ('24", A4, AP(1), Rending', "Plasma Gun",
         {"N": "plasma gun", "R": "24", "A": "4", "AP": "1", "T": "Rending"}),
        ('A3, AP(2)', "Power Sword",
         {"N": "power sword", "R": "", "A": "3", "AP": "2"}),
        ('12", A1', "Pistol",
         {"N": "pistol", "R": "12", "A": "1", "AP": ""}),
        ('A6, Rending, Reliable', "Claws",
         {"N": "claws", "R": "", "A": "6", "AP": "", "T": "Reliable;Rending"}),  # Tags sorted
    ]

    all_passed = True
    for profile, name, expected_parts in test_cases:
        key, rng, attacks, ap, tags = weapon_key_from_profile(profile, name)

        # Parse the key back
        actual_parts = {}
        for part in key.split("|"):
            if "=" in part:
                k, v = part.split("=", 1)
                actual_parts[k] = v

        passed = True
        for k, v in expected_parts.items():
            if actual_parts.get(k) != v:
                passed = False
                break

        status = "PASS" if passed else "FAIL"
        print(f"\n{status}: {name} with profile '{profile}'")
        print(f"  Generated key: {key}")
        if not passed:
            print(f"  Expected parts: {expected_parts}")
            print(f"  Actual parts:   {actual_parts}")
            all_passed = False

    return all_passed


def test_self_replacement_detection():
    """Test that self-replacement is correctly detected (case sensitivity issue)."""
    print("\n" + "=" * 60)
    print("TEST: Self-Replacement Detection (Case Sensitivity)")
    print("=" * 60)

    # Create a mock unit with a weapon
    unit = {
        "name": "Test Unit",
        "size": 1,
        "base_points": 100,
        "quality": 4,
        "defense": 4,
        "special_rules": [],
        "weapons": [
            {"name": "Heavy Sword", "count": 1, "range": "-", "attacks": 3, "ap": 1, "special": []}
        ],
        "options": []
    }

    base_w, name_to_key = build_base_weapon_multiset(unit)

    # Check if "heavy sword" (lowercase) maps to the same key as "Heavy Sword"
    target_name_1 = "Heavy Sword"
    target_name_2 = "heavy sword"
    target_name_3 = "Heavy sword"

    key_1 = name_to_key.get(norm_name(target_name_1), "NOT FOUND")
    key_2 = name_to_key.get(norm_name(target_name_2), "NOT FOUND")
    key_3 = name_to_key.get(norm_name(target_name_3), "NOT FOUND")

    print(f"\nBase weapon key mapping:")
    print(f"  norm_name('{target_name_1}') = '{norm_name(target_name_1)}'")
    print(f"  name_to_key lookup: {key_1}")

    print(f"\nAlternate case lookups:")
    print(f"  norm_name('{target_name_2}') = '{norm_name(target_name_2)}' -> {key_2}")
    print(f"  norm_name('{target_name_3}') = '{norm_name(target_name_3)}' -> {key_3}")

    # Generate a weapon key for an option with different case
    option_name = "Heavy sword"
    option_key = weapon_key_from_profile("A3, AP(1)", option_name)[0]

    print(f"\nOption weapon key for '{option_name}': {option_key}")
    print(f"Base weapon key: {key_1}")

    # They should match for self-replacement detection but won't due to case
    matches = (option_key == key_1)
    print(f"\nKeys match (for self-replacement detection): {matches}")

    if not matches:
        print("  WARNING: Case mismatch prevents self-replacement detection!")
        return False
    return True


def test_weapon_count_output():
    """Test that weapon counts are correctly output."""
    print("\n" + "=" * 60)
    print("TEST: Weapon Count Output Format")
    print("=" * 60)

    test_weapons = [
        {"name": "Rifle", "count": 1, "range": '24"', "attacks": 2, "ap": None, "special": []},
        {"name": "Sword", "count": 3, "range": "-", "attacks": 3, "ap": 1, "special": ["Rending"]},
        {"name": "Heavy Bolter", "count": 2, "range": '36"', "attacks": 4, "ap": 2, "special": ["Reliable"]},
    ]

    print("\nGenerated weapon display strings:")
    for w in test_weapons:
        display = _format_weapon_for_display(w)
        print(f"  {w['name']} (count={w['count']}): '{display}'")

        # Verify count prefix is present for count > 1
        if w['count'] > 1:
            expected_prefix = f"{w['count']}x "
            if not display.startswith(expected_prefix):
                print(f"    ERROR: Expected prefix '{expected_prefix}' not found!")
                return False

    return True


def test_rules_normalization():
    """Test that rules are properly cleaned and normalized."""
    print("\n" + "=" * 60)
    print("TEST: Rules Normalization")
    print("=" * 60)

    test_cases = [
        # (input_rules, expected_normalized)
        (["Fearless", "Fear(2)", "Fearless"], ("Fear(2)", "Fearless")),  # Deduplication
        (["fearless", "FEARLESS", "Fearless"], ("fearless",)),  # Case-insensitive dedup (first form kept)
        (["+20pts", "Relentless"], ("Relentless",)),  # Filter cost modifiers
        (["Casting(1)", "casting debu"], ("Casting(1)",)),  # Fix truncated rules
        (["Fear(2), Relentless"], ("Fear(2), Relentless",)),  # Single rule (not a multi-rule scenario)
    ]

    all_passed = True
    for rules_in, expected in test_cases:
        actual = normalize_rules_for_signature(rules_in)

        passed = actual == expected
        status = "PASS" if passed else "FAIL"

        print(f"\n{status}: {rules_in}")
        print(f"  Expected: {expected}")
        print(f"  Actual:   {actual}")

        if not passed:
            all_passed = False

    return all_passed


def test_signature_consistency():
    """Test that signatures are deterministic."""
    print("\n" + "=" * 60)
    print("TEST: Signature Consistency")
    print("=" * 60)

    # Build same signature multiple times
    points = 100
    rules = ("Fear(2)", "Fearless", "Relentless")
    weapons = {"N=Sword|R=|A=3|AP=1": 2, "N=Rifle|R=24|A=2|AP=0": 1}

    sig1 = build_stage1_signature(points, rules, weapons)
    sig2 = build_stage1_signature(points, rules, weapons)

    print(f"\nSignature 1: {sig1}")
    print(f"Signature 2: {sig2}")
    print(f"Match: {sig1 == sig2}")

    # Test with reordered weapon dict (should still be same)
    weapons_reordered = {"N=Rifle|R=24|A=2|AP=0": 1, "N=Sword|R=|A=3|AP=1": 2}
    sig3 = build_stage1_signature(points, rules, weapons_reordered)

    print(f"Signature 3 (reordered weapons): {sig3}")
    print(f"Match with sig1: {sig1 == sig3}")

    return sig1 == sig2 == sig3


def main():
    print("Unit Loadout Builder Validation Tests")
    print("=" * 60)

    results = []

    results.append(("Rule Extraction", test_rule_extraction()))
    results.append(("Weapon Key Generation", test_weapon_key_generation()))
    results.append(("Self-Replacement Detection", test_self_replacement_detection()))
    results.append(("Weapon Count Output", test_weapon_count_output()))
    results.append(("Rules Normalization", test_rules_normalization()))
    results.append(("Signature Consistency", test_signature_consistency()))

    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)

    all_passed = True
    for name, passed in results:
        status = "PASS" if passed else "FAIL"
        print(f"  {status}: {name}")
        if not passed:
            all_passed = False

    print("\n" + "=" * 60)
    if all_passed:
        print("All tests passed!")
    else:
        print("Some tests failed - see details above")
    print("=" * 60)

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
