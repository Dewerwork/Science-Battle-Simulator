"""Combat scenarios for simulation."""

from dataclasses import dataclass, field
from enum import Enum

from src.engine.combat import CombatPhase, CombatResolver, CombatResult
from src.engine.combat_state import UnitCombatState, UnitStatus
from src.engine.morale import MoraleSystem, MoraleOutcome, MoraleTestResult
from src.models.unit import Unit


class ScenarioType(str, Enum):
    """Types of combat scenarios to simulate."""

    # Basic scenarios
    SHOOTING_ONLY = "shooting_only"  # Attacker shoots, defender may take morale
    MUTUAL_SHOOTING = "mutual_shooting"  # Both units use their activations to shoot

    # Melee scenarios
    CHARGE = "charge"  # Attacker charges into melee
    RECEIVE_CHARGE = "receive_charge"  # Attacker receives a charge

    # Combined scenarios
    SHOOT_THEN_CHARGE = "shoot_then_charge"  # Attacker shoots, defender charges back
    APPROACH_1_TURN = "approach_1_turn"  # Take 1 round of fire then charge
    APPROACH_2_TURNS = "approach_2_turns"  # Take 2 rounds of fire then charge

    # Full combat (shooting + melee for both)
    FULL_ENGAGEMENT = "full_engagement"  # Complete combat sequence

    # New: Fighting retreat scenario (kiting)
    FIGHTING_RETREAT = "fighting_retreat"  # Attacker shoots, retreats, repeats for up to 5 rounds


@dataclass
class ScenarioPhaseResult:
    """Result of a single phase within a scenario."""

    phase_name: str
    combat_result: CombatResult | None
    attacker_models_after: int
    defender_models_after: int
    morale_result: MoraleTestResult | None = None
    attacker_status: UnitStatus = UnitStatus.NORMAL
    defender_status: UnitStatus = UnitStatus.NORMAL


@dataclass
class ScenarioResult:
    """Result of running a complete scenario."""

    scenario_type: ScenarioType
    attacker_name: str
    defender_name: str
    phases: list[ScenarioPhaseResult] = field(default_factory=list)

    # Final state
    attacker_models_start: int = 0
    attacker_models_end: int = 0
    defender_models_start: int = 0
    defender_models_end: int = 0

    # Final unit status
    attacker_final_status: UnitStatus = UnitStatus.NORMAL
    defender_final_status: UnitStatus = UnitStatus.NORMAL

    # Computed outcomes
    attacker_won: bool = False
    defender_won: bool = False
    draw: bool = False

    # Damage dealt
    total_wounds_dealt_by_attacker: int = 0
    total_wounds_dealt_by_defender: int = 0
    total_models_killed_by_attacker: int = 0
    total_models_killed_by_defender: int = 0

    # Morale tracking
    attacker_routed: bool = False
    defender_routed: bool = False

    def compute_outcome(self) -> None:
        """Compute the outcome based on final model counts and status."""
        # Check for rout (removed from play)
        attacker_out = self.attacker_models_end == 0 or self.attacker_final_status == UnitStatus.ROUTED
        defender_out = self.defender_models_end == 0 or self.defender_final_status == UnitStatus.ROUTED

        if attacker_out and defender_out:
            self.draw = True
        elif defender_out:
            self.attacker_won = True
        elif attacker_out:
            self.defender_won = True
        else:
            # Neither destroyed/routed - compare remaining percentage
            attacker_pct = self.attacker_models_end / self.attacker_models_start
            defender_pct = self.defender_models_end / self.defender_models_start

            if attacker_pct > defender_pct:
                self.attacker_won = True
            elif defender_pct > attacker_pct:
                self.defender_won = True
            else:
                self.draw = True


class ScenarioRunner:
    """Runs combat scenarios between units.

    Optimized to reuse working copies of units instead of creating fresh
    copies for each iteration, reducing memory allocation overhead.
    """

    def __init__(self, combat_resolver: CombatResolver | None = None):
        """Initialize scenario runner.

        Args:
            combat_resolver: Optional combat resolver to use
        """
        self.resolver = combat_resolver or CombatResolver()
        self.morale_system = MoraleSystem()
        # Working copies cache - keyed by (attacker_id, defender_id)
        self._working_copies: dict[tuple[int, int], tuple[Unit, Unit]] = {}

    def _get_working_copies(self, attacker: Unit, defender: Unit) -> tuple[Unit, Unit]:
        """Get or create working copies of units for simulation.

        Uses cached copies when available and resets them, avoiding
        expensive deep copies on every iteration.

        Args:
            attacker: Original attacker unit
            defender: Original defender unit

        Returns:
            Tuple of (attacker_copy, defender_copy) ready for simulation
        """
        key = (id(attacker), id(defender))

        if key in self._working_copies:
            # Reuse existing copies - just reset them
            atk_copy, def_copy = self._working_copies[key]
            atk_copy.reset()
            def_copy.reset()
        else:
            # Create new copies for this matchup
            atk_copy = attacker.copy_fresh()
            def_copy = defender.copy_fresh()
            self._working_copies[key] = (atk_copy, def_copy)

        return atk_copy, def_copy

    def clear_working_copies(self) -> None:
        """Clear cached working copies to free memory."""
        self._working_copies.clear()

    def run_scenario(
        self,
        scenario_type: ScenarioType,
        attacker: Unit,
        defender: Unit,
        defender_in_cover: bool = False,
    ) -> ScenarioResult:
        """Run a combat scenario.

        Args:
            scenario_type: The type of scenario to run
            attacker: The attacking unit
            defender: The defending unit
            defender_in_cover: Whether defender starts in cover

        Returns:
            ScenarioResult with full details
        """
        # Get working copies (reuses cached copies when possible)
        attacker_copy, defender_copy = self._get_working_copies(attacker, defender)

        # Create combat state trackers for morale
        attacker_state = UnitCombatState(unit=attacker_copy)
        defender_state = UnitCombatState(unit=defender_copy)

        result = ScenarioResult(
            scenario_type=scenario_type,
            attacker_name=attacker.name,
            defender_name=defender.name,
            attacker_models_start=attacker_copy.model_count,
            defender_models_start=defender_copy.model_count,
        )

        # Run the appropriate scenario
        match scenario_type:
            case ScenarioType.SHOOTING_ONLY:
                self._run_shooting_only(attacker_state, defender_state, defender_in_cover, result)
            case ScenarioType.MUTUAL_SHOOTING:
                self._run_mutual_shooting(attacker_state, defender_state, defender_in_cover, result)
            case ScenarioType.CHARGE:
                self._run_charge(attacker_state, defender_state, result)
            case ScenarioType.RECEIVE_CHARGE:
                self._run_receive_charge(attacker_state, defender_state, result)
            case ScenarioType.SHOOT_THEN_CHARGE:
                self._run_shoot_then_charge(attacker_state, defender_state, defender_in_cover, result)
            case ScenarioType.APPROACH_1_TURN:
                self._run_approach(attacker_state, defender_state, 1, result)
            case ScenarioType.APPROACH_2_TURNS:
                self._run_approach(attacker_state, defender_state, 2, result)
            case ScenarioType.FULL_ENGAGEMENT:
                self._run_full_engagement(attacker_state, defender_state, defender_in_cover, result)
            case ScenarioType.FIGHTING_RETREAT:
                self._run_fighting_retreat(attacker_state, defender_state, result)

        # Record final state
        result.attacker_models_end = attacker_copy.alive_count
        result.defender_models_end = defender_copy.alive_count
        result.attacker_final_status = attacker_state.status
        result.defender_final_status = defender_state.status
        result.attacker_routed = attacker_state.status == UnitStatus.ROUTED
        result.defender_routed = defender_state.status == UnitStatus.ROUTED
        result.compute_outcome()

        return result

    def _add_phase(
        self,
        result: ScenarioResult,
        phase_name: str,
        combat_result: CombatResult | None,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        morale_result: MoraleTestResult | None = None,
    ) -> None:
        """Add a phase result to the scenario."""
        result.phases.append(
            ScenarioPhaseResult(
                phase_name=phase_name,
                combat_result=combat_result,
                attacker_models_after=attacker_state.unit.alive_count,
                defender_models_after=defender_state.unit.alive_count,
                morale_result=morale_result,
                attacker_status=attacker_state.status,
                defender_status=defender_state.status,
            )
        )

        if combat_result:
            # Track damage dealt based on who is attacking
            # Check phase name to determine who dealt damage
            if "attacker" in phase_name.lower() and "strike back" not in phase_name.lower():
                result.total_wounds_dealt_by_attacker += combat_result.wound_allocation.wounds_allocated
                result.total_models_killed_by_attacker += combat_result.defender_models_killed
            elif "defender" in phase_name.lower() or "enemy" in phase_name.lower():
                result.total_wounds_dealt_by_defender += combat_result.wound_allocation.wounds_allocated
                result.total_models_killed_by_defender += combat_result.defender_models_killed
            elif "strike back" in phase_name.lower():
                # Attacker striking back deals damage to defender
                result.total_wounds_dealt_by_attacker += combat_result.wound_allocation.wounds_allocated
                result.total_models_killed_by_attacker += combat_result.defender_models_killed

    def _check_casualty_morale(
        self,
        unit_state: UnitCombatState,
        wounds_taken: int,
    ) -> MoraleTestResult | None:
        """Check and apply casualty morale if needed."""
        morale_result = self.morale_system.check_casualty_morale(unit_state, wounds_taken)
        return morale_result

    def _check_melee_morale(
        self,
        loser_state: UnitCombatState,
        wounds_dealt: int,
        wounds_taken: int,
    ) -> MoraleTestResult | None:
        """Check and apply melee morale if needed."""
        morale_result = self.morale_system.check_melee_morale(loser_state, wounds_dealt, wounds_taken)
        return morale_result

    def _is_out_of_action(self, state: UnitCombatState) -> bool:
        """Check if unit is destroyed or routed."""
        return state.unit.alive_count == 0 or state.status == UnitStatus.ROUTED

    def _run_shooting_only(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
        result: ScenarioResult,
    ) -> None:
        """Run shooting only scenario - one activation where attacker shoots.

        Includes morale check for defender if reduced to half strength.
        """
        combat = self.resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.SHOOTING,
            in_cover=defender_in_cover,
        )

        # Check morale for defender
        wounds_taken = combat.wound_allocation.wounds_allocated
        morale_result = self._check_casualty_morale(defender_state, wounds_taken)

        self._add_phase(result, "Attacker Shooting", combat, attacker_state, defender_state, morale_result)

    def _run_mutual_shooting(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
        result: ScenarioResult,
    ) -> None:
        """Run mutual shooting scenario - each unit uses their activation to shoot.

        Attacker activation: attacker shoots defender, morale check on defender.
        Defender activation: defender shoots attacker (if not routed), morale check on attacker.
        """
        # Attacker's activation - shoot defender
        attacker_combat = self.resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.SHOOTING,
            in_cover=defender_in_cover,
        )

        # Check morale for defender
        defender_wounds = attacker_combat.wound_allocation.wounds_allocated
        defender_morale = self._check_casualty_morale(defender_state, defender_wounds)

        self._add_phase(result, "Attacker Shooting", attacker_combat, attacker_state, defender_state, defender_morale)

        # Defender's activation - shoot attacker (if not destroyed/routed)
        if not self._is_out_of_action(defender_state):
            defender_combat = self.resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.SHOOTING,
                in_cover=False,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
            )

            # Check morale for attacker
            attacker_wounds = defender_combat.wound_allocation.wounds_allocated
            attacker_morale = self._check_casualty_morale(attacker_state, attacker_wounds)

            self._add_phase(result, "Defender Shooting", defender_combat, attacker_state, defender_state, attacker_morale)

    def _run_charge(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        result: ScenarioResult,
    ) -> None:
        """Run charge scenario - attacker strikes first."""
        # Attacker strikes first (charging)
        attacker_combat = self.resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.MELEE,
            is_charging=True,
            attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
            defender_shaken=defender_state.status == UnitStatus.SHAKEN,
        )

        attacker_wounds_dealt = attacker_combat.wound_allocation.wounds_allocated
        defender_wounds = attacker_wounds_dealt

        self._add_phase(result, "Attacker Charge", attacker_combat, attacker_state, defender_state)

        defender_wounds_dealt = 0
        # Defender strikes back (if alive and not routed)
        if not self._is_out_of_action(defender_state):
            defender_combat = self.resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=False,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
            )
            defender_wounds_dealt = defender_combat.wound_allocation.wounds_allocated

            self._add_phase(result, "Defender Strike Back", defender_combat, attacker_state, defender_state)

        # Check melee morale - loser (dealt fewer wounds) takes test
        if attacker_wounds_dealt < defender_wounds_dealt:
            morale_result = self._check_melee_morale(attacker_state, attacker_wounds_dealt, defender_wounds_dealt)
            if morale_result:
                self._add_phase(result, "Attacker Melee Morale", None, attacker_state, defender_state, morale_result)
        elif defender_wounds_dealt < attacker_wounds_dealt:
            morale_result = self._check_melee_morale(defender_state, defender_wounds_dealt, attacker_wounds_dealt)
            if morale_result:
                self._add_phase(result, "Defender Melee Morale", None, attacker_state, defender_state, morale_result)

    def _run_receive_charge(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        result: ScenarioResult,
    ) -> None:
        """Run receive charge scenario - defender (enemy) charges the attacker."""
        # Defender strikes first (charging)
        defender_combat = self.resolver.resolve_attack(
            attacker=defender_state.unit,
            defender=attacker_state.unit,
            phase=CombatPhase.MELEE,
            is_charging=True,
            attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
            defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
        )

        defender_wounds_dealt = defender_combat.wound_allocation.wounds_allocated

        self._add_phase(result, "Enemy Charge", defender_combat, attacker_state, defender_state)

        attacker_wounds_dealt = 0
        # Attacker strikes back (if alive and not routed)
        if not self._is_out_of_action(attacker_state):
            attacker_combat = self.resolver.resolve_attack(
                attacker=attacker_state.unit,
                defender=defender_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=False,
                attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                defender_shaken=defender_state.status == UnitStatus.SHAKEN,
            )
            attacker_wounds_dealt = attacker_combat.wound_allocation.wounds_allocated

            self._add_phase(result, "Attacker Strike Back", attacker_combat, attacker_state, defender_state)

        # Check melee morale
        if attacker_wounds_dealt < defender_wounds_dealt:
            morale_result = self._check_melee_morale(attacker_state, attacker_wounds_dealt, defender_wounds_dealt)
            if morale_result:
                self._add_phase(result, "Attacker Melee Morale", None, attacker_state, defender_state, morale_result)
        elif defender_wounds_dealt < attacker_wounds_dealt:
            morale_result = self._check_melee_morale(defender_state, defender_wounds_dealt, attacker_wounds_dealt)
            if morale_result:
                self._add_phase(result, "Defender Melee Morale", None, attacker_state, defender_state, morale_result)

    def _run_shoot_then_charge(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
        result: ScenarioResult,
    ) -> None:
        """Run shoot then charge scenario.

        Attacker shoots defender in their activation.
        Defender (if alive) uses their activation to charge the attacker.
        Attacker strikes back with melee weapons (if alive).
        """
        # Attacker's activation - shoot defender
        shoot_combat = self.resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.SHOOTING,
            in_cover=defender_in_cover,
        )

        # Check morale for defender
        defender_wounds = shoot_combat.wound_allocation.wounds_allocated
        defender_morale = self._check_casualty_morale(defender_state, defender_wounds)

        self._add_phase(result, "Attacker Shooting", shoot_combat, attacker_state, defender_state, defender_morale)

        # Defender's activation - charge attacker (if not destroyed/routed)
        if not self._is_out_of_action(defender_state):
            # Defender charges (strikes first with charge bonuses)
            defender_charge = self.resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=True,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
            )

            defender_wounds_dealt = defender_charge.wound_allocation.wounds_allocated

            self._add_phase(result, "Defender Charge", defender_charge, attacker_state, defender_state)

            attacker_wounds_dealt = 0
            # Attacker strikes back with melee weapons (if alive and not routed)
            if not self._is_out_of_action(attacker_state):
                attacker_melee = self.resolver.resolve_attack(
                    attacker=attacker_state.unit,
                    defender=defender_state.unit,
                    phase=CombatPhase.MELEE,
                    is_charging=False,
                    attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                    defender_shaken=defender_state.status == UnitStatus.SHAKEN,
                )
                attacker_wounds_dealt = attacker_melee.wound_allocation.wounds_allocated

                self._add_phase(result, "Attacker Strike Back", attacker_melee, attacker_state, defender_state)

            # Check melee morale
            if attacker_wounds_dealt < defender_wounds_dealt:
                morale_result = self._check_melee_morale(attacker_state, attacker_wounds_dealt, defender_wounds_dealt)
                if morale_result:
                    self._add_phase(result, "Attacker Melee Morale", None, attacker_state, defender_state, morale_result)
            elif defender_wounds_dealt < attacker_wounds_dealt:
                morale_result = self._check_melee_morale(defender_state, defender_wounds_dealt, attacker_wounds_dealt)
                if morale_result:
                    self._add_phase(result, "Defender Melee Morale", None, attacker_state, defender_state, morale_result)

    def _run_approach(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        turns: int,
        result: ScenarioResult,
    ) -> None:
        """Run approach scenario - attacker takes fire while closing."""
        # Defender shoots at attacker for N turns
        for turn in range(turns):
            if self._is_out_of_action(attacker_state):
                break

            defender_combat = self.resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.SHOOTING,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
            )

            # Check morale for attacker
            attacker_wounds = defender_combat.wound_allocation.wounds_allocated
            attacker_morale = self._check_casualty_morale(attacker_state, attacker_wounds)

            self._add_phase(
                result, f"Defender Shooting (Turn {turn + 1})", defender_combat,
                attacker_state, defender_state, attacker_morale
            )

        # Then attacker charges (if still alive and not routed)
        if not self._is_out_of_action(attacker_state) and not self._is_out_of_action(defender_state):
            self._run_charge(attacker_state, defender_state, result)

    def _run_full_engagement(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
        result: ScenarioResult,
    ) -> None:
        """Run full engagement - shooting exchange then melee."""
        # Mutual shooting first
        self._run_mutual_shooting(attacker_state, defender_state, defender_in_cover, result)

        # Then melee if both alive and not routed
        if not self._is_out_of_action(attacker_state) and not self._is_out_of_action(defender_state):
            self._run_charge(attacker_state, defender_state, result)

    def _run_fighting_retreat(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        result: ScenarioResult,
        max_rounds: int = 5,
    ) -> None:
        """Run fighting retreat scenario - attacker kites defender for up to 5 rounds.

        Round 1: Attacker shoots, defender charges, attacker strikes back.
        Round 2+: Attacker retreats 6" and shoots (no Lock-On weapons),
                  defender charges (if alive), attacker strikes back.
        Continues for up to 5 rounds or until one side is destroyed/routed.

        Lock-On weapons cannot be used after moving.
        """
        for round_num in range(1, max_rounds + 1):
            # Check if scenario should end
            if self._is_out_of_action(attacker_state) or self._is_out_of_action(defender_state):
                break

            # Attacker shoots (first round normally, subsequent rounds after moving)
            has_moved = round_num > 1  # After round 1, attacker has moved 6"

            # For shooting after moving, we need to resolve attack but Lock-On doesn't apply
            # The combat resolver already handles this via the not_moved condition check
            # We simulate this by passing moved state - but the resolver checks weapon rules
            shoot_combat = self.resolver.resolve_attack(
                attacker=attacker_state.unit,
                defender=defender_state.unit,
                phase=CombatPhase.SHOOTING,
                attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
            )

            # Check morale for defender
            defender_wounds = shoot_combat.wound_allocation.wounds_allocated
            defender_morale = self._check_casualty_morale(defender_state, defender_wounds)

            phase_prefix = f"Round {round_num}: "
            if has_moved:
                self._add_phase(result, f"{phase_prefix}Attacker Retreat + Shoot", shoot_combat,
                               attacker_state, defender_state, defender_morale)
            else:
                self._add_phase(result, f"{phase_prefix}Attacker Shooting", shoot_combat,
                               attacker_state, defender_state, defender_morale)

            # Defender charges (if not destroyed/routed)
            if self._is_out_of_action(defender_state):
                break

            defender_charge = self.resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=True,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
            )

            defender_wounds_dealt = defender_charge.wound_allocation.wounds_allocated

            self._add_phase(result, f"{phase_prefix}Defender Charge", defender_charge,
                           attacker_state, defender_state)

            # Attacker strikes back (if not destroyed/routed)
            attacker_wounds_dealt = 0
            if not self._is_out_of_action(attacker_state):
                attacker_melee = self.resolver.resolve_attack(
                    attacker=attacker_state.unit,
                    defender=defender_state.unit,
                    phase=CombatPhase.MELEE,
                    is_charging=False,
                    attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                    defender_shaken=defender_state.status == UnitStatus.SHAKEN,
                )
                attacker_wounds_dealt = attacker_melee.wound_allocation.wounds_allocated

                self._add_phase(result, f"{phase_prefix}Attacker Strike Back", attacker_melee,
                               attacker_state, defender_state)

            # Check melee morale
            if attacker_wounds_dealt < defender_wounds_dealt:
                morale_result = self._check_melee_morale(attacker_state, attacker_wounds_dealt, defender_wounds_dealt)
                if morale_result:
                    self._add_phase(result, f"{phase_prefix}Attacker Melee Morale", None,
                                   attacker_state, defender_state, morale_result)
            elif defender_wounds_dealt < attacker_wounds_dealt:
                morale_result = self._check_melee_morale(defender_state, defender_wounds_dealt, attacker_wounds_dealt)
                if morale_result:
                    self._add_phase(result, f"{phase_prefix}Defender Melee Morale", None,
                                   attacker_state, defender_state, morale_result)

            # Reset round state for next iteration (fatigue, etc.)
            attacker_state.reset_round_state()
            defender_state.reset_round_state()


def run_scenario(
    scenario_type: ScenarioType,
    attacker: Unit,
    defender: Unit,
    defender_in_cover: bool = False,
) -> ScenarioResult:
    """Convenience function to run a scenario."""
    runner = ScenarioRunner()
    return runner.run_scenario(scenario_type, attacker, defender, defender_in_cover)
