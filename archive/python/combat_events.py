"""Combat event data structures for visualization and logging."""

from dataclasses import dataclass, field
from enum import Enum
from typing import Any

import numpy as np
from numpy.typing import NDArray


class CombatEventType(str, Enum):
    """Types of combat events for visualization."""

    # Battle level events
    BATTLE_START = "battle_start"
    BATTLE_END = "battle_end"
    ROUND_START = "round_start"
    ROUND_END = "round_end"

    # Activation events
    ACTIVATION_START = "activation_start"
    ACTIVATION_END = "activation_end"
    UNIT_RALLIES = "unit_rallies"

    # Attack sequence events
    ATTACK_START = "attack_start"
    WEAPON_ATTACK_START = "weapon_attack_start"
    HIT_ROLL = "hit_roll"
    HIT_MODIFIER_APPLIED = "hit_modifier_applied"
    DEFENSE_ROLL = "defense_roll"
    WOUND_MODIFIER_APPLIED = "wound_modifier_applied"
    WEAPON_ATTACK_END = "weapon_attack_end"
    ATTACK_END = "attack_end"

    # Wound allocation events
    WOUND_ALLOCATED = "wound_allocated"
    MODEL_KILLED = "model_killed"
    REGENERATION_ROLL = "regeneration_roll"
    PROTECTED_ROLL = "protected_roll"

    # Special rule events
    RULE_TRIGGERED = "rule_triggered"

    # Status events
    UNIT_SHAKEN = "unit_shaken"
    UNIT_ROUTED = "unit_routed"
    UNIT_FATIGUED = "unit_fatigued"
    MORALE_TEST = "morale_test"

    # Strike back events
    STRIKE_BACK_START = "strike_back_start"
    STRIKE_BACK_END = "strike_back_end"


@dataclass
class CombatEvent:
    """A single event during combat for visualization."""

    event_type: CombatEventType
    timestamp: int  # Sequential event number
    description: str  # Human-readable description

    # Context information
    round_number: int = 0
    active_unit: str = ""
    target_unit: str = ""

    # Dice roll data (if applicable)
    dice_values: list[int] = field(default_factory=list)
    dice_target: int = 0  # Target number to meet
    dice_successes: int = 0
    dice_failures: int = 0

    # Rule information (if applicable)
    rule_name: str = ""
    rule_value: int | None = None
    rule_effect: str = ""

    # Model information (if applicable)
    model_name: str = ""
    wounds_before: int = 0
    wounds_after: int = 0

    # Additional data for complex events
    extra_data: dict[str, Any] = field(default_factory=dict)

    def __str__(self) -> str:
        return f"[{self.timestamp}] {self.event_type.value}: {self.description}"


@dataclass
class CombatEventLog:
    """Complete log of combat events for a battle."""

    attacker_name: str
    defender_name: str
    attacker_models_start: int
    defender_models_start: int
    events: list[CombatEvent] = field(default_factory=list)

    # Total wounds (tough) for health bar visualization
    attacker_total_wounds: int = 0
    defender_total_wounds: int = 0

    # Final state
    attacker_models_end: int = 0
    defender_models_end: int = 0
    winner: str = ""
    victory_condition: str = ""
    total_rounds: int = 0

    def add_event(self, event: CombatEvent) -> None:
        """Add an event to the log."""
        self.events.append(event)

    def get_events_by_type(self, event_type: CombatEventType) -> list[CombatEvent]:
        """Get all events of a specific type."""
        return [e for e in self.events if e.event_type == event_type]

    def get_events_for_round(self, round_number: int) -> list[CombatEvent]:
        """Get all events in a specific round."""
        return [e for e in self.events if e.round_number == round_number]

    def get_rule_events(self) -> list[CombatEvent]:
        """Get all events where special rules were triggered."""
        return self.get_events_by_type(CombatEventType.RULE_TRIGGERED)

    def get_dice_roll_events(self) -> list[CombatEvent]:
        """Get all dice roll events."""
        roll_types = [
            CombatEventType.HIT_ROLL,
            CombatEventType.DEFENSE_ROLL,
            CombatEventType.REGENERATION_ROLL,
            CombatEventType.PROTECTED_ROLL,
            CombatEventType.MORALE_TEST,
        ]
        return [e for e in self.events if e.event_type in roll_types]

    @property
    def event_count(self) -> int:
        """Get total number of events."""
        return len(self.events)


class CombatEventRecorder:
    """Records combat events during a battle for visualization."""

    def __init__(self, attacker_name: str, defender_name: str):
        """Initialize the recorder.

        Args:
            attacker_name: Name of the attacking unit
            defender_name: Name of the defending unit
        """
        self.log = CombatEventLog(
            attacker_name=attacker_name,
            defender_name=defender_name,
            attacker_models_start=0,
            defender_models_start=0,
        )
        self._timestamp = 0
        self._current_round = 0
        self._active_unit = ""
        self._target_unit = ""

    def _next_timestamp(self) -> int:
        """Get the next timestamp and increment."""
        ts = self._timestamp
        self._timestamp += 1
        return ts

    def _create_event(
        self,
        event_type: CombatEventType,
        description: str,
        **kwargs,
    ) -> CombatEvent:
        """Create an event with current context."""
        return CombatEvent(
            event_type=event_type,
            timestamp=self._next_timestamp(),
            description=description,
            round_number=self._current_round,
            active_unit=self._active_unit,
            target_unit=self._target_unit,
            **kwargs,
        )

    def record_battle_start(
        self,
        attacker_models: int,
        defender_models: int,
        attacker_total_wounds: int = 0,
        defender_total_wounds: int = 0,
    ) -> None:
        """Record battle starting."""
        self.log.attacker_models_start = attacker_models
        self.log.defender_models_start = defender_models
        # Store total wounds (default to model count if not provided)
        self.log.attacker_total_wounds = attacker_total_wounds or attacker_models
        self.log.defender_total_wounds = defender_total_wounds or defender_models

        event = self._create_event(
            CombatEventType.BATTLE_START,
            f"Battle begins: {self.log.attacker_name} ({attacker_models} models, {self.log.attacker_total_wounds} wounds) "
            f"vs {self.log.defender_name} ({defender_models} models, {self.log.defender_total_wounds} wounds)",
            extra_data={
                "attacker_models": attacker_models,
                "defender_models": defender_models,
                "attacker_total_wounds": self.log.attacker_total_wounds,
                "defender_total_wounds": self.log.defender_total_wounds,
            },
        )
        self.log.add_event(event)

    def record_battle_end(
        self,
        attacker_models: int,
        defender_models: int,
        winner: str,
        victory_condition: str,
    ) -> None:
        """Record battle ending."""
        self.log.attacker_models_end = attacker_models
        self.log.defender_models_end = defender_models
        self.log.winner = winner
        self.log.victory_condition = victory_condition
        self.log.total_rounds = self._current_round

        event = self._create_event(
            CombatEventType.BATTLE_END,
            f"Battle ends: {winner} wins by {victory_condition}. "
            f"Final: {self.log.attacker_name} {attacker_models} models, "
            f"{self.log.defender_name} {defender_models} models",
            extra_data={
                "winner": winner,
                "victory_condition": victory_condition,
                "attacker_models": attacker_models,
                "defender_models": defender_models,
            },
        )
        self.log.add_event(event)

    def record_round_start(self, round_number: int) -> None:
        """Record a new round starting."""
        self._current_round = round_number
        event = self._create_event(
            CombatEventType.ROUND_START,
            f"Round {round_number} begins",
        )
        self.log.add_event(event)

    def record_round_end(
        self,
        attacker_wounds_dealt: int,
        defender_wounds_dealt: int,
    ) -> None:
        """Record round ending."""
        event = self._create_event(
            CombatEventType.ROUND_END,
            f"Round {self._current_round} ends. "
            f"Wounds this round - {self.log.attacker_name}: {attacker_wounds_dealt}, "
            f"{self.log.defender_name}: {defender_wounds_dealt}",
            extra_data={
                "attacker_wounds": attacker_wounds_dealt,
                "defender_wounds": defender_wounds_dealt,
            },
        )
        self.log.add_event(event)

    def record_activation_start(
        self,
        unit_name: str,
        action: str,
        is_fatigued: bool = False,
    ) -> None:
        """Record unit activation starting."""
        self._active_unit = unit_name
        fatigue_str = " (fatigued - only hits on 6s)" if is_fatigued else ""
        event = self._create_event(
            CombatEventType.ACTIVATION_START,
            f"{unit_name} activates to {action}{fatigue_str}",
            extra_data={
                "action": action,
                "is_fatigued": is_fatigued,
            },
        )
        self.log.add_event(event)

    def record_activation_end(self, unit_name: str) -> None:
        """Record activation ending."""
        event = self._create_event(
            CombatEventType.ACTIVATION_END,
            f"{unit_name} finishes activation",
        )
        self.log.add_event(event)
        self._active_unit = ""

    def record_rally(self, unit_name: str) -> None:
        """Record unit rallying."""
        event = self._create_event(
            CombatEventType.UNIT_RALLIES,
            f"{unit_name} rallies and removes Shaken status",
        )
        self.log.add_event(event)

    def record_attack_start(
        self,
        attacker_name: str,
        defender_name: str,
        phase: str,
        is_charging: bool,
    ) -> None:
        """Record attack sequence starting."""
        # Update active and target units to match the current attack
        self._active_unit = attacker_name
        self._target_unit = defender_name
        charge_str = " (charging)" if is_charging else ""
        event = self._create_event(
            CombatEventType.ATTACK_START,
            f"{attacker_name} attacks {defender_name} in {phase}{charge_str}",
            extra_data={
                "phase": phase,
                "is_charging": is_charging,
            },
        )
        self.log.add_event(event)

    def record_attack_end(
        self,
        total_wounds: int,
        models_killed: int,
    ) -> None:
        """Record attack sequence ending."""
        event = self._create_event(
            CombatEventType.ATTACK_END,
            f"Attack complete: {total_wounds} wounds dealt, {models_killed} models killed",
            extra_data={
                "total_wounds": total_wounds,
                "models_killed": models_killed,
            },
        )
        self.log.add_event(event)
        self._target_unit = ""

    def record_weapon_attack_start(
        self,
        weapon_name: str,
        attacks: int,
        model_name: str,
    ) -> None:
        """Record weapon attack starting."""
        event = self._create_event(
            CombatEventType.WEAPON_ATTACK_START,
            f"{model_name} attacks with {weapon_name} ({attacks} attacks)",
            extra_data={
                "weapon_name": weapon_name,
                "attacks": attacks,
                "model_name": model_name,
            },
        )
        self.log.add_event(event)

    def record_weapon_attack_end(
        self,
        weapon_name: str,
        wounds_dealt: int,
    ) -> None:
        """Record weapon attack ending."""
        event = self._create_event(
            CombatEventType.WEAPON_ATTACK_END,
            f"{weapon_name} deals {wounds_dealt} wounds",
            extra_data={
                "weapon_name": weapon_name,
                "wounds_dealt": wounds_dealt,
            },
        )
        self.log.add_event(event)

    def record_hit_roll(
        self,
        dice_values: list[int] | NDArray[np.int_],
        target: int,
        successes: int,
        is_fatigued: bool = False,
    ) -> None:
        """Record hit roll."""
        if isinstance(dice_values, np.ndarray):
            dice_list = dice_values.tolist()
        else:
            dice_list = list(dice_values)

        if is_fatigued:
            desc = f"Hit roll (fatigued - need 6s): {dice_list} -> {successes} hits"
        else:
            desc = f"Hit roll (need {target}+): {dice_list} -> {successes} hits"

        event = self._create_event(
            CombatEventType.HIT_ROLL,
            desc,
            dice_values=dice_list,
            dice_target=target,
            dice_successes=successes,
            dice_failures=len(dice_list) - successes,
            extra_data={"is_fatigued": is_fatigued},
        )
        self.log.add_event(event)

    def record_hit_modifier(
        self,
        rule_name: str,
        original_hits: int,
        modified_hits: int,
        effect_description: str,
    ) -> None:
        """Record hit modifier being applied (Blast, Furious, etc.)."""
        event = self._create_event(
            CombatEventType.HIT_MODIFIER_APPLIED,
            f"{rule_name}: {effect_description} ({original_hits} -> {modified_hits} hits)",
            rule_name=rule_name,
            rule_effect=effect_description,
            extra_data={
                "original_hits": original_hits,
                "modified_hits": modified_hits,
            },
        )
        self.log.add_event(event)

    def record_defense_roll(
        self,
        dice_values: list[int] | NDArray[np.int_],
        target: int,
        ap: int,
        saves: int,
        wounds: int,
        rerolled_sixes: bool = False,
    ) -> None:
        """Record defense roll."""
        if isinstance(dice_values, np.ndarray):
            dice_list = dice_values.tolist()
        else:
            dice_list = list(dice_values)

        effective_target = max(2, min(6, target + ap))
        reroll_str = " (6s rerolled due to Poison/Bane)" if rerolled_sixes else ""
        event = self._create_event(
            CombatEventType.DEFENSE_ROLL,
            f"Defense roll (need {effective_target}+, base {target}+ with AP {ap}){reroll_str}: "
            f"{dice_list} -> {saves} saves, {wounds} wounds through",
            dice_values=dice_list,
            dice_target=effective_target,
            dice_successes=saves,
            dice_failures=wounds,
            extra_data={
                "ap": ap,
                "base_target": target,
                "rerolled_sixes": rerolled_sixes,
            },
        )
        self.log.add_event(event)

    def record_wound_modifier(
        self,
        rule_name: str,
        original_wounds: int,
        modified_wounds: int,
        effect_description: str,
    ) -> None:
        """Record wound modifier being applied (Deadly, etc.)."""
        event = self._create_event(
            CombatEventType.WOUND_MODIFIER_APPLIED,
            f"{rule_name}: {effect_description} ({original_wounds} -> {modified_wounds} wounds)",
            rule_name=rule_name,
            rule_effect=effect_description,
            extra_data={
                "original_wounds": original_wounds,
                "modified_wounds": modified_wounds,
            },
        )
        self.log.add_event(event)

    def record_rule_triggered(
        self,
        rule_name: str,
        rule_value: int | None,
        effect_description: str,
    ) -> None:
        """Record a special rule being triggered."""
        value_str = f"({rule_value})" if rule_value else ""
        event = self._create_event(
            CombatEventType.RULE_TRIGGERED,
            f"Rule triggered: {rule_name}{value_str} - {effect_description}",
            rule_name=rule_name,
            rule_value=rule_value,
            rule_effect=effect_description,
        )
        self.log.add_event(event)

    def record_wound_allocated(
        self,
        model_name: str,
        wounds: int,
        wounds_before: int,
        wounds_after: int,
        tough: int,
    ) -> None:
        """Record wound being allocated to a model."""
        remaining = tough - wounds_after
        event = self._create_event(
            CombatEventType.WOUND_ALLOCATED,
            f"{model_name} takes {wounds} wound(s) ({wounds_before}/{tough} -> {wounds_after}/{tough}, "
            f"{remaining} remaining)",
            model_name=model_name,
            wounds_before=wounds_before,
            wounds_after=wounds_after,
            extra_data={
                "wounds_applied": wounds,
                "tough": tough,
                "remaining": remaining,
            },
        )
        self.log.add_event(event)

    def record_model_killed(self, model_name: str, overkill: int = 0) -> None:
        """Record a model being killed."""
        overkill_str = f" ({overkill} overkill wounds)" if overkill > 0 else ""
        event = self._create_event(
            CombatEventType.MODEL_KILLED,
            f"{model_name} is killed!{overkill_str}",
            model_name=model_name,
            extra_data={"overkill": overkill},
        )
        self.log.add_event(event)

    def record_regeneration_roll(
        self,
        model_name: str,
        roll: int,
        target: int,
        success: bool,
    ) -> None:
        """Record a regeneration roll."""
        result = "wound ignored" if success else "wound taken"
        event = self._create_event(
            CombatEventType.REGENERATION_ROLL,
            f"{model_name} Regeneration roll (need {target}+): rolled {roll} -> {result}",
            model_name=model_name,
            dice_values=[roll],
            dice_target=target,
            dice_successes=1 if success else 0,
            dice_failures=0 if success else 1,
        )
        self.log.add_event(event)

    def record_protected_roll(
        self,
        model_name: str,
        roll: int,
        target: int,
        success: bool,
    ) -> None:
        """Record a Protected roll."""
        result = "wound ignored" if success else "wound taken"
        event = self._create_event(
            CombatEventType.PROTECTED_ROLL,
            f"{model_name} Protected roll (need {target}+): rolled {roll} -> {result}",
            model_name=model_name,
            dice_values=[roll],
            dice_target=target,
            dice_successes=1 if success else 0,
            dice_failures=0 if success else 1,
        )
        self.log.add_event(event)

    def record_morale_test(
        self,
        unit_name: str,
        roll: int,
        target: int,
        reason: str,
        result: str,
    ) -> None:
        """Record a morale test."""
        success = roll >= target
        event = self._create_event(
            CombatEventType.MORALE_TEST,
            f"{unit_name} morale test ({reason}, need {target}+): "
            f"rolled {roll} -> {result}",
            dice_values=[roll],
            dice_target=target,
            dice_successes=1 if success else 0,
            dice_failures=0 if success else 1,
            extra_data={
                "reason": reason,
                "result": result,
            },
        )
        self.log.add_event(event)

    def record_unit_shaken(self, unit_name: str) -> None:
        """Record unit becoming shaken."""
        event = self._create_event(
            CombatEventType.UNIT_SHAKEN,
            f"{unit_name} becomes Shaken! (-1 to Quality and Defense)",
        )
        self.log.add_event(event)

    def record_unit_routed(self, unit_name: str) -> None:
        """Record unit routing."""
        event = self._create_event(
            CombatEventType.UNIT_ROUTED,
            f"{unit_name} is Routed and flees the battle!",
        )
        self.log.add_event(event)

    def record_unit_fatigued(self, unit_name: str) -> None:
        """Record unit becoming fatigued."""
        event = self._create_event(
            CombatEventType.UNIT_FATIGUED,
            f"{unit_name} becomes Fatigued (only hits on unmodified 6s in melee)",
        )
        self.log.add_event(event)

    def record_strike_back_start(self, unit_name: str, is_fatigued: bool) -> None:
        """Record strike back starting.

        Updates _active_unit and _target_unit so that subsequent wound/kill
        events correctly reflect the striking unit as the attacker.
        """
        # Save original values to restore after strike back ends
        self._pre_strike_back_active_unit = self._active_unit
        self._pre_strike_back_target_unit = self._target_unit

        # Swap: the unit that was being attacked is now attacking
        self._target_unit = self._active_unit  # Original attacker is now target
        self._active_unit = unit_name  # Striking unit is now active

        fatigue_str = " (as fatigued)" if is_fatigued else ""
        event = self._create_event(
            CombatEventType.STRIKE_BACK_START,
            f"{unit_name} strikes back{fatigue_str}!",
            extra_data={"is_fatigued": is_fatigued},
        )
        self.log.add_event(event)

    def record_strike_back_end(
        self,
        unit_name: str,
        wounds_dealt: int,
        models_killed: int,
    ) -> None:
        """Record strike back ending.

        Restores _active_unit and _target_unit to their pre-strike-back values.
        """
        event = self._create_event(
            CombatEventType.STRIKE_BACK_END,
            f"{unit_name} strike back complete: {wounds_dealt} wounds, {models_killed} kills",
            extra_data={
                "wounds_dealt": wounds_dealt,
                "models_killed": models_killed,
            },
        )
        self.log.add_event(event)

        # Restore original active/target units
        if hasattr(self, "_pre_strike_back_active_unit"):
            self._active_unit = self._pre_strike_back_active_unit
            del self._pre_strike_back_active_unit
        if hasattr(self, "_pre_strike_back_target_unit"):
            self._target_unit = self._pre_strike_back_target_unit
            del self._pre_strike_back_target_unit

    def get_log(self) -> CombatEventLog:
        """Get the completed event log."""
        return self.log
