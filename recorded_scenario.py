"""Recorded scenario runner that captures events for visualization."""

from dataclasses import dataclass

from src.engine.combat import CombatPhase
from src.engine.combat_events import CombatEventLog, CombatEventRecorder
from src.engine.combat_state import UnitCombatState, UnitStatus
from src.engine.recorded_combat import (
    RecordedCombatResolver,
    RecordedMoraleSystem,
)
from src.engine.scenario import ScenarioType, ScenarioResult
from src.models.unit import Unit


@dataclass
class RecordedScenarioResult:
    """Result of a recorded scenario run."""

    scenario_type: ScenarioType
    attacker_name: str
    defender_name: str
    attacker_models_start: int
    defender_models_start: int
    attacker_models_end: int
    defender_models_end: int
    winner: str  # "attacker", "defender", or "draw"
    victory_condition: str


class RecordedScenarioRunner:
    """Runs combat scenarios with full event recording for visualization."""

    def __init__(self):
        self._recorder: CombatEventRecorder | None = None
        self._resolver: RecordedCombatResolver | None = None
        self._morale: RecordedMoraleSystem | None = None

    def run_scenario(
        self,
        scenario_type: ScenarioType,
        attacker: Unit,
        defender: Unit,
        defender_in_cover: bool = False,
    ) -> tuple[RecordedScenarioResult, CombatEventLog]:
        """Run a scenario with event recording.

        Args:
            scenario_type: The type of scenario to run
            attacker: The attacking unit
            defender: The defending unit
            defender_in_cover: Whether defender starts in cover

        Returns:
            Tuple of (RecordedScenarioResult, CombatEventLog)
        """
        # Create fresh copies for this simulation
        attacker_copy = attacker.copy_fresh()
        defender_copy = defender.copy_fresh()

        # Create combat state trackers
        attacker_state = UnitCombatState(unit=attacker_copy)
        defender_state = UnitCombatState(unit=defender_copy)

        # Create recorder and systems
        self._recorder = CombatEventRecorder(attacker_copy.name, defender_copy.name)
        self._resolver = RecordedCombatResolver(self._recorder)
        self._morale = RecordedMoraleSystem(self._recorder)

        # Record battle start
        self._recorder.record_battle_start(
            attacker_copy.alive_count,
            defender_copy.alive_count,
            attacker_total_wounds=sum(m.tough for m in attacker_copy.alive_models),
            defender_total_wounds=sum(m.tough for m in defender_copy.alive_models),
        )

        # Run the appropriate scenario
        match scenario_type:
            case ScenarioType.SHOOTING_ONLY:
                self._run_shooting_only(attacker_state, defender_state, defender_in_cover)
            case ScenarioType.MUTUAL_SHOOTING:
                self._run_mutual_shooting(attacker_state, defender_state, defender_in_cover)
            case ScenarioType.CHARGE:
                self._run_charge(attacker_state, defender_state)
            case ScenarioType.RECEIVE_CHARGE:
                self._run_receive_charge(attacker_state, defender_state)
            case ScenarioType.SHOOT_THEN_CHARGE:
                self._run_shoot_then_charge(attacker_state, defender_state, defender_in_cover)
            case ScenarioType.APPROACH_1_TURN:
                self._run_approach(attacker_state, defender_state, 1)
            case ScenarioType.APPROACH_2_TURNS:
                self._run_approach(attacker_state, defender_state, 2)
            case ScenarioType.FULL_ENGAGEMENT:
                self._run_full_engagement(attacker_state, defender_state, defender_in_cover)
            case ScenarioType.FIGHTING_RETREAT:
                self._run_fighting_retreat(attacker_state, defender_state)

        # Determine winner
        winner, victory_condition = self._determine_winner(attacker_state, defender_state)

        # Record battle end
        self._recorder.record_battle_end(
            attacker_copy.alive_count,
            defender_copy.alive_count,
            winner,
            victory_condition,
        )

        result = RecordedScenarioResult(
            scenario_type=scenario_type,
            attacker_name=attacker.name,
            defender_name=defender.name,
            attacker_models_start=attacker.model_count,
            defender_models_start=defender.model_count,
            attacker_models_end=attacker_copy.alive_count,
            defender_models_end=defender_copy.alive_count,
            winner=winner,
            victory_condition=victory_condition,
        )

        return result, self._recorder.get_log()

    def _is_out_of_action(self, state: UnitCombatState) -> bool:
        """Check if unit is destroyed or routed."""
        return state.unit.alive_count == 0 or state.status == UnitStatus.ROUTED

    def _check_casualty_morale(
        self,
        unit_state: UnitCombatState,
        wounds_taken: int,
    ) -> None:
        """Check and apply casualty morale if needed."""
        self._morale.check_casualty_morale(unit_state, wounds_taken)

    def _determine_winner(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
    ) -> tuple[str, str]:
        """Determine the winner of the scenario."""
        attacker_out = attacker_state.unit.alive_count == 0 or attacker_state.status == UnitStatus.ROUTED
        defender_out = defender_state.unit.alive_count == 0 or defender_state.status == UnitStatus.ROUTED

        if attacker_out and defender_out:
            return "draw", "mutual_destruction"
        elif defender_out:
            if defender_state.status == UnitStatus.ROUTED:
                return "attacker", "defender_routed"
            return "attacker", "attacker_destroyed_enemy"
        elif attacker_out:
            if attacker_state.status == UnitStatus.ROUTED:
                return "defender", "attacker_routed"
            return "defender", "defender_destroyed_enemy"
        else:
            # Compare remaining percentage
            attacker_start = len([m for m in attacker_state.unit.models])
            defender_start = len([m for m in defender_state.unit.models])
            attacker_pct = attacker_state.unit.alive_count / attacker_start if attacker_start > 0 else 0
            defender_pct = defender_state.unit.alive_count / defender_start if defender_start > 0 else 0

            if attacker_pct > defender_pct:
                return "attacker", "attacker_more_remaining"
            elif defender_pct > attacker_pct:
                return "defender", "defender_more_remaining"
            else:
                return "draw", "equal_remaining"

    def _run_shooting_only(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
    ) -> None:
        """Run shooting only scenario - attacker shoots defender."""
        self._recorder.record_round_start(1)
        self._recorder.record_activation_start(
            attacker_state.unit.name, "shooting", is_fatigued=False
        )

        combat = self._resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.SHOOTING,
            in_cover=defender_in_cover,
        )

        # Check morale for defender
        wounds_taken = combat.wound_allocation.wounds_allocated
        self._check_casualty_morale(defender_state, wounds_taken)

        self._recorder.record_activation_end(attacker_state.unit.name)
        self._recorder.record_round_end(wounds_taken, 0)

    def _run_mutual_shooting(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
    ) -> None:
        """Run mutual shooting scenario - both units shoot."""
        self._recorder.record_round_start(1)

        # Attacker shoots
        self._recorder.record_activation_start(
            attacker_state.unit.name, "shooting", is_fatigued=False
        )
        attacker_combat = self._resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.SHOOTING,
            in_cover=defender_in_cover,
        )
        defender_wounds = attacker_combat.wound_allocation.wounds_allocated
        self._check_casualty_morale(defender_state, defender_wounds)
        self._recorder.record_activation_end(attacker_state.unit.name)

        # Defender shoots (if not destroyed/routed)
        attacker_wounds = 0
        if not self._is_out_of_action(defender_state):
            self._recorder.record_activation_start(
                defender_state.unit.name, "shooting", is_fatigued=False
            )
            defender_combat = self._resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.SHOOTING,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
            )
            attacker_wounds = defender_combat.wound_allocation.wounds_allocated
            self._check_casualty_morale(attacker_state, attacker_wounds)
            self._recorder.record_activation_end(defender_state.unit.name)

        self._recorder.record_round_end(defender_wounds, attacker_wounds)

    def _run_charge(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
    ) -> None:
        """Run charge scenario - attacker charges and strikes first."""
        self._recorder.record_round_start(1)
        self._recorder.record_activation_start(
            attacker_state.unit.name, "charge", is_fatigued=False
        )

        # Attacker strikes first (charging)
        attacker_combat = self._resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.MELEE,
            is_charging=True,
            attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
            defender_shaken=defender_state.status == UnitStatus.SHAKEN,
        )
        attacker_wounds_dealt = attacker_combat.wound_allocation.wounds_allocated

        # Defender strikes back (if alive and not routed)
        defender_wounds_dealt = 0
        if not self._is_out_of_action(defender_state):
            self._recorder.record_strike_back_start(
                defender_state.unit.name,
                is_fatigued=False,
            )
            defender_combat = self._resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=False,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
            )
            defender_wounds_dealt = defender_combat.wound_allocation.wounds_allocated
            self._recorder.record_strike_back_end(
                defender_state.unit.name,
                defender_wounds_dealt,
                defender_combat.wound_allocation.models_killed,
            )

        self._recorder.record_activation_end(attacker_state.unit.name)

        # Check melee morale
        self._resolve_melee_morale(
            attacker_state, defender_state,
            attacker_wounds_dealt, defender_wounds_dealt
        )

        self._recorder.record_round_end(attacker_wounds_dealt, defender_wounds_dealt)

    def _run_receive_charge(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
    ) -> None:
        """Run receive charge scenario - defender charges attacker."""
        self._recorder.record_round_start(1)
        self._recorder.record_activation_start(
            defender_state.unit.name, "charge", is_fatigued=False
        )

        # Defender strikes first (charging)
        defender_combat = self._resolver.resolve_attack(
            attacker=defender_state.unit,
            defender=attacker_state.unit,
            phase=CombatPhase.MELEE,
            is_charging=True,
            attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
            defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
        )
        defender_wounds_dealt = defender_combat.wound_allocation.wounds_allocated

        # Attacker strikes back (if alive and not routed)
        attacker_wounds_dealt = 0
        if not self._is_out_of_action(attacker_state):
            self._recorder.record_strike_back_start(
                attacker_state.unit.name,
                is_fatigued=False,
            )
            attacker_combat = self._resolver.resolve_attack(
                attacker=attacker_state.unit,
                defender=defender_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=False,
                attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                defender_shaken=defender_state.status == UnitStatus.SHAKEN,
            )
            attacker_wounds_dealt = attacker_combat.wound_allocation.wounds_allocated
            self._recorder.record_strike_back_end(
                attacker_state.unit.name,
                attacker_wounds_dealt,
                attacker_combat.wound_allocation.models_killed,
            )

        self._recorder.record_activation_end(defender_state.unit.name)

        # Check melee morale
        self._resolve_melee_morale(
            attacker_state, defender_state,
            attacker_wounds_dealt, defender_wounds_dealt
        )

        self._recorder.record_round_end(attacker_wounds_dealt, defender_wounds_dealt)

    def _run_shoot_then_charge(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
    ) -> None:
        """Run shoot then charge scenario - attacker shoots, defender charges back."""
        # Round 1: Attacker shoots
        self._recorder.record_round_start(1)
        self._recorder.record_activation_start(
            attacker_state.unit.name, "shooting", is_fatigued=False
        )
        shoot_combat = self._resolver.resolve_attack(
            attacker=attacker_state.unit,
            defender=defender_state.unit,
            phase=CombatPhase.SHOOTING,
            in_cover=defender_in_cover,
        )
        defender_wounds = shoot_combat.wound_allocation.wounds_allocated
        self._check_casualty_morale(defender_state, defender_wounds)
        self._recorder.record_activation_end(attacker_state.unit.name)
        self._recorder.record_round_end(defender_wounds, 0)

        # Round 2: Defender charges (if not destroyed/routed)
        if not self._is_out_of_action(defender_state):
            self._recorder.record_round_start(2)
            self._recorder.record_activation_start(
                defender_state.unit.name, "charge", is_fatigued=False
            )

            # Defender charges (strikes first with charge bonuses)
            defender_charge = self._resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=True,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
            )
            defender_wounds_dealt = defender_charge.wound_allocation.wounds_allocated

            # Attacker strikes back (if alive and not routed)
            attacker_wounds_dealt = 0
            if not self._is_out_of_action(attacker_state):
                self._recorder.record_strike_back_start(
                    attacker_state.unit.name,
                    is_fatigued=False,
                )
                attacker_melee = self._resolver.resolve_attack(
                    attacker=attacker_state.unit,
                    defender=defender_state.unit,
                    phase=CombatPhase.MELEE,
                    is_charging=False,
                    attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                    defender_shaken=defender_state.status == UnitStatus.SHAKEN,
                )
                attacker_wounds_dealt = attacker_melee.wound_allocation.wounds_allocated
                self._recorder.record_strike_back_end(
                    attacker_state.unit.name,
                    attacker_wounds_dealt,
                    attacker_melee.wound_allocation.models_killed,
                )

            self._recorder.record_activation_end(defender_state.unit.name)

            # Check melee morale
            self._resolve_melee_morale(
                attacker_state, defender_state,
                attacker_wounds_dealt, defender_wounds_dealt
            )

            self._recorder.record_round_end(attacker_wounds_dealt, defender_wounds_dealt)

    def _run_approach(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        turns: int,
    ) -> None:
        """Run approach scenario - attacker takes fire while closing."""
        # Defender shoots at attacker for N turns
        for turn in range(turns):
            if self._is_out_of_action(attacker_state):
                break

            self._recorder.record_round_start(turn + 1)
            self._recorder.record_activation_start(
                defender_state.unit.name, "shooting", is_fatigued=False
            )

            defender_combat = self._resolver.resolve_attack(
                attacker=defender_state.unit,
                defender=attacker_state.unit,
                phase=CombatPhase.SHOOTING,
                attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
            )

            attacker_wounds = defender_combat.wound_allocation.wounds_allocated
            self._check_casualty_morale(attacker_state, attacker_wounds)

            self._recorder.record_activation_end(defender_state.unit.name)
            self._recorder.record_round_end(0, attacker_wounds)

        # Then attacker charges (if still alive and not routed)
        if not self._is_out_of_action(attacker_state) and not self._is_out_of_action(defender_state):
            self._recorder.record_round_start(turns + 1)
            self._recorder.record_activation_start(
                attacker_state.unit.name, "charge", is_fatigued=False
            )

            # Attacker charges
            attacker_combat = self._resolver.resolve_attack(
                attacker=attacker_state.unit,
                defender=defender_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=True,
                attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                defender_shaken=defender_state.status == UnitStatus.SHAKEN,
            )
            attacker_wounds_dealt = attacker_combat.wound_allocation.wounds_allocated

            # Defender strikes back
            defender_wounds_dealt = 0
            if not self._is_out_of_action(defender_state):
                self._recorder.record_strike_back_start(
                    defender_state.unit.name,
                    is_fatigued=False,
                )
                defender_melee = self._resolver.resolve_attack(
                    attacker=defender_state.unit,
                    defender=attacker_state.unit,
                    phase=CombatPhase.MELEE,
                    is_charging=False,
                    attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                    defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
                )
                defender_wounds_dealt = defender_melee.wound_allocation.wounds_allocated
                self._recorder.record_strike_back_end(
                    defender_state.unit.name,
                    defender_wounds_dealt,
                    defender_melee.wound_allocation.models_killed,
                )

            self._recorder.record_activation_end(attacker_state.unit.name)

            # Check melee morale
            self._resolve_melee_morale(
                attacker_state, defender_state,
                attacker_wounds_dealt, defender_wounds_dealt
            )

            self._recorder.record_round_end(attacker_wounds_dealt, defender_wounds_dealt)

    def _run_full_engagement(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        defender_in_cover: bool,
    ) -> None:
        """Run full engagement - shooting exchange then melee."""
        # Mutual shooting first
        self._run_mutual_shooting(attacker_state, defender_state, defender_in_cover)

        # Then melee if both alive and not routed
        if not self._is_out_of_action(attacker_state) and not self._is_out_of_action(defender_state):
            self._recorder.record_round_start(2)
            self._recorder.record_activation_start(
                attacker_state.unit.name, "charge", is_fatigued=False
            )

            # Attacker charges
            attacker_combat = self._resolver.resolve_attack(
                attacker=attacker_state.unit,
                defender=defender_state.unit,
                phase=CombatPhase.MELEE,
                is_charging=True,
                attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                defender_shaken=defender_state.status == UnitStatus.SHAKEN,
            )
            attacker_wounds_dealt = attacker_combat.wound_allocation.wounds_allocated

            # Defender strikes back
            defender_wounds_dealt = 0
            if not self._is_out_of_action(defender_state):
                self._recorder.record_strike_back_start(
                    defender_state.unit.name,
                    is_fatigued=False,
                )
                defender_melee = self._resolver.resolve_attack(
                    attacker=defender_state.unit,
                    defender=attacker_state.unit,
                    phase=CombatPhase.MELEE,
                    is_charging=False,
                    attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                    defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
                )
                defender_wounds_dealt = defender_melee.wound_allocation.wounds_allocated
                self._recorder.record_strike_back_end(
                    defender_state.unit.name,
                    defender_wounds_dealt,
                    defender_melee.wound_allocation.models_killed,
                )

            self._recorder.record_activation_end(attacker_state.unit.name)

            # Check melee morale
            self._resolve_melee_morale(
                attacker_state, defender_state,
                attacker_wounds_dealt, defender_wounds_dealt
            )

            self._recorder.record_round_end(attacker_wounds_dealt, defender_wounds_dealt)

    def _run_fighting_retreat(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        max_rounds: int = 5,
    ) -> None:
        """Run fighting retreat scenario - attacker kites defender for up to 5 rounds."""
        for round_num in range(1, max_rounds + 1):
            if self._is_out_of_action(attacker_state) or self._is_out_of_action(defender_state):
                break

            self._recorder.record_round_start(round_num)

            # Attacker shoots
            self._recorder.record_activation_start(
                attacker_state.unit.name, "shooting", is_fatigued=False
            )
            shoot_combat = self._resolver.resolve_attack(
                attacker=attacker_state.unit,
                defender=defender_state.unit,
                phase=CombatPhase.SHOOTING,
                attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
            )
            defender_wounds = shoot_combat.wound_allocation.wounds_allocated
            self._check_casualty_morale(defender_state, defender_wounds)
            self._recorder.record_activation_end(attacker_state.unit.name)

            # Defender charges (if not destroyed/routed)
            defender_wounds_dealt = 0
            attacker_wounds_dealt = 0
            if not self._is_out_of_action(defender_state):
                self._recorder.record_activation_start(
                    defender_state.unit.name, "charge", is_fatigued=False
                )
                defender_charge = self._resolver.resolve_attack(
                    attacker=defender_state.unit,
                    defender=attacker_state.unit,
                    phase=CombatPhase.MELEE,
                    is_charging=True,
                    attacker_shaken=defender_state.status == UnitStatus.SHAKEN,
                    defender_shaken=attacker_state.status == UnitStatus.SHAKEN,
                )
                defender_wounds_dealt = defender_charge.wound_allocation.wounds_allocated

                # Attacker strikes back (if not destroyed/routed)
                if not self._is_out_of_action(attacker_state):
                    self._recorder.record_strike_back_start(
                        attacker_state.unit.name,
                        is_fatigued=False,
                    )
                    attacker_melee = self._resolver.resolve_attack(
                        attacker=attacker_state.unit,
                        defender=defender_state.unit,
                        phase=CombatPhase.MELEE,
                        is_charging=False,
                        attacker_shaken=attacker_state.status == UnitStatus.SHAKEN,
                        defender_shaken=defender_state.status == UnitStatus.SHAKEN,
                    )
                    attacker_wounds_dealt = attacker_melee.wound_allocation.wounds_allocated
                    self._recorder.record_strike_back_end(
                        attacker_state.unit.name,
                        attacker_wounds_dealt,
                        attacker_melee.wound_allocation.models_killed,
                    )

                self._recorder.record_activation_end(defender_state.unit.name)

                # Check melee morale
                self._resolve_melee_morale(
                    attacker_state, defender_state,
                    attacker_wounds_dealt, defender_wounds_dealt
                )

            self._recorder.record_round_end(
                defender_wounds + attacker_wounds_dealt,
                defender_wounds_dealt
            )

            # Reset round state for next iteration
            attacker_state.reset_round_state()
            defender_state.reset_round_state()

    def _resolve_melee_morale(
        self,
        attacker_state: UnitCombatState,
        defender_state: UnitCombatState,
        attacker_wounds_dealt: int,
        defender_wounds_dealt: int,
    ) -> None:
        """Resolve melee morale for the loser."""
        if attacker_wounds_dealt == defender_wounds_dealt:
            return

        if attacker_wounds_dealt > defender_wounds_dealt:
            # Defender lost
            if not self._is_out_of_action(defender_state):
                self._morale.check_melee_morale(
                    defender_state,
                    defender_wounds_dealt,
                    attacker_wounds_dealt,
                )
        else:
            # Attacker lost
            if not self._is_out_of_action(attacker_state):
                self._morale.check_melee_morale(
                    attacker_state,
                    attacker_wounds_dealt,
                    defender_wounds_dealt,
                )


def run_recorded_scenario(
    scenario_type: ScenarioType,
    attacker: Unit,
    defender: Unit,
    defender_in_cover: bool = False,
) -> tuple[RecordedScenarioResult, CombatEventLog]:
    """Convenience function to run a recorded scenario.

    Args:
        scenario_type: The type of scenario to run
        attacker: The attacking unit
        defender: The defending unit
        defender_in_cover: Whether defender starts in cover

    Returns:
        Tuple of (RecordedScenarioResult, CombatEventLog)
    """
    runner = RecordedScenarioRunner()
    return runner.run_scenario(scenario_type, attacker, defender, defender_in_cover)
