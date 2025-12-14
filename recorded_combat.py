"""Recorded combat runner that captures events for visualization."""

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from src.engine.combat import CombatPhase, CombatResult, AttackResult
from src.engine.combat_events import CombatEventLog, CombatEventRecorder
from src.engine.combat_state import CombatStateManager, UnitCombatState, UnitStatus
from src.engine.dice import DiceRoller, get_dice_roller
from src.engine.morale import MoraleSystem, MoraleTestResult
from src.engine.multi_round import (
    ActivationAction,
    ActivationResult,
    BattleWinner,
    MultiRoundResult,
    RoundResult,
    VictoryCondition,
)
from src.engine.wound_allocation import WoundAllocationManager, WoundAllocationResult
from src.models.special_rule import SpecialRule
from src.models.unit import Model, Unit
from src.models.weapon import Weapon


class RecordingDiceRoller(DiceRoller):
    """Dice roller that records all rolls for visualization."""

    def __init__(self, recorder: CombatEventRecorder):
        super().__init__()
        self._recorder = recorder
        self._pending_hit_rolls: list[tuple[NDArray[np.int_], int, int]] = []
        self._pending_defense_rolls: list[tuple[NDArray[np.int_], int, int, int]] = []

    def roll_quality_test(
        self,
        attacks: int,
        quality: int,
        modifier: int = 0,
    ) -> tuple[int, NDArray[np.int_]]:
        """Roll quality test and record."""
        hits, rolls = super().roll_quality_test(attacks, quality, modifier)
        target = quality - modifier
        self._recorder.record_hit_roll(rolls, target, hits, is_fatigued=False)
        return hits, rolls

    def roll_defense_test(
        self,
        hits: int,
        defense: int,
        ap: int = 0,
        modifier: int = 0,
        reroll_sixes: bool = False,
    ) -> tuple[int, NDArray[np.int_]]:
        """Roll defense test and record."""
        wounds, rolls = super().roll_defense_test(hits, defense, ap, modifier, reroll_sixes)
        target = defense + ap - modifier
        saves = hits - wounds
        self._recorder.record_defense_roll(rolls, defense, ap, saves, wounds, reroll_sixes)
        return wounds, rolls


class RecordingWoundAllocationManager(WoundAllocationManager):
    """Wound allocation manager that records events."""

    def __init__(self, recorder: CombatEventRecorder):
        super().__init__()
        self._recorder = recorder

    def allocate_wounds(
        self,
        unit: Unit,
        wounds: int,
        allow_regeneration: bool = True,
    ) -> WoundAllocationResult:
        """Allocate wounds and record events."""
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

        while remaining_wounds > 0:
            allocation_order = unit.get_wound_allocation_order()

            if not allocation_order:
                break

            target_model = allocation_order[0]
            wounds_to_apply = min(remaining_wounds, target_model.remaining_wounds)

            model_died = False
            wounds_on_this_model = 0
            wounds_before = target_model.wounds_taken

            for _ in range(wounds_to_apply):
                # Check Protected
                if self._model_has_protected(target_model, unit):
                    roll = get_dice_roller().roll_d6(1)[0]
                    protected_rule = target_model.get_rule("Protected") or unit.get_rule("Protected")
                    target = protected_rule.value if protected_rule and protected_rule.value else 6
                    success = roll >= target
                    self._recorder.record_protected_roll(target_model.name, roll, target, success)
                    if success:
                        wounds_regenerated += 1
                        remaining_wounds -= 1
                        continue

                # Check Regeneration
                if allow_regeneration and self._model_has_regeneration(target_model, unit):
                    roll = get_dice_roller().roll_d6(1)[0]
                    target = self._get_regeneration_target(target_model, unit)
                    success = roll >= target
                    self._recorder.record_regeneration_roll(target_model.name, roll, target, success)
                    if success:
                        wounds_regenerated += 1
                        remaining_wounds -= 1
                        continue

                wounds_before_this = target_model.wounds_taken
                model_died = target_model.apply_wound()
                wounds_on_this_model += 1
                remaining_wounds -= 1

                # Record wound allocation
                self._recorder.record_wound_allocated(
                    target_model.name,
                    1,
                    wounds_before_this,
                    target_model.wounds_taken,
                    target_model.tough,
                )

                if model_died:
                    models_killed += 1
                    self._recorder.record_model_killed(target_model.name)
                    break

            if wounds_on_this_model > 0:
                allocation_details.append(
                    (target_model.name, wounds_on_this_model, model_died)
                )

        overkill = remaining_wounds

        return WoundAllocationResult(
            wounds_allocated=wounds - overkill,
            models_killed=models_killed,
            wounds_regenerated=wounds_regenerated,
            overkill_wounds=overkill,
            allocation_details=allocation_details,
        )


class RecordedCombatResolver:
    """Combat resolver that records events for visualization."""

    def __init__(self, recorder: CombatEventRecorder):
        self._recorder = recorder
        self.dice = RecordingDiceRoller(recorder)
        self.wound_manager = RecordingWoundAllocationManager(recorder)

    def resolve_attack(
        self,
        attacker: Unit,
        defender: Unit,
        phase: CombatPhase,
        is_charging: bool = False,
        in_cover: bool = False,
        attacker_shaken: bool = False,
        defender_shaken: bool = False,
        attacker_fatigued: bool = False,
    ) -> CombatResult:
        """Resolve an attack with event recording."""
        self._recorder.record_attack_start(
            attacker.name,
            defender.name,
            phase.value,
            is_charging,
        )

        attack_results: list[AttackResult] = []
        total_hits = 0
        total_wounds = 0
        any_bane_wounds = False

        for model in attacker.alive_models:
            weapons = (
                model.melee_weapons if phase == CombatPhase.MELEE else model.ranged_weapons
            )

            for weapon in weapons:
                result = self._resolve_weapon_attack(
                    model=model,
                    weapon=weapon,
                    defender=defender,
                    attacker_unit=attacker,
                    is_charging=is_charging,
                    in_cover=in_cover,
                    attacker_shaken=attacker_shaken,
                    defender_shaken=defender_shaken,
                    attacker_fatigued=attacker_fatigued and phase == CombatPhase.MELEE,
                )
                attack_results.append(result)
                total_hits += result.hits_after_modifiers
                total_wounds += result.wounds_dealt
                if result.has_bane and result.wounds_dealt > 0:
                    any_bane_wounds = True

        # Allocate wounds
        wound_result = self.wound_manager.allocate_wounds(
            defender, total_wounds, allow_regeneration=not any_bane_wounds
        )

        self._recorder.record_attack_end(total_wounds, wound_result.models_killed)

        return CombatResult(
            attacker_name=attacker.name,
            defender_name=defender.name,
            phase=phase,
            is_charging=is_charging,
            attack_results=attack_results,
            total_hits=total_hits,
            total_wounds_before_saves=total_hits,
            total_wounds_after_saves=total_wounds,
            wound_allocation=wound_result,
            defender_models_killed=wound_result.models_killed,
            defender_models_remaining=defender.alive_count,
            attacker_models_remaining=attacker.alive_count,
        )

    def _resolve_weapon_attack(
        self,
        model: Model,
        weapon: Weapon,
        defender: Unit,
        attacker_unit: Unit,
        is_charging: bool,
        in_cover: bool,
        attacker_shaken: bool,
        defender_shaken: bool,
        attacker_fatigued: bool = False,
    ) -> AttackResult:
        """Resolve attacks from a single weapon with event recording."""
        special_effects: dict[str, int] = {}
        attacks = weapon.attacks

        self._recorder.record_weapon_attack_start(weapon.name, attacks, model.name)

        # Roll to hit
        if attacker_fatigued:
            hit_rolls = self.dice.roll_d6(attacks)
            hits = self.dice.count_sixes(hit_rolls)
            self._recorder.record_hit_roll(hit_rolls, 6, hits, is_fatigued=True)
            special_effects["fatigued"] = 1
        else:
            quality_mod = -1 if attacker_shaken else 0
            effective_quality = model.quality

            # Check for on_hit_roll weapon rules
            # Reliable: Sets quality to 2+
            reliable_rule = weapon.get_rule("Reliable")
            if reliable_rule:
                effective_quality = reliable_rule.value if reliable_rule.value else 2
                special_effects["reliable"] = 1
                self._recorder.record_rule_triggered(
                    "Reliable", None, f"Quality set to {effective_quality}+"
                )

            # Precise: +1 to hit rolls
            precise_rule = weapon.get_rule("Precise")
            if precise_rule:
                bonus = precise_rule.value if precise_rule.value else 1
                quality_mod += bonus
                special_effects["precise"] = 1
                self._recorder.record_rule_triggered(
                    "Precise", bonus, f"+{bonus} to hit rolls"
                )

            hits, hit_rolls = self.dice.roll_quality_test(attacks, effective_quality, quality_mod)

        # Apply hit modifiers
        hits_after_mods = hits

        # Furious
        if is_charging:
            furious = model.get_rule("Furious") or attacker_unit.get_rule("Furious")
            if furious:
                sixes = self.dice.count_sixes(hit_rolls)
                if sixes > 0:
                    old_hits = hits_after_mods
                    hits_after_mods += sixes
                    special_effects["furious_extra_hits"] = sixes
                    self._recorder.record_hit_modifier(
                        "Furious",
                        old_hits,
                        hits_after_mods,
                        f"+{sixes} extra hits from 6s when charging",
                    )
                    self._recorder.record_rule_triggered(
                        "Furious", None, f"Charging grants +{sixes} extra hits from 6s"
                    )

        # Blast
        blast_rule = weapon.get_rule("Blast")
        if blast_rule and blast_rule.value:
            multiplier = min(blast_rule.value, defender.model_count)
            old_hits = hits_after_mods
            hits_after_mods = hits_after_mods * multiplier
            special_effects["blast_multiplier"] = multiplier
            self._recorder.record_hit_modifier(
                f"Blast({blast_rule.value})",
                old_hits,
                hits_after_mods,
                f"Multiply hits by {multiplier} (capped at defender model count)",
            )
            self._recorder.record_rule_triggered(
                "Blast", blast_rule.value, f"Hits multiplied by {multiplier}"
            )

        # Calculate AP
        ap = weapon.ap
        if is_charging:
            lance = weapon.get_rule("Lance")
            if lance:
                bonus = lance.value if lance.value else 2
                ap += bonus
                self._recorder.record_rule_triggered(
                    "Lance", bonus, f"+{bonus} AP when charging"
                )

        # Roll defense
        defender_model = defender.alive_models[0] if defender.alive_models else None
        if not defender_model:
            self._recorder.record_weapon_attack_end(weapon.name, 0)
            return AttackResult(
                weapon_name=weapon.name,
                attacks_made=attacks,
                hit_rolls=hit_rolls,
                hits=hits,
                hits_after_modifiers=hits_after_mods,
                defense_rolls=np.array([], dtype=np.int_),
                wounds_dealt=0,
                special_effects=special_effects,
            )

        defense = defender_model.defense
        defense_mod = 0
        if in_cover:
            defense_mod += 1
        if defender_shaken:
            defense_mod -= 1

        has_poison = weapon.has_rule("Poison")
        if has_poison:
            self._recorder.record_rule_triggered(
                "Poison", None, "Defender rerolls successful defense rolls of 6"
            )

        wounds, defense_rolls = self.dice.roll_defense_test(
            hits_after_mods, defense, ap, defense_mod, reroll_sixes=has_poison
        )

        if has_poison:
            special_effects["poison_reroll"] = 1

        # Apply wound modifiers
        deadly_rule = weapon.get_rule("Deadly")
        if deadly_rule and deadly_rule.value and wounds > 0:
            old_wounds = wounds
            wounds = wounds * deadly_rule.value
            special_effects["deadly_multiplier"] = deadly_rule.value
            self._recorder.record_wound_modifier(
                f"Deadly({deadly_rule.value})",
                old_wounds,
                wounds,
                f"Multiply wounds by {deadly_rule.value}",
            )
            self._recorder.record_rule_triggered(
                "Deadly", deadly_rule.value, f"Each wound multiplied by {deadly_rule.value}"
            )

        # Rending tracking
        rending_rule = weapon.get_rule("Rending")
        if rending_rule:
            sixes = self.dice.count_sixes(hit_rolls)
            if sixes > 0:
                special_effects["rending_hits"] = sixes
                self._recorder.record_rule_triggered(
                    "Rending", 4, f"{sixes} hits with AP(4) from 6s"
                )

        # Bane
        has_bane = weapon.has_rule("Bane")
        if has_bane:
            special_effects["bane"] = 1
            self._recorder.record_rule_triggered(
                "Bane", None, "Wounds bypass Regeneration and force defense 6 rerolls"
            )

        self._recorder.record_weapon_attack_end(weapon.name, wounds)

        return AttackResult(
            weapon_name=weapon.name,
            attacks_made=attacks,
            hit_rolls=hit_rolls,
            hits=hits,
            hits_after_modifiers=hits_after_mods,
            defense_rolls=defense_rolls,
            wounds_dealt=wounds,
            special_effects=special_effects,
            has_bane=has_bane,
        )


class RecordedMoraleSystem(MoraleSystem):
    """Morale system that records events."""

    def __init__(self, recorder: CombatEventRecorder):
        super().__init__()
        self._recorder = recorder

    def check_casualty_morale(
        self,
        unit_state: UnitCombatState,
        wounds_just_taken: int,
    ) -> MoraleTestResult | None:
        """Check casualty morale and record the result."""
        result = super().check_casualty_morale(unit_state, wounds_just_taken)

        if result:
            roll = result.roll
            target = result.target
            outcome = "passed" if result.passed else f"failed -> {result.outcome.value}"
            reason = f"casualties (wounds taken: {wounds_just_taken}, now at half strength)"

            self._recorder.record_morale_test(
                unit_state.unit.name,
                roll,
                target,
                reason,
                outcome,
            )

            if not result.passed:
                if result.outcome.value == "shaken":
                    self._recorder.record_unit_shaken(unit_state.unit.name)
                elif result.outcome.value == "routed":
                    self._recorder.record_unit_routed(unit_state.unit.name)

        return result

    def check_melee_morale(
        self,
        loser_state: UnitCombatState,
        wounds_dealt: int,
        wounds_taken: int,
    ) -> MoraleTestResult | None:
        """Check melee morale and record the result."""
        result = super().check_melee_morale(loser_state, wounds_dealt, wounds_taken)

        if result:
            roll = result.roll
            target = result.target
            outcome = "passed" if result.passed else f"failed -> {result.outcome}"
            reason = f"melee morale (dealt {wounds_dealt}, took {wounds_taken})"

            self._recorder.record_morale_test(
                loser_state.unit.name,
                roll,
                target,
                reason,
                outcome,
            )

            if not result.passed:
                if result.outcome == "shaken":
                    self._recorder.record_unit_shaken(loser_state.unit.name)
                elif result.outcome == "routed":
                    self._recorder.record_unit_routed(loser_state.unit.name)

        return result


class RecordedMultiRoundCombat:
    """Multi-round combat that records all events for visualization."""

    def __init__(self, max_rounds: int = 10):
        self.max_rounds = max_rounds
        self._recorder: CombatEventRecorder | None = None
        self._resolver: RecordedCombatResolver | None = None
        self._morale: RecordedMoraleSystem | None = None

    def run_battle(
        self,
        attacker: Unit,
        defender: Unit,
        attacker_charges: bool = True,
    ) -> tuple[MultiRoundResult, CombatEventLog]:
        """Run a recorded battle.

        Returns:
            Tuple of (MultiRoundResult, CombatEventLog)
        """
        # Create recorder
        self._recorder = CombatEventRecorder(attacker.name, defender.name)
        self._resolver = RecordedCombatResolver(self._recorder)
        self._morale = RecordedMoraleSystem(self._recorder)

        state = CombatStateManager.create(attacker, defender, self.max_rounds)

        # Record battle start with total wounds for health bar visualization
        self._recorder.record_battle_start(
            state.attacker.alive_count,
            state.defender.alive_count,
            attacker_total_wounds=state.attacker.total_wounds,
            defender_total_wounds=state.defender.total_wounds,
        )

        result = MultiRoundResult(
            attacker_name=attacker.name,
            defender_name=defender.name,
            attacker_models_start=state.attacker.alive_count,
            defender_models_start=state.defender.alive_count,
        )

        first_state = state.attacker_state if attacker_charges else state.defender_state
        second_state = state.defender_state if attacker_charges else state.attacker_state

        while not state.is_battle_over:
            self._recorder.record_round_start(state.current_round)

            round_result = self._run_round(
                state,
                first_state,
                second_state,
                is_first_round=(state.current_round == 1),
            )
            result.rounds.append(round_result)

            result.total_wounds_by_attacker += round_result.attacker_wounds_dealt
            result.total_wounds_by_defender += round_result.defender_wounds_dealt

            self._recorder.record_round_end(
                round_result.attacker_wounds_dealt,
                round_result.defender_wounds_dealt,
            )

            if state.is_battle_over:
                break

            state.start_new_round()

        # Final state
        result.total_rounds = state.current_round
        result.attacker_models_end = state.attacker.alive_count
        result.defender_models_end = state.defender.alive_count

        result.total_kills_by_attacker = (
            result.defender_models_start - result.defender_models_end
        )
        result.total_kills_by_defender = (
            result.attacker_models_start - result.attacker_models_end
        )

        self._determine_winner(result, state)

        # Record battle end
        self._recorder.record_battle_end(
            result.attacker_models_end,
            result.defender_models_end,
            result.winner.value,
            result.victory_condition.value,
        )

        return result, self._recorder.get_log()

    def _run_round(
        self,
        state: CombatStateManager,
        first_state: UnitCombatState,
        second_state: UnitCombatState,
        is_first_round: bool,
    ) -> RoundResult:
        """Run a single round with event recording."""
        round_result = RoundResult(
            round_number=state.current_round,
            attacker_models_start=state.attacker.alive_count,
            defender_models_start=state.defender.alive_count,
        )

        # First activation
        if not first_state.is_out_of_action:
            activation1 = self._resolve_activation(
                state=state,
                active_state=first_state,
                opponent_state=second_state,
                is_charging=is_first_round,
                allow_strike_back=is_first_round,
            )
            round_result.activations.append(activation1)

            if first_state is state.attacker_state:
                round_result.attacker_wounds_dealt += activation1.wounds_dealt
                round_result.defender_wounds_dealt += activation1.strike_back_wounds
            else:
                round_result.defender_wounds_dealt += activation1.wounds_dealt
                round_result.attacker_wounds_dealt += activation1.strike_back_wounds

        if state.is_battle_over:
            round_result.round_ended_early = True
            round_result.attacker_models_end = state.attacker.alive_count
            round_result.defender_models_end = state.defender.alive_count
            return round_result

        # Second activation
        if not second_state.is_out_of_action:
            activation2 = self._resolve_activation(
                state=state,
                active_state=second_state,
                opponent_state=first_state,
                is_charging=False,
                allow_strike_back=False,
            )
            round_result.activations.append(activation2)

            if second_state is state.attacker_state:
                round_result.attacker_wounds_dealt += activation2.wounds_dealt
            else:
                round_result.defender_wounds_dealt += activation2.wounds_dealt

        round_result.attacker_models_end = state.attacker.alive_count
        round_result.defender_models_end = state.defender.alive_count

        # Melee morale
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
        """Resolve activation with event recording."""
        # Rally if shaken
        if active_state.status == UnitStatus.SHAKEN:
            self._recorder.record_activation_start(
                active_state.unit.name, "rally", is_fatigued=False
            )
            active_state.rally()
            self._recorder.record_rally(active_state.unit.name)
            self._recorder.record_activation_end(active_state.unit.name)
            return ActivationResult(
                unit_name=active_state.unit.name,
                action=ActivationAction.RALLY,
                rallied=True,
            )

        action = ActivationAction.CHARGE if is_charging else ActivationAction.MELEE
        is_fatigued = active_state.is_fatigued

        self._recorder.record_activation_start(
            active_state.unit.name,
            action.value,
            is_fatigued=is_fatigued,
        )

        # Attack
        combat_result = self._resolver.resolve_attack(
            attacker=active_state.unit,
            defender=opponent_state.unit,
            phase=CombatPhase.MELEE,
            is_charging=is_charging,
            attacker_shaken=False,
            defender_shaken=(opponent_state.status == UnitStatus.SHAKEN),
            attacker_fatigued=is_fatigued,
        )

        wounds_dealt = combat_result.wound_allocation.wounds_allocated
        models_killed = combat_result.wound_allocation.models_killed

        active_state.apply_fatigue()
        self._recorder.record_unit_fatigued(active_state.unit.name)

        activation_result = ActivationResult(
            unit_name=active_state.unit.name,
            action=action,
            combat_result=combat_result,
            wounds_dealt=wounds_dealt,
            models_killed=models_killed,
            was_fatigued=is_fatigued,
        )

        # Strike back
        if allow_strike_back and not opponent_state.is_out_of_action:
            is_striker_fatigued = (
                opponent_state.is_fatigued
                or opponent_state.status == UnitStatus.SHAKEN
            )

            self._recorder.record_strike_back_start(
                opponent_state.unit.name,
                is_striker_fatigued,
            )

            strike_back_result = self._resolver.resolve_attack(
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

            opponent_state.apply_fatigue()
            self._recorder.record_unit_fatigued(opponent_state.unit.name)

            self._recorder.record_strike_back_end(
                opponent_state.unit.name,
                sb_wounds,
                sb_kills,
            )

            activation_result.strike_back_wounds = sb_wounds
            activation_result.strike_back_kills = sb_kills
            activation_result.strike_back_result = strike_back_result

        self._recorder.record_activation_end(active_state.unit.name)
        return activation_result

    def _resolve_melee_morale(
        self,
        state: CombatStateManager,
        attacker_wounds: int,
        defender_wounds: int,
        round_result: RoundResult,
    ) -> None:
        """Resolve melee morale."""
        if attacker_wounds == defender_wounds:
            return

        if attacker_wounds > defender_wounds:
            loser_state = state.defender_state
            wounds_dealt = defender_wounds
            wounds_taken = attacker_wounds
        else:
            loser_state = state.attacker_state
            wounds_dealt = attacker_wounds
            wounds_taken = defender_wounds

        if not loser_state.is_out_of_action:
            morale_result = self._morale.check_melee_morale(
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
        """Determine the winner."""
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


def run_recorded_battle(
    attacker: Unit,
    defender: Unit,
    attacker_charges: bool = True,
    max_rounds: int = 10,
) -> tuple[MultiRoundResult, CombatEventLog]:
    """Convenience function to run a recorded battle.

    Args:
        attacker: The attacking unit
        defender: The defending unit
        attacker_charges: Whether attacker charges first
        max_rounds: Maximum rounds before forcing a result

    Returns:
        Tuple of (MultiRoundResult, CombatEventLog)
    """
    runner = RecordedMultiRoundCombat(max_rounds=max_rounds)
    return runner.run_battle(attacker, defender, attacker_charges)
