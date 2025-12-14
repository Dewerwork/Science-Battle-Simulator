"""Multi-round combat engine for extended battles with alternating activations."""

from dataclasses import dataclass, field
from enum import Enum

from src.engine.combat import CombatPhase, CombatResolver, CombatResult
from src.engine.combat_state import CombatStateManager, UnitCombatState, UnitStatus
from src.engine.morale import MoraleSystem, MoraleTestResult
from src.models.unit import Unit


class VictoryCondition(str, Enum):
    """How the battle was won."""

    ATTACKER_DESTROYED_ENEMY = "attacker_destroyed_enemy"
    DEFENDER_DESTROYED_ENEMY = "defender_destroyed_enemy"
    ATTACKER_ROUTED_ENEMY = "attacker_routed_enemy"
    DEFENDER_ROUTED_ENEMY = "defender_routed_enemy"
    ATTACKER_ROUTED = "attacker_routed"
    DEFENDER_ROUTED = "defender_routed"
    MAX_ROUNDS_ATTACKER_AHEAD = "max_rounds_attacker_ahead"
    MAX_ROUNDS_DEFENDER_AHEAD = "max_rounds_defender_ahead"
    MAX_ROUNDS_DRAW = "max_rounds_draw"
    MUTUAL_DESTRUCTION = "mutual_destruction"


class BattleWinner(str, Enum):
    """Who won the battle."""

    ATTACKER = "attacker"
    DEFENDER = "defender"
    DRAW = "draw"


class ActivationAction(str, Enum):
    """What action a unit takes during activation."""

    CHARGE = "charge"
    STRIKE_BACK = "strike_back"
    MELEE = "melee"
    RALLY = "rally"


@dataclass
class ActivationResult:
    """Result of a single unit activation."""

    unit_name: str
    action: ActivationAction
    combat_result: CombatResult | None = None
    wounds_dealt: int = 0
    models_killed: int = 0
    was_fatigued: bool = False
    rallied: bool = False
    casualty_morale: MoraleTestResult | None = None
    # Strike back results (only on charge)
    strike_back_wounds: int = 0
    strike_back_kills: int = 0
    strike_back_result: CombatResult | None = None  # Full combat result for detailed logging


@dataclass
class RoundResult:
    """Result of a single round of combat (two activations)."""

    round_number: int
    activations: list[ActivationResult] = field(default_factory=list)
    attacker_models_start: int = 0
    attacker_models_end: int = 0
    defender_models_start: int = 0
    defender_models_end: int = 0
    attacker_wounds_dealt: int = 0
    defender_wounds_dealt: int = 0
    melee_morale_result: MoraleTestResult | None = None
    melee_morale_unit: str | None = None
    round_ended_early: bool = False


@dataclass
class MultiRoundResult:
    """Result of a complete multi-round battle."""

    attacker_name: str
    defender_name: str
    rounds: list[RoundResult] = field(default_factory=list)

    # Final state
    total_rounds: int = 0
    attacker_models_start: int = 0
    attacker_models_end: int = 0
    defender_models_start: int = 0
    defender_models_end: int = 0

    # Outcome
    winner: BattleWinner = BattleWinner.DRAW
    victory_condition: VictoryCondition = VictoryCondition.MAX_ROUNDS_DRAW

    # Damage totals
    total_wounds_by_attacker: int = 0
    total_wounds_by_defender: int = 0
    total_kills_by_attacker: int = 0
    total_kills_by_defender: int = 0


class MultiRoundCombat:
    """Runs multi-round combat between two units with alternating activations.

    Models the OPR alternating activation system:
    - Each round, both players get one activation
    - On activation, a unit can: Attack (melee), or Rally (if shaken)
    - Shaken units MUST rally (optimal play)
    - Fatigued units can still attack but only hit on 6s
    - Fatigue resets at the start of each round
    """

    def __init__(
        self,
        combat_resolver: CombatResolver | None = None,
        morale_system: MoraleSystem | None = None,
        max_rounds: int = 10,
    ):
        """Initialize multi-round combat runner.

        Args:
            combat_resolver: Combat resolver to use
            morale_system: Morale system to use
            max_rounds: Maximum rounds before forcing a result
        """
        self.resolver = combat_resolver or CombatResolver()
        self.morale = morale_system or MoraleSystem()
        self.max_rounds = max_rounds

    def run_battle(
        self,
        attacker: Unit,
        defender: Unit,
        attacker_charges: bool = True,
    ) -> MultiRoundResult:
        """Run a complete multi-round battle with alternating activations.

        Args:
            attacker: The attacking unit
            defender: The defending unit
            attacker_charges: Whether attacker charges first (else defender charges)

        Returns:
            MultiRoundResult with complete battle details
        """
        state = CombatStateManager.create(attacker, defender, self.max_rounds)

        result = MultiRoundResult(
            attacker_name=attacker.name,
            defender_name=defender.name,
            attacker_models_start=state.attacker.model_count,
            defender_models_start=state.defender.model_count,
        )

        # Determine activation order (who goes first each round)
        # The charger activates first in round 1, then alternates
        first_state = state.attacker_state if attacker_charges else state.defender_state
        second_state = state.defender_state if attacker_charges else state.attacker_state

        # Run rounds until battle ends
        while not state.is_battle_over:
            round_result = self._run_round(
                state,
                first_state,
                second_state,
                is_first_round=(state.current_round == 1),
            )
            result.rounds.append(round_result)

            # Accumulate damage
            result.total_wounds_by_attacker += round_result.attacker_wounds_dealt
            result.total_wounds_by_defender += round_result.defender_wounds_dealt

            if state.is_battle_over:
                break

            # Prepare for next round - reset fatigue
            state.start_new_round()

        # Record final state
        result.total_rounds = state.current_round
        result.attacker_models_end = state.attacker.alive_count
        result.defender_models_end = state.defender.alive_count

        # Calculate total kills
        result.total_kills_by_attacker = (
            result.defender_models_start - result.defender_models_end
        )
        result.total_kills_by_defender = (
            result.attacker_models_start - result.attacker_models_end
        )

        # Determine winner
        self._determine_winner(result, state)

        return result

    def _run_round(
        self,
        state: CombatStateManager,
        first_state: UnitCombatState,
        second_state: UnitCombatState,
        is_first_round: bool,
    ) -> RoundResult:
        """Run a single round with two activations (one per player).

        Args:
            state: The combat state manager
            first_state: State of the unit activating first
            second_state: State of the unit activating second
            is_first_round: Whether this is the first round (charge)

        Returns:
            RoundResult with round details
        """
        round_result = RoundResult(
            round_number=state.current_round,
            attacker_models_start=state.attacker.alive_count,
            defender_models_start=state.defender.alive_count,
        )

        # === ACTIVATION 1: First player ===
        if not first_state.is_out_of_action:
            activation1 = self._resolve_activation(
                state=state,
                active_state=first_state,
                opponent_state=second_state,
                is_charging=is_first_round,
                allow_strike_back=is_first_round,  # Only on charge
            )
            round_result.activations.append(activation1)

            # Track wounds from the active unit's attack
            if first_state is state.attacker_state:
                round_result.attacker_wounds_dealt += activation1.wounds_dealt
                # Strike back wounds go to defender's tally
                round_result.defender_wounds_dealt += activation1.strike_back_wounds
            else:
                round_result.defender_wounds_dealt += activation1.wounds_dealt
                # Strike back wounds go to attacker's tally
                round_result.attacker_wounds_dealt += activation1.strike_back_wounds

        # Check if battle ended after first activation
        if state.is_battle_over:
            round_result.round_ended_early = True
            round_result.attacker_models_end = state.attacker.alive_count
            round_result.defender_models_end = state.defender.alive_count
            return round_result

        # === ACTIVATION 2: Second player ===
        if not second_state.is_out_of_action:
            activation2 = self._resolve_activation(
                state=state,
                active_state=second_state,
                opponent_state=first_state,
                is_charging=False,
                allow_strike_back=False,
            )
            round_result.activations.append(activation2)

            # Track wounds
            if second_state is state.attacker_state:
                round_result.attacker_wounds_dealt += activation2.wounds_dealt
            else:
                round_result.defender_wounds_dealt += activation2.wounds_dealt

        # Record final model counts
        round_result.attacker_models_end = state.attacker.alive_count
        round_result.defender_models_end = state.defender.alive_count

        # === END OF ROUND: Melee morale check ===
        # Compare total wounds dealt this round - loser tests
        if not state.is_battle_over:
            self._resolve_melee_morale(
                state,
                round_result.attacker_wounds_dealt,
                round_result.defender_wounds_dealt,
                round_result,
            )

        return round_result

    def _resolve_activation(
        self,
        state: CombatStateManager,
        active_state: UnitCombatState,
        opponent_state: UnitCombatState,
        is_charging: bool,
        allow_strike_back: bool,
    ) -> ActivationResult:
        """Resolve a single unit activation.

        The unit will take the optimal action:
        - If shaken: Rally (remove shaken status)
        - Otherwise: Attack (even if fatigued)

        Args:
            state: Combat state manager
            active_state: State of the activating unit
            opponent_state: State of the opponent unit
            is_charging: Whether this is a charge action
            allow_strike_back: Whether opponent can strike back

        Returns:
            ActivationResult with action details
        """
        # Shaken units must rally (optimal play - can't attack effectively anyway)
        if active_state.status == UnitStatus.SHAKEN:
            active_state.rally()
            return ActivationResult(
                unit_name=active_state.unit.name,
                action=ActivationAction.RALLY,
                rallied=True,
            )

        # Determine action type
        if is_charging:
            action = ActivationAction.CHARGE
        else:
            action = ActivationAction.MELEE

        # Attack (even if fatigued - it's optimal to try)
        is_fatigued = active_state.is_fatigued
        combat_result = self.resolver.resolve_attack(
            attacker=active_state.unit,
            defender=opponent_state.unit,
            phase=CombatPhase.MELEE,
            is_charging=is_charging,
            attacker_shaken=False,  # Already handled above
            defender_shaken=(opponent_state.status == UnitStatus.SHAKEN),
            attacker_fatigued=is_fatigued,
        )

        wounds_dealt = combat_result.wound_allocation.wounds_allocated
        models_killed = combat_result.wound_allocation.models_killed

        # Apply fatigue after attacking
        active_state.apply_fatigue()

        # NOTE: Per OPR rules, units in melee don't take casualty morale tests.
        # They only take morale tests at end of round based on wound comparison.
        # "Units in melee don't take morale tests from wounds at the end of an activation."

        activation_result = ActivationResult(
            unit_name=active_state.unit.name,
            action=action,
            combat_result=combat_result,
            wounds_dealt=wounds_dealt,
            models_killed=models_killed,
            was_fatigued=is_fatigued,
        )

        # Handle strike back on charge
        # Per OPR rules: "Shaken: Must stay idle, but may strike back counting as fatigued"
        if allow_strike_back and not opponent_state.is_out_of_action:
            # Determine if striking back is beneficial (simple heuristic)
            should_strike_back = self._should_strike_back(
                opponent_state, active_state
            )

            if should_strike_back:
                # Shaken units count as fatigued when striking back
                is_striker_fatigued = (
                    opponent_state.is_fatigued
                    or opponent_state.status == UnitStatus.SHAKEN
                )

                strike_back_result = self.resolver.resolve_attack(
                    attacker=opponent_state.unit,
                    defender=active_state.unit,
                    phase=CombatPhase.MELEE,
                    is_charging=False,
                    attacker_shaken=False,
                    defender_shaken=False,
                    attacker_fatigued=is_striker_fatigued,
                )

                sb_wounds = strike_back_result.wound_allocation.wounds_allocated
                sb_kills = strike_back_result.wound_allocation.models_killed

                # Opponent becomes fatigued from striking back
                opponent_state.apply_fatigue()

                # Store strike back info in the activation result
                activation_result.strike_back_wounds = sb_wounds
                activation_result.strike_back_kills = sb_kills
                activation_result.strike_back_result = strike_back_result  # Store full result for logging

                # NOTE: No casualty morale in melee - only end-of-round wound comparison

        return activation_result

    def _should_strike_back(
        self,
        striker_state: UnitCombatState,
        target_state: UnitCombatState,
    ) -> bool:
        """Decide whether a unit should strike back.

        Per OPR rules, striking back is optional. This method implements
        a decision heuristic based on expected value.

        Generally strike back unless:
        - Already fatigued AND would gain nothing (opponent nearly dead anyway)

        Args:
            striker_state: State of the unit that would strike back
            target_state: State of the unit that just attacked

        Returns:
            True if the unit should strike back
        """
        # If already fatigued, striking back has no additional fatigue cost
        # so always do it
        if striker_state.is_fatigued:
            return True

        # If shaken, we hit on 6s anyway (count as fatigued), but striking back
        # still contributes to the wound comparison for end-of-round morale
        # Usually worth it to try to win or tie the wound comparison
        if striker_state.status == UnitStatus.SHAKEN:
            return True

        # If not fatigued, striking back will fatigue us, but it's usually
        # worth it to deal damage and contribute to wound comparison
        # The main downside is being fatigued if we get charged again this round,
        # but in a 1v1 melee that won't happen
        return True

    def _resolve_melee_morale(
        self,
        state: CombatStateManager,
        attacker_wounds: int,
        defender_wounds: int,
        round_result: RoundResult,
    ) -> None:
        """Resolve end-of-round melee morale.

        Per OPR: Compare wounds dealt in the melee, loser takes morale test.

        Args:
            state: Combat state manager
            attacker_wounds: Total wounds dealt by attacker this round
            defender_wounds: Total wounds dealt by defender this round
            round_result: Round result to record morale outcome
        """
        if attacker_wounds == defender_wounds:
            return  # Draw - no test

        if attacker_wounds > defender_wounds:
            loser_state = state.defender_state
            wounds_dealt = defender_wounds
            wounds_taken = attacker_wounds
        else:
            loser_state = state.attacker_state
            wounds_dealt = attacker_wounds
            wounds_taken = defender_wounds

        if not loser_state.is_out_of_action:
            morale_result = self.morale.check_melee_morale(
                loser_state,
                wounds_dealt,
                wounds_taken,
            )

            if morale_result:
                round_result.melee_morale_result = morale_result
                round_result.melee_morale_unit = loser_state.unit.name

    def _determine_winner(
        self,
        result: MultiRoundResult,
        state: CombatStateManager,
    ) -> None:
        """Determine the winner and victory condition."""
        attacker_out = state.attacker_state.is_out_of_action
        defender_out = state.defender_state.is_out_of_action
        attacker_routed = state.attacker_state.status == UnitStatus.ROUTED
        defender_routed = state.defender_state.status == UnitStatus.ROUTED

        if attacker_out and defender_out:
            result.winner = BattleWinner.DRAW
            result.victory_condition = VictoryCondition.MUTUAL_DESTRUCTION
            return

        if attacker_out:
            result.winner = BattleWinner.DEFENDER
            if attacker_routed:
                result.victory_condition = VictoryCondition.ATTACKER_ROUTED
            else:
                result.victory_condition = VictoryCondition.DEFENDER_DESTROYED_ENEMY
            return

        if defender_out:
            result.winner = BattleWinner.ATTACKER
            if defender_routed:
                result.victory_condition = VictoryCondition.ATTACKER_ROUTED_ENEMY
            else:
                result.victory_condition = VictoryCondition.ATTACKER_DESTROYED_ENEMY
            return

        # Max rounds - compare remaining strength
        attacker_pct = result.attacker_models_end / result.attacker_models_start
        defender_pct = result.defender_models_end / result.defender_models_start

        if attacker_pct > defender_pct:
            result.winner = BattleWinner.ATTACKER
            result.victory_condition = VictoryCondition.MAX_ROUNDS_ATTACKER_AHEAD
        elif defender_pct > attacker_pct:
            result.winner = BattleWinner.DEFENDER
            result.victory_condition = VictoryCondition.MAX_ROUNDS_DEFENDER_AHEAD
        else:
            result.winner = BattleWinner.DRAW
            result.victory_condition = VictoryCondition.MAX_ROUNDS_DRAW


def run_multi_round_battle(
    attacker: Unit,
    defender: Unit,
    attacker_charges: bool = True,
    max_rounds: int = 10,
) -> MultiRoundResult:
    """Convenience function to run a multi-round battle.

    Args:
        attacker: The attacking unit
        defender: The defending unit
        attacker_charges: Whether attacker charges first
        max_rounds: Maximum rounds before forcing a result

    Returns:
        MultiRoundResult with complete battle details
    """
    runner = MultiRoundCombat(max_rounds=max_rounds)
    return runner.run_battle(attacker, defender, attacker_charges)
