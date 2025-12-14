"""Dice rolling engine using NumPy for efficient vectorized operations."""

import numpy as np
from numpy.typing import NDArray


class DiceRoller:
    """High-performance dice roller using NumPy.

    Uses vectorized operations to roll thousands of dice efficiently
    for Monte Carlo simulations.
    """

    def __init__(self, seed: int | None = None):
        """Initialize the dice roller.

        Args:
            seed: Optional random seed for reproducibility
        """
        self.rng = np.random.default_rng(seed)

    def roll_d6(self, count: int = 1) -> NDArray[np.int_]:
        """Roll multiple D6 dice.

        Args:
            count: Number of dice to roll

        Returns:
            Array of dice results (1-6)
        """
        if count <= 0:
            return np.array([], dtype=np.int_)
        return self.rng.integers(1, 7, size=count)

    def roll_d6_target(self, count: int, target: int) -> tuple[int, NDArray[np.int_]]:
        """Roll dice and count successes against a target number.

        Args:
            count: Number of dice to roll
            target: Target number to meet or exceed (e.g., 4 for 4+)

        Returns:
            Tuple of (number of successes, array of all rolls)
        """
        if count <= 0:
            return 0, np.array([], dtype=np.int_)

        rolls = self.roll_d6(count)
        successes = int(np.sum(rolls >= target))
        return successes, rolls

    def roll_quality_test(
        self, attacks: int, quality: int, modifier: int = 0
    ) -> tuple[int, NDArray[np.int_]]:
        """Roll to hit with quality modifier.

        Args:
            attacks: Number of attacks (dice to roll)
            quality: Quality value (2-6, roll this or higher)
            modifier: Modifier to the roll (+1 for aiming, -1 for moving, etc.)

        Returns:
            Tuple of (hits, all rolls)
        """
        # Clamp to 2-6: 1s always fail, 6s always succeed
        target = max(2, min(6, quality - modifier))
        return self.roll_d6_target(attacks, target)

    def roll_defense_test(
        self, hits: int, defense: int, ap: int = 0, modifier: int = 0,
        reroll_sixes: bool = False
    ) -> tuple[int, NDArray[np.int_]]:
        """Roll defense saves.

        Args:
            hits: Number of hits to save against
            defense: Defense value (2-6, roll this or higher)
            ap: Armor Piercing value (reduces defense)
            modifier: Additional modifier (cover gives +1, shaken gives -1)
            reroll_sixes: If True, reroll unmodified results of 6 (Poison)

        Returns:
            Tuple of (wounds taken, all rolls)
        """
        # AP increases the target number needed
        # Modifier: positive is good for defender (cover), negative is bad (shaken)
        effective_defense = defense + ap - modifier

        # Clamp to valid range: rolls of 1 always fail, rolls of 6 always succeed
        # Target cannot go below 2 (1s always fail) or above 6 (6s always succeed)
        effective_defense = max(2, min(6, effective_defense))

        successes, rolls = self.roll_d6_target(hits, effective_defense)

        # Poison: Reroll unmodified results of 6 (successful saves on 6)
        if reroll_sixes and len(rolls) > 0:
            sixes_mask = rolls == 6
            num_sixes = np.sum(sixes_mask)
            if num_sixes > 0:
                # Reroll the 6s
                rerolls = self.roll_d6(int(num_sixes))
                rolls[sixes_mask] = rerolls
                # Recalculate successes
                successes = int(np.sum(rolls >= effective_defense))

        wounds = hits - successes  # Failed saves = wounds
        return wounds, rolls

    def roll_regeneration(self, wounds: int, target: int = 5) -> tuple[int, NDArray[np.int_]]:
        """Roll regeneration saves.

        Args:
            wounds: Number of wounds to try to regenerate
            target: Target to regenerate (default 5+)

        Returns:
            Tuple of (wounds remaining after regen, all rolls)
        """
        if wounds <= 0:
            return 0, np.array([], dtype=np.int_)

        successes, rolls = self.roll_d6_target(wounds, target)
        remaining_wounds = wounds - successes
        return remaining_wounds, rolls

    def count_sixes(self, rolls: NDArray[np.int_]) -> int:
        """Count natural 6s in a set of rolls.

        Used for Rending and other "on 6" effects.

        Args:
            rolls: Array of dice results

        Returns:
            Number of 6s rolled
        """
        return int(np.sum(rolls == 6))

    def count_ones(self, rolls: NDArray[np.int_]) -> int:
        """Count natural 1s in a set of rolls.

        Used for effects that trigger on 1s.

        Args:
            rolls: Array of dice results

        Returns:
            Number of 1s rolled
        """
        return int(np.sum(rolls == 1))

    def reroll_failures(
        self, rolls: NDArray[np.int_], target: int
    ) -> tuple[NDArray[np.int_], int]:
        """Reroll failed dice.

        Args:
            rolls: Original dice results
            target: Target number

        Returns:
            Tuple of (new rolls array with rerolls, additional successes from rerolls)
        """
        failures = rolls < target
        num_rerolls = int(np.sum(failures))

        if num_rerolls == 0:
            return rolls, 0

        rerolls = self.roll_d6(num_rerolls)
        new_successes = int(np.sum(rerolls >= target))

        # Create new array with rerolls substituted
        new_rolls = rolls.copy()
        new_rolls[failures] = rerolls

        return new_rolls, new_successes

    def reroll_ones(self, rolls: NDArray[np.int_], target: int) -> tuple[NDArray[np.int_], int]:
        """Reroll only 1s.

        Args:
            rolls: Original dice results
            target: Target number for determining additional successes

        Returns:
            Tuple of (new rolls array with rerolls, additional successes from rerolls)
        """
        ones = rolls == 1
        num_rerolls = int(np.sum(ones))

        if num_rerolls == 0:
            return rolls, 0

        rerolls = self.roll_d6(num_rerolls)
        new_successes = int(np.sum(rerolls >= target))

        new_rolls = rolls.copy()
        new_rolls[ones] = rerolls

        return new_rolls, new_successes


def get_dice_roller(seed: int | None = None) -> DiceRoller:
    """Create a new dice roller instance.

    This function always creates a new DiceRoller instance to ensure
    thread-safety in concurrent environments. Each simulation/request
    should use its own dice roller to avoid race conditions.

    Args:
        seed: Optional seed for reproducibility

    Returns:
        A new DiceRoller instance
    """
    return DiceRoller(seed)


def roll_d6(count: int = 1) -> NDArray[np.int_]:
    """Convenience function to roll D6s using default roller."""
    return get_dice_roller().roll_d6(count)


def roll_quality(attacks: int, quality: int, modifier: int = 0) -> tuple[int, NDArray[np.int_]]:
    """Convenience function for quality tests."""
    return get_dice_roller().roll_quality_test(attacks, quality, modifier)


def roll_defense(
    hits: int, defense: int, ap: int = 0, modifier: int = 0
) -> tuple[int, NDArray[np.int_]]:
    """Convenience function for defense tests."""
    return get_dice_roller().roll_defense_test(hits, defense, ap, modifier)
