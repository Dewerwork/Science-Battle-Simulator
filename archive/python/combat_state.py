"""Combat state tracking for multi-round battles."""

from dataclasses import dataclass, field
from enum import Enum

from src.models.unit import Unit


class UnitStatus(str, Enum):
    """Status conditions for a unit."""

    NORMAL = "normal"
    SHAKEN = "shaken"
    ROUTED = "routed"  # Removed from play


@dataclass
class UnitCombatState:
    """Tracks combat state for a single unit across rounds."""

    unit: Unit
    status: UnitStatus = UnitStatus.NORMAL
    is_fatigued: bool = False
    is_engaged_in_melee: bool = False
    has_charged_this_round: bool = False
    has_struck_back_this_round: bool = False
    wounds_dealt_this_activation: int = 0
    wounds_taken_this_activation: int = 0

    @property
    def starting_model_count(self) -> int:
        """Get the starting model count (total models, alive or dead)."""
        return self.unit.model_count

    @property
    def starting_tough_value(self) -> int:
        """Get total starting tough value for single-model units."""
        return sum(m.tough for m in self.unit.models)

    @property
    def current_tough_value(self) -> int:
        """Get current tough value remaining."""
        return self.unit.total_wounds_remaining

    @property
    def is_at_half_strength(self) -> bool:
        """Check if unit is at half or less of starting strength.

        For multi-model units: compare model count.
        For single-model units (Tough): compare tough value.
        """
        if self.unit.model_count == 1:
            # Single model unit - check tough value
            starting = self.starting_tough_value
            current = self.current_tough_value
            return current <= starting // 2
        else:
            # Multi-model unit - check model count
            return self.unit.alive_count <= self.starting_model_count // 2

    @property
    def is_destroyed(self) -> bool:
        """Check if unit is destroyed."""
        return self.unit.is_destroyed

    @property
    def is_out_of_action(self) -> bool:
        """Check if unit is destroyed or routed."""
        return self.is_destroyed or self.status == UnitStatus.ROUTED

    def apply_fatigue(self) -> None:
        """Mark unit as fatigued (after charging or striking back)."""
        self.is_fatigued = True

    def reset_round_state(self) -> None:
        """Reset per-round state at the start of a new round."""
        self.is_fatigued = False
        self.has_charged_this_round = False
        self.has_struck_back_this_round = False
        self.wounds_dealt_this_activation = 0
        self.wounds_taken_this_activation = 0

    def become_shaken(self) -> None:
        """Set unit status to shaken."""
        self.status = UnitStatus.SHAKEN

    def rally(self) -> None:
        """Remove shaken status (after spending activation idle)."""
        if self.status == UnitStatus.SHAKEN:
            self.status = UnitStatus.NORMAL

    def rout(self) -> None:
        """Unit routs and is removed from play."""
        self.status = UnitStatus.ROUTED


@dataclass
class CombatStateManager:
    """Manages combat state for all units in a multi-round battle."""

    attacker_state: UnitCombatState
    defender_state: UnitCombatState
    current_round: int = 1
    max_rounds: int = 10  # Safety limit

    @classmethod
    def create(
        cls,
        attacker: Unit,
        defender: Unit,
        max_rounds: int = 10,
    ) -> "CombatStateManager":
        """Create a new combat state manager with fresh unit copies."""
        attacker_copy = attacker.copy_fresh()
        defender_copy = defender.copy_fresh()

        # Validate units and print warnings for potential issues
        for unit in [attacker_copy, defender_copy]:
            warnings = unit.validate_equipment()
            for warning in warnings:
                print(f"WARNING [{unit.name}]: {warning}")

        return cls(
            attacker_state=UnitCombatState(unit=attacker_copy),
            defender_state=UnitCombatState(unit=defender_copy),
            current_round=1,
            max_rounds=max_rounds,
        )

    @property
    def attacker(self) -> Unit:
        """Get the attacker unit."""
        return self.attacker_state.unit

    @property
    def defender(self) -> Unit:
        """Get the defender unit."""
        return self.defender_state.unit

    @property
    def is_battle_over(self) -> bool:
        """Check if the battle has ended."""
        if self.current_round > self.max_rounds:
            return True
        if self.attacker_state.is_out_of_action:
            return True
        if self.defender_state.is_out_of_action:
            return True
        return False

    def start_new_round(self) -> None:
        """Start a new round, resetting per-round state."""
        self.current_round += 1
        self.attacker_state.reset_round_state()
        self.defender_state.reset_round_state()

    def get_state(self, unit: Unit) -> UnitCombatState:
        """Get the combat state for a unit."""
        if unit is self.attacker:
            return self.attacker_state
        return self.defender_state

    def get_opponent_state(self, unit: Unit) -> UnitCombatState:
        """Get the combat state for the opponent of a unit."""
        if unit is self.attacker:
            return self.defender_state
        return self.attacker_state
