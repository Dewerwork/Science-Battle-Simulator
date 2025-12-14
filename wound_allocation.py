"""Wound allocation system following OPR rules.

Optimized for performance with batched operations where possible.
"""

from dataclasses import dataclass

from src.models.unit import Model, Unit


@dataclass
class WoundAllocationResult:
    """Result of allocating wounds to a unit."""

    wounds_allocated: int
    models_killed: int
    wounds_regenerated: int
    overkill_wounds: int  # Wounds that couldn't be allocated (unit destroyed)
    allocation_details: list[tuple[str, int, bool]]  # (model_name, wounds, died)


class WoundAllocationManager:
    """Manages wound allocation following OPR rules.

    Allocation order:
    1. Non-tough models first (1 wound = 1 kill)
    2. Tough models (continue putting wounds on most wounded until dead)
    3. Heroes are assigned wounds last

    Optimized with batched operations for non-tough models and
    vectorized dice rolls for saves.
    """

    def allocate_wounds(
        self,
        unit: Unit,
        wounds: int,
        allow_regeneration: bool = True,
    ) -> WoundAllocationResult:
        """Allocate wounds to a unit following OPR rules.

        Uses optimized batching for non-tough models to reduce iterations.

        Args:
            unit: The unit taking wounds
            wounds: Number of wounds to allocate
            allow_regeneration: Whether to allow regeneration saves

        Returns:
            WoundAllocationResult with details of allocation
        """
        if wounds <= 0:
            return WoundAllocationResult(
                wounds_allocated=0,
                models_killed=0,
                wounds_regenerated=0,
                overkill_wounds=0,
                allocation_details=[],
            )

        remaining_wounds = wounds
        models_killed = 0
        wounds_regenerated = 0
        allocation_details: list[tuple[str, int, bool]] = []

        # Check unit-wide special rules once (avoid repeated lookups)
        unit_has_protected = unit.has_rule("Protected")
        unit_has_regen = unit.has_rule("Regeneration")
        unit_protected_target = self._get_rule_target(unit.get_rule("Protected"), 6)
        unit_regen_target = self._get_rule_target(unit.get_rule("Regeneration"), 5)

        # Process wounds - use batched allocation for non-tough models
        while remaining_wounds > 0:
            allocation_order = unit.get_wound_allocation_order()

            if not allocation_order:
                # Unit destroyed, remaining wounds are overkill
                break

            target_model = allocation_order[0]

            # OPTIMIZATION: Batch process non-tough models (tough=1, non-hero)
            if target_model.tough == 1 and not target_model.is_hero:
                batch_result = self._allocate_batch_non_tough(
                    unit=unit,
                    wounds=remaining_wounds,
                    allow_regeneration=allow_regeneration,
                    unit_has_protected=unit_has_protected,
                    unit_has_regen=unit_has_regen,
                    unit_protected_target=unit_protected_target,
                    unit_regen_target=unit_regen_target,
                )
                remaining_wounds -= batch_result[0]  # wounds_consumed
                models_killed += batch_result[1]  # kills
                wounds_regenerated += batch_result[2]  # saves
                allocation_details.extend(batch_result[3])  # details
                continue

            # For tough models and heroes, process individually
            wounds_to_apply = min(remaining_wounds, target_model.remaining_wounds)
            saves_made = 0

            # Check model-specific rules
            model_has_protected = target_model.has_rule("Protected") or unit_has_protected
            model_has_regen = target_model.has_rule("Regeneration") or unit_has_regen

            if model_has_protected or model_has_regen:
                # Use batched saves for this model's wounds
                protected_target = self._get_rule_target(target_model.get_rule("Protected"), unit_protected_target)
                regen_target = self._get_rule_target(target_model.get_rule("Regeneration"), unit_regen_target)

                wounds_after_saves, saves_made = self._roll_saves_batch(
                    wounds=wounds_to_apply,
                    has_protected=model_has_protected,
                    protected_target=protected_target,
                    has_regen=model_has_regen and allow_regeneration,
                    regen_target=regen_target,
                )
                wounds_regenerated += saves_made
                remaining_wounds -= saves_made  # Account for saved wounds
                wounds_to_apply = wounds_after_saves

            # Apply wounds to this model
            model_died = False
            wounds_on_this_model = 0

            for _ in range(wounds_to_apply):
                model_died = target_model.apply_wound()
                wounds_on_this_model += 1
                remaining_wounds -= 1

                if model_died:
                    models_killed += 1
                    break

            if wounds_on_this_model > 0:
                allocation_details.append(
                    (target_model.name, wounds_on_this_model, model_died)
                )

        overkill = remaining_wounds  # Any wounds left over

        return WoundAllocationResult(
            wounds_allocated=wounds - overkill,
            models_killed=models_killed,
            wounds_regenerated=wounds_regenerated,
            overkill_wounds=overkill,
            allocation_details=allocation_details,
        )

    def _get_rule_target(self, rule, default: int) -> int:
        """Get target value from a rule, or default if not present."""
        if rule and rule.value:
            return rule.value
        return default

    def _allocate_batch_non_tough(
        self,
        unit: Unit,
        wounds: int,
        allow_regeneration: bool,
        unit_has_protected: bool,
        unit_has_regen: bool,
        unit_protected_target: int,
        unit_regen_target: int,
    ) -> tuple[int, int, int, list[tuple[str, int, bool]]]:
        """Batch allocate wounds to non-tough models.

        Returns:
            Tuple of (wounds_consumed, models_killed, wounds_saved, allocation_details)
        """
        # Get all non-tough, non-hero alive models
        non_tough = [m for m in unit.alive_models if m.tough == 1 and not m.is_hero]
        if not non_tough:
            return (0, 0, 0, [])

        available_targets = len(non_tough)
        wounds_to_allocate = min(wounds, available_targets)

        # Roll saves in batch if needed
        wounds_saved = 0
        if unit_has_protected or (unit_has_regen and allow_regeneration):
            wounds_after_saves, wounds_saved = self._roll_saves_batch(
                wounds=wounds_to_allocate,
                has_protected=unit_has_protected,
                protected_target=unit_protected_target,
                has_regen=unit_has_regen and allow_regeneration,
                regen_target=unit_regen_target,
            )
            wounds_to_allocate = wounds_after_saves

        # Kill models (each wound kills one non-tough model)
        models_killed = min(wounds_to_allocate, available_targets)
        allocation_details: list[tuple[str, int, bool]] = []

        for i in range(models_killed):
            model = non_tough[i]
            model.apply_wound()  # Will set state to DEAD
            allocation_details.append((model.name, 1, True))

        wounds_consumed = models_killed + wounds_saved
        return (wounds_consumed, models_killed, wounds_saved, allocation_details)

    def _roll_saves_batch(
        self,
        wounds: int,
        has_protected: bool,
        protected_target: int,
        has_regen: bool,
        regen_target: int,
    ) -> tuple[int, int]:
        """Roll saves for multiple wounds in batch.

        Args:
            wounds: Number of wounds to save against
            has_protected: Whether Protected applies
            protected_target: Target for Protected (default 6)
            has_regen: Whether Regeneration applies
            regen_target: Target for Regeneration (default 5)

        Returns:
            Tuple of (wounds_remaining, saves_made)
        """
        from src.engine.dice import get_dice_roller

        dice = get_dice_roller()
        remaining = wounds
        total_saves = 0

        # Protected saves first (typically 6+)
        if has_protected and remaining > 0:
            successes, _ = dice.roll_d6_target(remaining, protected_target)
            total_saves += successes
            remaining -= successes

        # Regeneration saves (typically 5+)
        if has_regen and remaining > 0:
            successes, _ = dice.roll_d6_target(remaining, regen_target)
            total_saves += successes
            remaining -= successes

        return (remaining, total_saves)

    def _model_has_protected(self, model: Model, unit: Unit) -> bool:
        """Check if a model has Protected (from model or unit rules).

        Protected requires ALL models in the unit to have it.
        """
        # Check if unit has Protected rule (applies to all models)
        if unit.has_rule("Protected"):
            return True
        # Check if model itself has Protected
        if model.has_rule("Protected"):
            return True
        return False

    def _roll_protected(self, model: Model, unit: Unit) -> bool:
        """Roll Protected save for a single wound.

        Protected: Roll one die per wound, on 6+ it is ignored.

        Returns:
            True if wound was ignored
        """
        from src.engine.dice import get_dice_roller

        # Get target value (default 6+)
        protected_rule = model.get_rule("Protected") or unit.get_rule("Protected")
        target = 6  # Default is 6+
        if protected_rule and protected_rule.value:
            target = protected_rule.value

        roll = get_dice_roller().roll_d6(1)[0]
        return roll >= target

    def _model_has_regeneration(self, model: Model, unit: Unit) -> bool:
        """Check if a model has regeneration (from model or unit rules)."""
        if model.has_rule("Regeneration"):
            return True
        if unit.has_rule("Regeneration"):
            return True
        return False

    def _get_regeneration_target(self, model: Model, unit: Unit) -> int:
        """Get the regeneration target value (default 5+)."""
        # Check model first, then unit
        regen_rule = model.get_rule("Regeneration") or unit.get_rule("Regeneration")
        if regen_rule and regen_rule.value:
            return regen_rule.value
        return 5  # Default regeneration is 5+

    def _roll_regeneration(self, model: Model, unit: Unit) -> bool:
        """Roll regeneration for a single wound.

        Returns:
            True if wound was regenerated
        """
        from src.engine.dice import get_dice_roller

        target = self._get_regeneration_target(model, unit)
        roll = get_dice_roller().roll_d6(1)[0]
        return roll >= target

    def allocate_wounds_batch(
        self,
        unit: Unit,
        wounds: int,
        rending_modifier: int = 0,
    ) -> WoundAllocationResult:
        """Allocate wounds with Rending modifier considered.

        Rending causes -1 to regeneration rolls per OPR rules.

        Args:
            unit: The unit taking wounds
            wounds: Number of wounds to allocate
            rending_modifier: Penalty to regeneration from Rending

        Returns:
            WoundAllocationResult with details
        """
        # For now, use standard allocation
        # Rending modifier would be applied to regeneration rolls
        # This is handled in the combat resolver
        return self.allocate_wounds(unit, wounds)


def allocate_wounds(unit: Unit, wounds: int) -> WoundAllocationResult:
    """Convenience function for wound allocation."""
    manager = WoundAllocationManager()
    return manager.allocate_wounds(unit, wounds)
