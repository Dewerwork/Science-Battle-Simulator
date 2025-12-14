"""Morale system following OPR rules."""

from dataclasses import dataclass
from enum import Enum

from src.engine.combat_state import UnitCombatState, UnitStatus
from src.engine.dice import get_dice_roller
from src.models.unit import Unit


class MoraleTrigger(str, Enum):
    """What triggered the morale test."""

    CASUALTIES = "casualties"  # Unit reduced to half strength
    LOST_MELEE = "lost_melee"  # Unit dealt fewer wounds in melee


class MoraleOutcome(str, Enum):
    """Result of a morale test."""

    PASSED = "passed"
    SHAKEN = "shaken"
    ROUTED = "routed"
    NOT_REQUIRED = "not_required"


@dataclass
class MoraleTestResult:
    """Result of a morale test."""

    trigger: MoraleTrigger
    outcome: MoraleOutcome
    roll: int
    target: int
    was_at_half_strength: bool
    fearless_reroll_used: bool = False
    fearless_reroll_value: int | None = None

    @property
    def passed(self) -> bool:
        """Check if the test was passed."""
        return self.outcome == MoraleOutcome.PASSED

    @property
    def failed(self) -> bool:
        """Check if the test was failed."""
        return self.outcome in (MoraleOutcome.SHAKEN, MoraleOutcome.ROUTED)


class MoraleSystem:
    """Handles morale tests according to OPR rules.

    Morale tests are taken when:
    1. Wounds leave a unit at half or less of its starting size/tough value
    2. A unit loses a melee (dealt fewer wounds than it took)

    A failed morale test results in:
    - Shaken status (if not already shaken and not at half strength after losing melee)
    - Rout (removed from play) if at half strength or below when failing

    Units that are already Shaken automatically fail morale tests.
    """

    def __init__(self):
        self.dice = get_dice_roller()

    def check_casualty_morale(
        self,
        unit_state: UnitCombatState,
        wounds_just_taken: int,
    ) -> MoraleTestResult | None:
        """Check if a morale test is needed due to casualties.

        Per OPR rules: morale test when wounds leave unit at half or less
        of its total size or tough value.

        Args:
            unit_state: The unit's combat state
            wounds_just_taken: Wounds taken in this activation

        Returns:
            MoraleTestResult if test was taken, None if no test needed
        """
        # Only test if wounds were actually taken
        if wounds_just_taken <= 0:
            return None

        # Only test if now at half strength (and wasn't before these wounds)
        # We track this by checking if we're at half strength now
        if not unit_state.is_at_half_strength:
            return None

        return self._take_morale_test(
            unit_state,
            MoraleTrigger.CASUALTIES,
        )

    def check_melee_morale(
        self,
        loser_state: UnitCombatState,
        wounds_dealt: int,
        wounds_taken: int,
    ) -> MoraleTestResult | None:
        """Check morale after losing a melee.

        The unit that dealt fewer wounds must take a morale test.

        Args:
            loser_state: The losing unit's combat state
            wounds_dealt: Wounds the loser dealt
            wounds_taken: Wounds the loser took

        Returns:
            MoraleTestResult if test was taken, None if no test needed
        """
        # Only the loser tests (dealt fewer wounds)
        if wounds_dealt >= wounds_taken:
            return None

        return self._take_morale_test(
            loser_state,
            MoraleTrigger.LOST_MELEE,
        )

    def _take_morale_test(
        self,
        unit_state: UnitCombatState,
        trigger: MoraleTrigger,
    ) -> MoraleTestResult:
        """Take a morale test.

        Args:
            unit_state: The unit's combat state
            trigger: What triggered this test

        Returns:
            MoraleTestResult with the outcome
        """
        unit = unit_state.unit
        at_half = unit_state.is_at_half_strength

        # Already shaken units automatically fail
        if unit_state.status == UnitStatus.SHAKEN:
            outcome = MoraleOutcome.ROUTED if at_half else MoraleOutcome.SHAKEN
            return MoraleTestResult(
                trigger=trigger,
                outcome=outcome,
                roll=0,
                target=unit.base_quality,
                was_at_half_strength=at_half,
            )

        # Roll quality test
        target = unit.base_quality
        roll = int(self.dice.roll_d6(1)[0])
        passed = roll >= target

        # Check for Fearless reroll
        fearless_used = False
        fearless_roll = None

        if not passed and self._unit_has_fearless(unit):
            fearless_roll = int(self.dice.roll_d6(1)[0])
            fearless_used = True
            if fearless_roll >= 4:  # Fearless: on 4+ counts as passed
                passed = True

        if passed:
            outcome = MoraleOutcome.PASSED
        elif at_half and trigger == MoraleTrigger.LOST_MELEE:
            # Losing melee while at half strength = rout
            outcome = MoraleOutcome.ROUTED
        else:
            outcome = MoraleOutcome.SHAKEN

        result = MoraleTestResult(
            trigger=trigger,
            outcome=outcome,
            roll=roll,
            target=target,
            was_at_half_strength=at_half,
            fearless_reroll_used=fearless_used,
            fearless_reroll_value=fearless_roll,
        )

        # Apply the outcome to unit state
        if outcome == MoraleOutcome.SHAKEN:
            unit_state.become_shaken()
        elif outcome == MoraleOutcome.ROUTED:
            unit_state.rout()

        return result

    def _unit_has_fearless(self, unit: Unit) -> bool:
        """Check if all models in the unit have Fearless."""
        # Unit-wide rule
        if unit.has_rule("Fearless"):
            return True

        # All models must have it
        for model in unit.alive_models:
            if not model.has_rule("Fearless"):
                return False

        return len(unit.alive_models) > 0


def check_casualty_morale(
    unit_state: UnitCombatState,
    wounds_taken: int,
) -> MoraleTestResult | None:
    """Convenience function for casualty morale check."""
    system = MoraleSystem()
    return system.check_casualty_morale(unit_state, wounds_taken)


def check_melee_morale(
    loser_state: UnitCombatState,
    wounds_dealt: int,
    wounds_taken: int,
) -> MoraleTestResult | None:
    """Convenience function for melee morale check."""
    system = MoraleSystem()
    return system.check_melee_morale(loser_state, wounds_dealt, wounds_taken)
