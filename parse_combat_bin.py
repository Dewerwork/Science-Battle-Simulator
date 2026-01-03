#!/usr/bin/env python3
"""
Combat Binary File Parser and Viewer

This script can:
1. Generate a sample combat and save it to a .bin file
2. Parse a .bin file and display everything that happened in human-readable format

Binary Format Specification:
============================

Header (32 bytes):
    - Magic: 4 bytes "CMBT"
    - Version: 2 bytes (u16)
    - Flags: 2 bytes (u16)
    - Attacker name length: 1 byte (u8)
    - Defender name length: 1 byte (u8)
    - Attacker name: up to 32 bytes (padded)
    - Defender name: up to 32 bytes (padded)
    - Attacker models start: 1 byte (u8)
    - Defender models start: 1 byte (u8)
    - Attacker total wounds: 2 bytes (u16)
    - Defender total wounds: 2 bytes (u16)
    - Event count: 4 bytes (u32)
    - Total rounds: 1 byte (u8)
    - Winner: 1 byte (u8) - 0=attacker, 1=defender, 2=draw
    - Victory condition: 1 byte (u8)
    - Reserved: padding to 96 bytes

Events (variable length):
    Each event has:
    - Event type: 1 byte (u8)
    - Timestamp: 2 bytes (u16)
    - Round number: 1 byte (u8)
    - Data length: 2 bytes (u16)
    - Data: variable (depends on event type)

Usage:
    # Generate sample combat and save to file
    python parse_combat_bin.py --generate sample_combat.bin

    # Parse and display combat from file
    python parse_combat_bin.py sample_combat.bin

    # Verbose output with all dice rolls
    python parse_combat_bin.py sample_combat.bin --verbose
"""

from __future__ import annotations

import struct
import sys
import argparse
import random
from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path
from typing import BinaryIO, Optional, List, Dict


# ==============================================================================
# Constants and Enums
# ==============================================================================

MAGIC = b"CMBT"
VERSION = 1

class EventType(IntEnum):
    """Combat event types - matches the Python CombatEventType"""
    BATTLE_START = 0
    BATTLE_END = 1
    ROUND_START = 2
    ROUND_END = 3
    ACTIVATION_START = 4
    ACTIVATION_END = 5
    UNIT_RALLIES = 6
    ATTACK_START = 7
    WEAPON_ATTACK_START = 8
    HIT_ROLL = 9
    HIT_MODIFIER_APPLIED = 10
    DEFENSE_ROLL = 11
    WOUND_MODIFIER_APPLIED = 12
    WEAPON_ATTACK_END = 13
    ATTACK_END = 14
    WOUND_ALLOCATED = 15
    MODEL_KILLED = 16
    REGENERATION_ROLL = 17
    PROTECTED_ROLL = 18
    RULE_TRIGGERED = 19
    UNIT_SHAKEN = 20
    UNIT_ROUTED = 21
    UNIT_FATIGUED = 22
    MORALE_TEST = 23
    STRIKE_BACK_START = 24
    STRIKE_BACK_END = 25


class Winner(IntEnum):
    ATTACKER = 0
    DEFENDER = 1
    DRAW = 2


class VictoryCondition(IntEnum):
    ATTACKER_DESTROYED_ENEMY = 0
    DEFENDER_DESTROYED_ENEMY = 1
    ATTACKER_ROUTED_ENEMY = 2
    DEFENDER_ROUTED_ENEMY = 3
    ATTACKER_ROUTED = 4
    DEFENDER_ROUTED = 5
    MAX_ROUNDS_ATTACKER_AHEAD = 6
    MAX_ROUNDS_DEFENDER_AHEAD = 7
    MAX_ROUNDS_DRAW = 8
    MUTUAL_DESTRUCTION = 9


# ==============================================================================
# Data Classes
# ==============================================================================

@dataclass
class CombatEvent:
    """A single combat event."""
    event_type: EventType
    timestamp: int
    round_number: int
    description: str
    data: dict = field(default_factory=dict)


@dataclass
class CombatHeader:
    """Binary file header information."""
    attacker_name: str
    defender_name: str
    attacker_models_start: int
    defender_models_start: int
    attacker_total_wounds: int
    defender_total_wounds: int
    event_count: int
    total_rounds: int
    winner: Winner
    victory_condition: VictoryCondition


@dataclass
class CombatLog:
    """Complete combat log with header and events."""
    header: CombatHeader
    events: List[CombatEvent]


# ==============================================================================
# Binary Serialization
# ==============================================================================

def write_string(f: BinaryIO, s: str, max_len: int = 32) -> None:
    """Write a string with length prefix, padded to max_len."""
    encoded = s.encode("utf-8")[:max_len]
    f.write(struct.pack("B", len(encoded)))
    f.write(encoded)
    # Pad to max_len
    padding = max_len - len(encoded)
    f.write(b"\x00" * padding)


def read_string(f: BinaryIO, max_len: int = 32) -> str:
    """Read a length-prefixed string."""
    length = struct.unpack("B", f.read(1))[0]
    data = f.read(max_len)
    return data[:length].decode("utf-8")


def write_event(f: BinaryIO, event: CombatEvent) -> None:
    """Write a single event to binary format."""
    # Encode description
    desc_bytes = event.description.encode("utf-8")

    # Encode extra data as simple key=value pairs
    data_parts = []
    for key, value in event.data.items():
        if isinstance(value, list):
            value_str = ",".join(str(v) for v in value)
        else:
            value_str = str(value)
        data_parts.append(f"{key}={value_str}")
    data_str = ";".join(data_parts)
    data_bytes = data_str.encode("utf-8")

    # Calculate total data length
    total_len = 2 + len(desc_bytes) + 2 + len(data_bytes)

    # Write event header
    f.write(struct.pack("<BHBH",
        event.event_type,
        event.timestamp,
        event.round_number,
        total_len
    ))

    # Write description
    f.write(struct.pack("<H", len(desc_bytes)))
    f.write(desc_bytes)

    # Write data
    f.write(struct.pack("<H", len(data_bytes)))
    f.write(data_bytes)


def read_event(f: BinaryIO) -> Optional[CombatEvent]:
    """Read a single event from binary format."""
    header_data = f.read(6)
    if len(header_data) < 6:
        return None

    event_type, timestamp, round_number, data_len = struct.unpack("<BHBH", header_data)

    # Read description
    desc_len = struct.unpack("<H", f.read(2))[0]
    description = f.read(desc_len).decode("utf-8")

    # Read data
    extra_len = struct.unpack("<H", f.read(2))[0]
    extra_str = f.read(extra_len).decode("utf-8")

    # Parse data
    data = {}
    if extra_str:
        for part in extra_str.split(";"):
            if "=" in part:
                key, value = part.split("=", 1)
                # Try to parse as int/float/list
                if "," in value:
                    try:
                        data[key] = [int(v) for v in value.split(",")]
                    except ValueError:
                        data[key] = value.split(",")
                else:
                    try:
                        data[key] = int(value)
                    except ValueError:
                        try:
                            data[key] = float(value)
                        except ValueError:
                            data[key] = value

    return CombatEvent(
        event_type=EventType(event_type),
        timestamp=timestamp,
        round_number=round_number,
        description=description,
        data=data
    )


def write_combat_log(filepath: Path, log: CombatLog) -> None:
    """Write a complete combat log to a binary file."""
    with open(filepath, "wb") as f:
        # Write magic and version
        f.write(MAGIC)
        f.write(struct.pack("<HH", VERSION, 0))  # version, flags

        # Write unit names
        write_string(f, log.header.attacker_name)
        write_string(f, log.header.defender_name)

        # Write model counts and wounds
        f.write(struct.pack("<BBHH",
            log.header.attacker_models_start,
            log.header.defender_models_start,
            log.header.attacker_total_wounds,
            log.header.defender_total_wounds
        ))

        # Write event count
        f.write(struct.pack("<I", log.header.event_count))

        # Write outcome
        f.write(struct.pack("<BBB",
            log.header.total_rounds,
            log.header.winner,
            log.header.victory_condition
        ))

        # Padding to align to 96 bytes header
        current_pos = f.tell()
        padding_needed = 96 - current_pos
        f.write(b"\x00" * padding_needed)

        # Write events
        for event in log.events:
            write_event(f, event)


def read_combat_log(filepath: Path) -> CombatLog:
    """Read a complete combat log from a binary file."""
    with open(filepath, "rb") as f:
        # Read and verify magic
        magic = f.read(4)
        if magic != MAGIC:
            raise ValueError(f"Invalid magic bytes: {magic!r}, expected {MAGIC!r}")

        # Read version and flags
        version, flags = struct.unpack("<HH", f.read(4))
        if version > VERSION:
            raise ValueError(f"Unsupported version: {version}")

        # Read unit names
        attacker_name = read_string(f)
        defender_name = read_string(f)

        # Read model counts and wounds
        (attacker_models, defender_models,
         attacker_wounds, defender_wounds) = struct.unpack("<BBHH", f.read(6))

        # Read event count
        event_count = struct.unpack("<I", f.read(4))[0]

        # Read outcome
        total_rounds, winner, victory_cond = struct.unpack("<BBB", f.read(3))

        # Skip padding
        current_pos = f.tell()
        f.seek(96)

        # Read events
        events = []
        for _ in range(event_count):
            event = read_event(f)
            if event:
                events.append(event)

        header = CombatHeader(
            attacker_name=attacker_name,
            defender_name=defender_name,
            attacker_models_start=attacker_models,
            defender_models_start=defender_models,
            attacker_total_wounds=attacker_wounds,
            defender_total_wounds=defender_wounds,
            event_count=event_count,
            total_rounds=total_rounds,
            winner=Winner(winner),
            victory_condition=VictoryCondition(victory_cond)
        )

        return CombatLog(header=header, events=events)


# ==============================================================================
# Sample Combat Generation
# ==============================================================================

def roll_d6(count: int = 1) -> List[int]:
    """Roll D6 dice."""
    return [random.randint(1, 6) for _ in range(count)]


def count_successes(rolls: List[int], target: int) -> int:
    """Count dice that meet or exceed target."""
    return sum(1 for r in rolls if r >= target)


def generate_sample_combat() -> CombatLog:
    """Generate a sample combat between two units."""
    events: List[CombatEvent] = []
    timestamp = 0

    def add_event(event_type: EventType, desc: str, round_num: int = 0, **data) -> None:
        nonlocal timestamp
        events.append(CombatEvent(
            event_type=event_type,
            timestamp=timestamp,
            round_number=round_num,
            description=desc,
            data=data
        ))
        timestamp += 1

    # Setup
    attacker_name = "Assault Marines"
    defender_name = "Ork Boyz"
    attacker_models = 5
    defender_models = 10
    attacker_wounds = 5  # 1 wound each
    defender_wounds = 10  # 1 wound each

    attacker_quality = 3
    attacker_defense = 4
    defender_quality = 5
    defender_defense = 5

    # Track state
    attacker_alive = attacker_models
    defender_alive = defender_models

    # Battle start
    add_event(
        EventType.BATTLE_START,
        f"Battle begins: {attacker_name} ({attacker_models} models, {attacker_wounds} wounds) "
        f"vs {defender_name} ({defender_models} models, {defender_wounds} wounds)",
        attacker_models=attacker_models,
        defender_models=defender_models,
        attacker_wounds=attacker_wounds,
        defender_wounds=defender_wounds
    )

    current_round = 0
    max_rounds = 4

    while attacker_alive > 0 and defender_alive > 0 and current_round < max_rounds:
        current_round += 1

        # Round start
        add_event(EventType.ROUND_START, f"Round {current_round} begins", current_round)

        round_attacker_wounds = 0
        round_defender_wounds = 0

        # === Attacker's Turn ===
        add_event(
            EventType.ACTIVATION_START,
            f"{attacker_name} activates to charge",
            current_round,
            action="charge",
            is_fatigued=False
        )

        # Attack sequence
        is_charging = (current_round == 1)
        charge_str = " (charging)" if is_charging else ""
        add_event(
            EventType.ATTACK_START,
            f"{attacker_name} attacks {defender_name} in melee{charge_str}",
            current_round,
            phase="melee",
            is_charging=is_charging
        )

        # Weapon attacks - Chainswords
        attacks_per_model = 2
        total_attacks = attacker_alive * attacks_per_model

        add_event(
            EventType.WEAPON_ATTACK_START,
            f"Marine attacks with Chainsword ({attacks_per_model} attacks)",
            current_round,
            weapon_name="Chainsword",
            attacks=total_attacks,
            model_name="Marine"
        )

        # Hit roll
        hit_rolls = roll_d6(total_attacks)
        hit_target = attacker_quality
        hits = count_successes(hit_rolls, hit_target)
        sixes = sum(1 for r in hit_rolls if r == 6)

        add_event(
            EventType.HIT_ROLL,
            f"Hit roll (need {hit_target}+): {hit_rolls} -> {hits} hits",
            current_round,
            dice_values=hit_rolls,
            dice_target=hit_target,
            dice_successes=hits,
            dice_failures=total_attacks - hits
        )

        # Furious on charge
        hits_after_mods = hits
        if is_charging and sixes > 0:
            hits_after_mods += sixes
            add_event(
                EventType.RULE_TRIGGERED,
                f"Rule triggered: Furious - Charging grants +{sixes} extra hits from 6s",
                current_round,
                rule_name="Furious",
                rule_effect=f"+{sixes} extra hits"
            )
            add_event(
                EventType.HIT_MODIFIER_APPLIED,
                f"Furious: +{sixes} extra hits from 6s when charging ({hits} -> {hits_after_mods} hits)",
                current_round,
                original_hits=hits,
                modified_hits=hits_after_mods
            )

        # Defense roll
        defense_target = defender_defense
        ap = 1  # Chainsword AP(1)
        effective_defense = min(6, defense_target + ap)
        defense_rolls = roll_d6(hits_after_mods)
        saves = count_successes(defense_rolls, effective_defense)
        wounds = hits_after_mods - saves

        add_event(
            EventType.DEFENSE_ROLL,
            f"Defense roll (need {effective_defense}+, base {defense_target}+ with AP {ap}): "
            f"{defense_rolls} -> {saves} saves, {wounds} wounds through",
            current_round,
            dice_values=defense_rolls,
            dice_target=effective_defense,
            dice_successes=saves,
            dice_failures=wounds,
            ap=ap,
            base_target=defense_target
        )

        add_event(
            EventType.WEAPON_ATTACK_END,
            f"Chainsword deals {wounds} wounds",
            current_round,
            weapon_name="Chainsword",
            wounds_dealt=wounds
        )

        # Allocate wounds
        models_killed = 0
        wounds_remaining = wounds
        for i in range(wounds_remaining):
            if defender_alive > 0:
                add_event(
                    EventType.WOUND_ALLOCATED,
                    f"Ork Boy takes 1 wound(s) (0/1 -> 1/1, 0 remaining)",
                    current_round,
                    model_name="Ork Boy",
                    wounds_applied=1,
                    tough=1,
                    remaining=0
                )
                add_event(
                    EventType.MODEL_KILLED,
                    "Ork Boy is killed!",
                    current_round,
                    model_name="Ork Boy"
                )
                defender_alive -= 1
                models_killed += 1
                round_attacker_wounds += 1

        add_event(
            EventType.ATTACK_END,
            f"Attack complete: {wounds} wounds dealt, {models_killed} models killed",
            current_round,
            total_wounds=wounds,
            models_killed=models_killed
        )

        add_event(
            EventType.UNIT_FATIGUED,
            f"{attacker_name} becomes Fatigued (only hits on unmodified 6s in melee)",
            current_round
        )

        # === Strike Back (Round 1 only) ===
        if current_round == 1 and defender_alive > 0:
            add_event(
                EventType.STRIKE_BACK_START,
                f"{defender_name} strikes back!",
                current_round,
                is_fatigued=False
            )

            # Ork attack
            add_event(
                EventType.ATTACK_START,
                f"{defender_name} attacks {attacker_name} in melee",
                current_round,
                phase="melee",
                is_charging=False
            )

            ork_attacks = defender_alive * 2  # 2 attacks each
            add_event(
                EventType.WEAPON_ATTACK_START,
                f"Ork Boy attacks with Choppa ({ork_attacks} attacks total)",
                current_round,
                weapon_name="Choppa",
                attacks=ork_attacks,
                model_name="Ork Boy"
            )

            # Hit roll
            ork_hit_rolls = roll_d6(ork_attacks)
            ork_hits = count_successes(ork_hit_rolls, defender_quality)

            add_event(
                EventType.HIT_ROLL,
                f"Hit roll (need {defender_quality}+): {ork_hit_rolls} -> {ork_hits} hits",
                current_round,
                dice_values=ork_hit_rolls,
                dice_target=defender_quality,
                dice_successes=ork_hits,
                dice_failures=ork_attacks - ork_hits
            )

            # Defense
            ork_defense_rolls = roll_d6(ork_hits)
            marine_saves = count_successes(ork_defense_rolls, attacker_defense)
            ork_wounds = ork_hits - marine_saves

            add_event(
                EventType.DEFENSE_ROLL,
                f"Defense roll (need {attacker_defense}+): {ork_defense_rolls} -> "
                f"{marine_saves} saves, {ork_wounds} wounds through",
                current_round,
                dice_values=ork_defense_rolls,
                dice_target=attacker_defense,
                dice_successes=marine_saves,
                dice_failures=ork_wounds,
                ap=0
            )

            add_event(
                EventType.WEAPON_ATTACK_END,
                f"Choppa deals {ork_wounds} wounds",
                current_round,
                weapon_name="Choppa",
                wounds_dealt=ork_wounds
            )

            # Allocate wounds to marines
            strike_back_kills = 0
            for i in range(ork_wounds):
                if attacker_alive > 0:
                    add_event(
                        EventType.WOUND_ALLOCATED,
                        "Marine takes 1 wound(s) (0/1 -> 1/1, 0 remaining)",
                        current_round,
                        model_name="Marine",
                        wounds_applied=1,
                        tough=1,
                        remaining=0
                    )
                    add_event(
                        EventType.MODEL_KILLED,
                        "Marine is killed!",
                        current_round,
                        model_name="Marine"
                    )
                    attacker_alive -= 1
                    strike_back_kills += 1
                    round_defender_wounds += 1

            add_event(
                EventType.ATTACK_END,
                f"Attack complete: {ork_wounds} wounds dealt, {strike_back_kills} models killed",
                current_round,
                total_wounds=ork_wounds,
                models_killed=strike_back_kills
            )

            add_event(
                EventType.STRIKE_BACK_END,
                f"{defender_name} strike back complete: {ork_wounds} wounds, {strike_back_kills} kills",
                current_round,
                wounds_dealt=ork_wounds,
                models_killed=strike_back_kills
            )

            add_event(
                EventType.UNIT_FATIGUED,
                f"{defender_name} becomes Fatigued (only hits on unmodified 6s in melee)",
                current_round
            )

        add_event(
            EventType.ACTIVATION_END,
            f"{attacker_name} finishes activation",
            current_round
        )

        # === Defender's Turn (Round 2+) ===
        if current_round > 1 and defender_alive > 0 and attacker_alive > 0:
            add_event(
                EventType.ACTIVATION_START,
                f"{defender_name} activates to melee (fatigued - only hits on 6s)",
                current_round,
                action="melee",
                is_fatigued=True
            )

            add_event(
                EventType.ATTACK_START,
                f"{defender_name} attacks {attacker_name} in melee",
                current_round,
                phase="melee",
                is_charging=False
            )

            # Fatigued attack - only 6s hit
            ork_attacks = defender_alive * 2
            add_event(
                EventType.WEAPON_ATTACK_START,
                f"Ork Boy attacks with Choppa ({ork_attacks} attacks)",
                current_round,
                weapon_name="Choppa",
                attacks=ork_attacks
            )

            fat_rolls = roll_d6(ork_attacks)
            fat_hits = sum(1 for r in fat_rolls if r == 6)  # Only 6s

            add_event(
                EventType.HIT_ROLL,
                f"Hit roll (fatigued - need 6s): {fat_rolls} -> {fat_hits} hits",
                current_round,
                dice_values=fat_rolls,
                dice_target=6,
                dice_successes=fat_hits,
                is_fatigued=True
            )

            if fat_hits > 0:
                fat_def_rolls = roll_d6(fat_hits)
                fat_saves = count_successes(fat_def_rolls, attacker_defense)
                fat_wounds = fat_hits - fat_saves

                add_event(
                    EventType.DEFENSE_ROLL,
                    f"Defense roll (need {attacker_defense}+): {fat_def_rolls} -> "
                    f"{fat_saves} saves, {fat_wounds} wounds through",
                    current_round,
                    dice_values=fat_def_rolls,
                    dice_target=attacker_defense,
                    dice_successes=fat_saves,
                    dice_failures=fat_wounds
                )

                for i in range(fat_wounds):
                    if attacker_alive > 0:
                        add_event(EventType.WOUND_ALLOCATED,
                            "Marine takes 1 wound(s)", current_round)
                        add_event(EventType.MODEL_KILLED,
                            "Marine is killed!", current_round)
                        attacker_alive -= 1
                        round_defender_wounds += 1

            add_event(
                EventType.WEAPON_ATTACK_END,
                f"Choppa deals wounds",
                current_round
            )

            add_event(
                EventType.ATTACK_END,
                "Attack complete",
                current_round
            )

            add_event(
                EventType.ACTIVATION_END,
                f"{defender_name} finishes activation",
                current_round
            )

        # Round end
        add_event(
            EventType.ROUND_END,
            f"Round {current_round} ends. Wounds this round - {attacker_name}: {round_attacker_wounds}, "
            f"{defender_name}: {round_defender_wounds}",
            current_round,
            attacker_wounds=round_attacker_wounds,
            defender_wounds=round_defender_wounds
        )

        # Morale check (simplified - if at half strength)
        if defender_alive <= defender_models // 2 and defender_alive > 0:
            morale_roll = roll_d6(1)[0]
            passed = morale_roll >= defender_quality

            add_event(
                EventType.MORALE_TEST,
                f"{defender_name} morale test (casualties, need {defender_quality}+): "
                f"rolled {morale_roll} -> {'passed' if passed else 'failed -> shaken'}",
                current_round,
                dice_values=[morale_roll],
                dice_target=defender_quality,
                dice_successes=1 if passed else 0,
                reason="casualties"
            )

            if not passed:
                add_event(
                    EventType.UNIT_SHAKEN,
                    f"{defender_name} becomes Shaken! (-1 to Quality and Defense)",
                    current_round
                )

    # Determine winner
    if attacker_alive <= 0 and defender_alive <= 0:
        winner = Winner.DRAW
        victory_cond = VictoryCondition.MUTUAL_DESTRUCTION
    elif defender_alive <= 0:
        winner = Winner.ATTACKER
        victory_cond = VictoryCondition.ATTACKER_DESTROYED_ENEMY
    elif attacker_alive <= 0:
        winner = Winner.DEFENDER
        victory_cond = VictoryCondition.DEFENDER_DESTROYED_ENEMY
    else:
        # Compare remaining percentage
        attacker_pct = attacker_alive / attacker_models
        defender_pct = defender_alive / defender_models
        if attacker_pct > defender_pct:
            winner = Winner.ATTACKER
            victory_cond = VictoryCondition.MAX_ROUNDS_ATTACKER_AHEAD
        elif defender_pct > attacker_pct:
            winner = Winner.DEFENDER
            victory_cond = VictoryCondition.MAX_ROUNDS_DEFENDER_AHEAD
        else:
            winner = Winner.DRAW
            victory_cond = VictoryCondition.MAX_ROUNDS_DRAW

    winner_name = attacker_name if winner == Winner.ATTACKER else (
        defender_name if winner == Winner.DEFENDER else "Draw"
    )

    # Battle end
    add_event(
        EventType.BATTLE_END,
        f"Battle ends: {winner_name} wins by {victory_cond.name}. "
        f"Final: {attacker_name} {attacker_alive} models, {defender_name} {defender_alive} models",
        current_round,
        winner=winner_name,
        victory_condition=victory_cond.name,
        attacker_models=attacker_alive,
        defender_models=defender_alive
    )

    header = CombatHeader(
        attacker_name=attacker_name,
        defender_name=defender_name,
        attacker_models_start=attacker_models,
        defender_models_start=defender_models,
        attacker_total_wounds=attacker_wounds,
        defender_total_wounds=defender_wounds,
        event_count=len(events),
        total_rounds=current_round,
        winner=winner,
        victory_condition=victory_cond
    )

    return CombatLog(header=header, events=events)


# ==============================================================================
# Display Functions
# ==============================================================================

def get_event_icon(event_type: EventType) -> str:
    """Get an icon/symbol for each event type."""
    icons = {
        EventType.BATTLE_START: ">>>",
        EventType.BATTLE_END: "<<<",
        EventType.ROUND_START: "===",
        EventType.ROUND_END: "---",
        EventType.ACTIVATION_START: ">>",
        EventType.ACTIVATION_END: "<<",
        EventType.UNIT_RALLIES: "[R]",
        EventType.ATTACK_START: "[A]",
        EventType.WEAPON_ATTACK_START: " W ",
        EventType.HIT_ROLL: " H ",
        EventType.HIT_MODIFIER_APPLIED: " +H",
        EventType.DEFENSE_ROLL: " D ",
        EventType.WOUND_MODIFIER_APPLIED: " +W",
        EventType.WEAPON_ATTACK_END: " /W",
        EventType.ATTACK_END: "[/A]",
        EventType.WOUND_ALLOCATED: " *W",
        EventType.MODEL_KILLED: " XX",
        EventType.REGENERATION_ROLL: " RG",
        EventType.PROTECTED_ROLL: " PR",
        EventType.RULE_TRIGGERED: " !R",
        EventType.UNIT_SHAKEN: "[!]",
        EventType.UNIT_ROUTED: "[X]",
        EventType.UNIT_FATIGUED: "[F]",
        EventType.MORALE_TEST: " M ",
        EventType.STRIKE_BACK_START: "->",
        EventType.STRIKE_BACK_END: "<-",
    }
    return icons.get(event_type, "   ")


def format_dice_rolls(rolls: List[int], target: int) -> str:
    """Format dice rolls with highlighting for successes."""
    result = []
    for r in rolls:
        if r >= target:
            result.append(f"[{r}]")  # Success
        else:
            result.append(f" {r} ")  # Failure
    return "".join(result)


def display_combat(log: CombatLog, verbose: bool = False) -> None:
    """Display a combat log in human-readable format."""
    h = log.header

    print("\n" + "=" * 70)
    print("                    COMBAT REPLAY")
    print("=" * 70)
    print(f"\n  ATTACKER: {h.attacker_name}")
    print(f"    Models: {h.attacker_models_start}, Total Wounds: {h.attacker_total_wounds}")
    print(f"\n  DEFENDER: {h.defender_name}")
    print(f"    Models: {h.defender_models_start}, Total Wounds: {h.defender_total_wounds}")
    print("\n" + "-" * 70)

    current_round = 0
    indent = ""

    for event in log.events:
        icon = get_event_icon(event.event_type)

        # Adjust indentation based on event type
        if event.event_type == EventType.ROUND_START:
            current_round = event.round_number
            print(f"\n{'=' * 70}")
            print(f"                      ROUND {current_round}")
            print("=" * 70)
            indent = ""
        elif event.event_type == EventType.ROUND_END:
            indent = ""
            print("-" * 70)
        elif event.event_type == EventType.ACTIVATION_START:
            indent = "  "
            print(f"\n{icon} {event.description}")
        elif event.event_type == EventType.ACTIVATION_END:
            indent = "  "
        elif event.event_type == EventType.ATTACK_START:
            indent = "    "
            print(f"\n{indent}{icon} {event.description}")
        elif event.event_type == EventType.ATTACK_END:
            indent = "    "
            print(f"{indent}{icon} {event.description}")
        elif event.event_type == EventType.STRIKE_BACK_START:
            indent = "    "
            print(f"\n{indent}{icon} {event.description}")
        elif event.event_type == EventType.STRIKE_BACK_END:
            indent = "    "
            print(f"{indent}{icon} {event.description}")
        elif event.event_type in (EventType.BATTLE_START, EventType.BATTLE_END):
            print(f"\n{icon} {event.description}")
        elif event.event_type == EventType.HIT_ROLL:
            if verbose:
                dice = event.data.get("dice_values", [])
                target = event.data.get("dice_target", 0)
                print(f"{indent}      {icon} {event.description}")
                if dice:
                    print(f"{indent}          Dice: {format_dice_rolls(dice, target)}")
            else:
                print(f"{indent}      {icon} {event.description}")
        elif event.event_type == EventType.DEFENSE_ROLL:
            if verbose:
                dice = event.data.get("dice_values", [])
                target = event.data.get("dice_target", 0)
                print(f"{indent}      {icon} {event.description}")
                if dice:
                    print(f"{indent}          Dice: {format_dice_rolls(dice, target)}")
            else:
                print(f"{indent}      {icon} {event.description}")
        elif event.event_type == EventType.MODEL_KILLED:
            print(f"{indent}      {icon} {event.description}")
        elif event.event_type == EventType.WOUND_ALLOCATED:
            if verbose:
                print(f"{indent}      {icon} {event.description}")
        elif event.event_type == EventType.RULE_TRIGGERED:
            print(f"{indent}      {icon} {event.description}")
        elif event.event_type == EventType.HIT_MODIFIER_APPLIED:
            print(f"{indent}      {icon} {event.description}")
        elif event.event_type == EventType.UNIT_FATIGUED:
            print(f"{indent}  {icon} {event.description}")
        elif event.event_type == EventType.UNIT_SHAKEN:
            print(f"{indent}  {icon} {event.description}")
        elif event.event_type == EventType.MORALE_TEST:
            print(f"{indent}  {icon} {event.description}")
        elif event.event_type == EventType.WEAPON_ATTACK_START:
            if verbose:
                print(f"{indent}      {icon} {event.description}")
        elif event.event_type == EventType.WEAPON_ATTACK_END:
            print(f"{indent}      {icon} {event.description}")
        else:
            if verbose:
                print(f"{indent}{icon} {event.description}")

    # Summary
    print("\n" + "=" * 70)
    print("                    BATTLE SUMMARY")
    print("=" * 70)
    winner_str = {
        Winner.ATTACKER: h.attacker_name,
        Winner.DEFENDER: h.defender_name,
        Winner.DRAW: "DRAW"
    }[h.winner]

    victory_str = h.victory_condition.name.replace("_", " ").title()

    print(f"\n  Winner: {winner_str}")
    print(f"  Victory: {victory_str}")
    print(f"  Rounds: {h.total_rounds}")
    print(f"\n  {h.attacker_name}: Started {h.attacker_models_start} models")
    print(f"  {h.defender_name}: Started {h.defender_models_start} models")
    print(f"\n  Total Events: {h.event_count}")
    print("=" * 70 + "\n")


# ==============================================================================
# Main Entry Point
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Parse and display combat from binary files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        "file",
        nargs="?",
        help="Path to combat .bin file to parse"
    )
    parser.add_argument(
        "--generate", "-g",
        metavar="OUTPUT",
        help="Generate a sample combat and save to OUTPUT file"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show detailed output including all dice rolls"
    )
    parser.add_argument(
        "--seed", "-s",
        type=int,
        default=None,
        help="Random seed for reproducible combat generation"
    )

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    if args.generate:
        # Generate sample combat
        output_path = Path(args.generate)
        print(f"Generating sample combat...")
        log = generate_sample_combat()
        write_combat_log(output_path, log)
        print(f"Combat saved to: {output_path}")
        print(f"  Events: {log.header.event_count}")
        print(f"  Rounds: {log.header.total_rounds}")
        print(f"  Winner: {log.header.winner.name}")

        # Also display it
        print("\nDisplaying generated combat:")
        display_combat(log, verbose=args.verbose)

    elif args.file:
        # Parse and display
        file_path = Path(args.file)
        if not file_path.exists():
            print(f"Error: File not found: {file_path}", file=sys.stderr)
            sys.exit(1)

        print(f"Loading combat from: {file_path}")
        log = read_combat_log(file_path)
        display_combat(log, verbose=args.verbose)

    else:
        # No arguments - show help
        parser.print_help()
        print("\nQuick start:")
        print("  # Generate and view sample combat:")
        print("  python parse_combat_bin.py --generate sample.bin")
        print("")
        print("  # View existing combat file:")
        print("  python parse_combat_bin.py sample.bin")


if __name__ == "__main__":
    main()
