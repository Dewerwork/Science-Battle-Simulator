#!/usr/bin/env python3
"""
Edge case tests for A0 weapon fix.
"""

import re


def looks_like_weapon_profile(inside: str) -> bool:
    if not inside:
        return False
    if re.search(r'\bA\d+\b', inside):
        return True
    if re.search(r'\bAP\(\s*-?\d+\s*\)', inside):
        return True
    return False


def run_edge_case_tests():
    print("=" * 70)
    print("EDGE CASE TESTS FOR A0 FIX")
    print("=" * 70)

    passed = 0
    failed = 0

    # Edge case tests for looks_like_weapon_profile
    tests = [
        # (input, expected, description)
        ("", False, "Empty string"),
        ("A", False, "Just 'A'"),
        ("AP", False, "Just 'AP'"),
        ("A0", True, "A0 - zero attacks (legitimate)"),
        ("A1", True, "A1 - one attack"),
        ("A99", True, "A99 - many attacks"),
        ("A1234", True, "A1234 - very many attacks"),
        ("Attack", False, "Word 'Attack' (not A followed by digit)"),
        ("Attacks", False, "Word 'Attacks'"),
        ("AP(1)", True, "AP(1)"),
        ("AP(0)", True, "AP(0) - zero AP"),
        ("AP(-1)", True, "AP(-1) - negative AP"),
        ("AP( 2 )", True, "AP with spaces"),
        ("AP(10)", True, "AP(10) - double digit"),
        ("AP", False, "Just AP without parens"),
        ("Blast(3)", False, "Blast - not A# or AP"),
        ("Deadly(6)", False, "Deadly - not A# or AP"),
        ("Caster(3)", False, "Caster - not A# or AP"),
        ("Spawn(Spores [5])", False, "Spawn - not A# or AP"),
        ("Regeneration", False, "Simple rule"),
        ("Furious", False, "Simple rule"),
        ("Ambush, Flying", False, "Multiple rules"),
        ("Tough(6), Transport(21)", False, "Rules with params"),
        ("12\", A2, Blast(3)", True, "Range + A2"),
        ("18\", A4, AP(1), Rupture", True, "Full weapon profile"),
        ("A3, AP(1)", True, "Attack and AP"),
        ("A8, Bane, Precise", True, "Attack with tags"),
        ("Breath Attack", False, "Breath Attack - 'Attack' not A#"),
        ("Surprise Attack(3)", False, "Surprise Attack(3) - no A#"),
        ("Increased Shooting Range", False, "Long rule name"),
        ("Rapid Charge Aura", False, "Aura rule"),
        ("Hive Bond Boost Aura", False, "Another aura"),
        ("A3, AP(1), Rending", True, "Weapon with special"),
        ("A1, AP(2), Deadly(3)", True, "Full profile"),
        ("A6, AP(1), Reliable, Rending", True, "Profile with multiple specials"),
        ("24\", A2, Blast(3)", True, "Ranged weapon"),
        ("36\", A1, AP(2), Blast(3)", True, "Long range weapon"),
        # Tricky cases
        ("Takedown", False, "Rule that might confuse"),
        ("Scout", False, "Simple rule"),
        ("Strider", False, "Simple rule"),
        ("Fearless", False, "Simple rule"),
        ("Hero", False, "Simple rule"),
        ("Fast", False, "Simple rule"),
        ("Flying", False, "Simple rule"),
        ("Resistance", False, "Simple rule"),
        ("Regeneration", False, "Simple rule"),
        # Numbers that aren't A#
        ("3)", False, "Just number and paren"),
        ("12", False, "Just number"),
        ("(3)", False, "Number in parens"),
        # AP edge cases
        ("AP()", False, "Empty AP - malformed"),
        ("AP( )", False, "Space in AP - malformed"),
    ]

    print("\n### Testing looks_like_weapon_profile() function ###\n")

    for input_val, expected, desc in tests:
        result = looks_like_weapon_profile(input_val)
        if result == expected:
            status = "✓"
            passed += 1
        else:
            status = "❌"
            failed += 1

        print(f"  {status} '{input_val}' → {result} (expected {expected}) [{desc}]")

    print("\n" + "=" * 70)
    print("EDGE CASE SUMMARY")
    print("=" * 70)
    print(f"Total: {passed + failed}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")

    if failed == 0:
        print("\n✅ ALL EDGE CASE TESTS PASSED!")
        return True
    else:
        print(f"\n❌ {failed} TESTS FAILED!")
        return False


if __name__ == "__main__":
    success = run_edge_case_tests()
    exit(0 if success else 1)
