"""Core combat resolution engine."""

from dataclasses import dataclass, field
from enum import Enum

import numpy as np
from numpy.typing import NDArray

from src.engine.dice import DiceRoller, get_dice_roller
from src.engine.wound_allocation import WoundAllocationManager, WoundAllocationResult
from src.models.special_rule import RuleEffect, RuleTrigger, SpecialRule
from src.models.unit import Model, Unit
from src.models.weapon import Weapon


class CombatPhase(str, Enum):
    """Phase of combat."""

    SHOOTING = "shooting"
    MELEE = "melee"


@dataclass
class AttackResult:
    """Result of a single attack sequence (one weapon's attacks)."""

    weapon_name: str
    attacks_made: int
    hit_rolls: NDArray[np.int_]
    hits: int
    hits_after_modifiers: int  # After Blast, etc.
    defense_rolls: NDArray[np.int_]
    wounds_dealt: int
    special_effects: dict[str, int] = field(default_factory=dict)  # e.g., {"rending_hits": 2}
    has_bane: bool = False  # Whether this weapon has Bane (bypasses regeneration)


@dataclass
class CombatResult:
    """Result of a complete combat exchange."""

    attacker_name: str
    defender_name: str
    phase: CombatPhase
    is_charging: bool
    attack_results: list[AttackResult]
    total_hits: int
    total_wounds_before_saves: int
    total_wounds_after_saves: int
    wound_allocation: WoundAllocationResult
    defender_models_killed: int
    defender_models_remaining: int
    attacker_models_remaining: int


class CombatResolver:
    """Resolves combat between units following OPR rules."""

    def __init__(self, dice_roller: DiceRoller | None = None):
        """Initialize combat resolver.

        Args:
            dice_roller: Optional dice roller (uses global default if None)
        """
        self.dice = dice_roller or get_dice_roller()
        self.wound_manager = WoundAllocationManager()

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
        """Resolve an attack from one unit to another.

        Args:
            attacker: The attacking unit
            defender: The defending unit
            phase: Combat phase (shooting or melee)
            is_charging: Whether attacker is charging (for Lance, Furious, etc.)
            in_cover: Whether defender is in cover (+1 to defense)
            attacker_shaken: Whether attacker is shaken (-1 to quality)
            defender_shaken: Whether defender is shaken (-1 to defense)
            attacker_fatigued: Whether attacker is fatigued (only hits on 6s in melee)

        Returns:
            CombatResult with full details of the attack
        """
        attack_results: list[AttackResult] = []
        total_hits = 0
        total_wounds = 0

        # Get all weapons for this phase
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
                    phase=phase,
                    is_charging=is_charging,
                    in_cover=in_cover,
                    attacker_shaken=attacker_shaken,
                    defender_shaken=defender_shaken,
                    attacker_fatigued=attacker_fatigued and phase == CombatPhase.MELEE,
                )
                attack_results.append(result)
                total_hits += result.hits_after_modifiers
                total_wounds += result.wounds_dealt
                # Track if any Bane weapon dealt wounds
                if result.has_bane and result.wounds_dealt > 0:
                    any_bane_wounds = True

        # Allocate wounds to defender
        # If any Bane weapon dealt wounds, regeneration is bypassed
        wound_result = self.wound_manager.allocate_wounds(
            defender, total_wounds, allow_regeneration=not any_bane_wounds
        )

        return CombatResult(
            attacker_name=attacker.name,
            defender_name=defender.name,
            phase=phase,
            is_charging=is_charging,
            attack_results=attack_results,
            total_hits=total_hits,
            total_wounds_before_saves=total_hits,  # Before defense rolls
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
        phase: CombatPhase,
        is_charging: bool,
        in_cover: bool,
        attacker_shaken: bool,
        defender_shaken: bool,
        attacker_fatigued: bool = False,
    ) -> AttackResult:
        """Resolve attacks from a single weapon.

        Args:
            model: The model making the attack
            weapon: The weapon being used
            defender: The target unit
            attacker_unit: The attacker's unit (for unit-wide rules)
            phase: Combat phase (shooting or melee)
            is_charging: Whether charging
            in_cover: Whether defender in cover
            attacker_shaken: Attacker shaken status
            defender_shaken: Defender shaken status
            attacker_fatigued: Whether attacker is fatigued (only hits on 6s)

        Returns:
            AttackResult for this weapon
        """
        special_effects: dict[str, int] = {}

        # Calculate attacks
        attacks = self._calculate_attacks(
            model, weapon, attacker_unit, is_charging
        )

        # Roll to hit
        # Fatigued units only hit on unmodified 6s (per OPR rules)
        if attacker_fatigued:
            hit_rolls = self.dice.roll_d6(attacks)
            hits = self.dice.count_sixes(hit_rolls)
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

            # Precise: +1 to hit rolls
            precise_rule = weapon.get_rule("Precise")
            if precise_rule:
                quality_mod += precise_rule.value if precise_rule.value else 1
                special_effects["precise"] = 1

            hits, hit_rolls = self.dice.roll_quality_test(
                attacks, effective_quality, quality_mod
            )

        # Apply hit modifiers (Blast, Furious, etc.)
        hits_after_mods = hits
        hits_after_mods, effects = self._apply_hit_modifiers(
            hits, hit_rolls, weapon, model, attacker_unit, defender, is_charging
        )
        special_effects.update(effects)

        # Note: Poison forces defender to reroll defense results of 6.
        # This is handled in the defense roll logic below, not here.

        # Calculate effective AP
        effective_ap = self._calculate_ap(weapon, is_charging, model, attacker_unit)

        # Roll defense for each hit
        # Get defender's defense (use first alive model as representative)
        defender_model = defender.alive_models[0] if defender.alive_models else None
        if not defender_model:
            # Defender destroyed
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
            defense_mod += 1  # Cover gives +1 to defense
        if defender_shaken:
            defense_mod -= 1  # Shaken gives -1

        # Check for Poison - forces defender to reroll defense results of 6
        has_poison = weapon.has_rule("Poison")

        wounds, defense_rolls = self.dice.roll_defense_test(
            hits_after_mods, defense, effective_ap, defense_mod,
            reroll_sixes=has_poison
        )

        if has_poison:
            special_effects["poison_reroll"] = 1

        # Apply wound modifiers (Deadly, etc.)
        wounds = self._apply_wound_modifiers(
            wounds, hit_rolls, weapon, model, attacker_unit, defender, special_effects
        )

        # Check for Bane (wounds bypass regeneration)
        has_bane = weapon.has_rule("Bane")

        # Check for Bane in Melee (model/unit rule that grants Bane in melee)
        if phase == CombatPhase.MELEE and not has_bane:
            has_bane_in_melee = (
                model.has_rule("Bane in Melee")
                or attacker_unit.has_rule("Bane in Melee")
                or weapon.has_rule("Bane in Melee")
            )
            if has_bane_in_melee:
                has_bane = True
                special_effects["bane_in_melee"] = 1

        if has_bane:
            special_effects["bane"] = 1

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

    def _calculate_attacks(
        self,
        model: Model,
        weapon: Weapon,
        unit: Unit,
        is_charging: bool,
    ) -> int:
        """Calculate total attacks including modifiers."""
        attacks = weapon.attacks
        # NOTE: Furious is handled in _apply_hit_modifiers, not here.
        # Furious gives extra hits on 6s, not extra attacks.
        return attacks

    def _calculate_ap(
        self,
        weapon: Weapon,
        is_charging: bool,
        model: Model,
        unit: Unit,
    ) -> int:
        """Calculate effective AP including modifiers."""
        ap = weapon.ap

        # Lance: +2 AP when charging
        if is_charging:
            lance = weapon.get_rule("Lance")
            if lance:
                bonus = lance.value if lance.value else 2
                ap += bonus

        return ap

    def _apply_hit_modifiers(
        self,
        hits: int,
        hit_rolls: NDArray[np.int_],
        weapon: Weapon,
        model: Model,
        unit: Unit,
        defender: Unit,
        is_charging: bool = False,
    ) -> tuple[int, dict[str, int]]:
        """Apply modifiers that affect hits (Blast, Furious, etc.)

        Returns:
            Tuple of (modified hits, special effects dict)
        """
        effects: dict[str, int] = {}
        modified_hits = hits

        # Furious: When charging, unmodified results of 6 to hit deal 1 extra hit
        # (only the original hit counts as a 6 for special rules)
        if is_charging:
            furious = model.get_rule("Furious") or unit.get_rule("Furious")
            if furious:
                sixes = self.dice.count_sixes(hit_rolls)
                if sixes > 0:
                    modified_hits += sixes
                    effects["furious_extra_hits"] = sixes

        # Blast(X): Multiply each hit by X, where X is capped at the number of
        # models in the target unit. Example: Blast(3) vs 2 models = multiply by 2.
        blast_rule = weapon.get_rule("Blast")
        if blast_rule and blast_rule.value:
            # Cap the multiplier at the number of models in the target unit
            multiplier = min(blast_rule.value, defender.model_count)
            modified_hits = modified_hits * multiplier
            effects["blast_multiplier"] = multiplier

        return modified_hits, effects

    def _apply_wound_modifiers(
        self,
        wounds: int,
        hit_rolls: NDArray[np.int_],
        weapon: Weapon,
        model: Model,
        attacker_unit: Unit,
        defender: Unit,
        effects: dict[str, int],
    ) -> int:
        """Apply modifiers that affect wounds (Deadly, Rending)."""
        modified_wounds = wounds

        # Deadly(X): Multiply wounds by X (against single model)
        deadly_rule = weapon.get_rule("Deadly")
        if deadly_rule and deadly_rule.value and wounds > 0:
            # Deadly applies per wound against a single model
            # Complex to model exactly, so we multiply total wounds
            # This is an approximation - real game would pick target
            multiplier = deadly_rule.value
            modified_wounds = wounds * multiplier
            effects["deadly_multiplier"] = multiplier

        # Rending: 6s to hit count as AP(4)
        # This is already partially handled in AP calculation
        # Here we track for informational purposes
        rending_rule = weapon.get_rule("Rending")
        if rending_rule:
            sixes = self.dice.count_sixes(hit_rolls)
            if sixes > 0:
                effects["rending_hits"] = sixes

        return modified_wounds


def resolve_combat(
    attacker: Unit,
    defender: Unit,
    phase: CombatPhase,
    is_charging: bool = False,
    in_cover: bool = False,
) -> CombatResult:
    """Convenience function to resolve combat."""
    resolver = CombatResolver()
    return resolver.resolve_attack(
        attacker=attacker,
        defender=defender,
        phase=phase,
        is_charging=is_charging,
        in_cover=in_cover,
    )
