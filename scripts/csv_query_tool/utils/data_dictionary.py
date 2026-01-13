"""Data dictionary with column definitions for batch_test CSV files."""

from typing import Dict, Optional


# Column definitions for batch_test CSV files
# Format: {table_name: {column_name: description}}

DATA_DICTIONARY: Dict[str, Dict[str, str]] = {
    "matchups": {
        "matchup_id": "Unique identifier for each matchup configuration",
        "matchup_name": "Name/description of the test matchup scenario",
        "iterations": "Number of times this matchup was simulated",
        "unit_a_name": "Name of Unit A (first combatant)",
        "unit_a_models": "Number of models in Unit A at start",
        "unit_a_quality": "Quality stat of Unit A (hit roll target)",
        "unit_a_defense": "Defense stat of Unit A (save roll target)",
        "unit_a_points": "Points cost of Unit A",
        "unit_a_rules": "Special rules assigned to Unit A",
        "unit_a_weapons": "Weapons equipped by Unit A",
        "unit_b_name": "Name of Unit B (second combatant)",
        "unit_b_models": "Number of models in Unit B at start",
        "unit_b_quality": "Quality stat of Unit B (hit roll target)",
        "unit_b_defense": "Defense stat of Unit B (save roll target)",
        "unit_b_points": "Points cost of Unit B",
        "unit_b_rules": "Special rules assigned to Unit B",
        "unit_b_weapons": "Weapons equipped by Unit B",
    },
    "iterations": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup (0-indexed)",
        "seed": "Random seed used for this iteration",
        "winner": "Overall winner: 'A', 'B', or 'Draw'",
        "games_won_a": "Number of games won by Unit A in this iteration",
        "games_won_b": "Number of games won by Unit B in this iteration",
        "total_wounds_dealt_a": "Total wounds dealt by Unit A across all games",
        "total_wounds_dealt_b": "Total wounds dealt by Unit B across all games",
        "total_models_killed_a": "Total models killed by Unit A across all games",
        "total_models_killed_b": "Total models killed by Unit B across all games",
    },
    "rounds": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration (1-indexed)",
        "round": "Round number within the game (1-indexed)",
        "initiative_roll": "Die roll result for initiative",
        "unit_a_first": "1 if Unit A acts first this round, 0 otherwise",
        "initiative_reason": "Explanation of how initiative was determined",
        "a_controls_objective": "1 if Unit A controls the objective, 0 otherwise",
        "b_controls_objective": "1 if Unit B controls the objective, 0 otherwise",
        "control_reason": "Explanation of objective control determination",
        "unit_a_models_remaining": "Models remaining in Unit A at round end",
        "unit_b_models_remaining": "Models remaining in Unit B at round end",
        "unit_a_status": "Unit A status (e.g., 'active', 'shaken', 'routed')",
        "unit_b_status": "Unit B status (e.g., 'active', 'shaken', 'routed')",
    },
    "movements": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration",
        "round": "Round number within the game",
        "unit": "Which unit moved: 'A' or 'B'",
        "move_seq": "Sequence number of movement within activation",
        "move_type": "Type of movement (e.g., 'advance', 'charge', 'retreat')",
        "from_pos": "Starting position on the battlefield",
        "to_pos": "Ending position on the battlefield",
        "distance_moved": "Distance moved in inches",
        "reason": "Explanation for the movement decision",
    },
    "morale": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration",
        "round": "Round number within the game",
        "unit": "Which unit is taking morale check: 'A' or 'B'",
        "morale_seq": "Sequence number of morale check within activation",
        "unit_name": "Name of the unit taking the morale check",
        "trigger_reason": "What triggered this morale check",
        "models_remaining": "Models remaining when check was taken",
        "models_total": "Original model count of the unit",
        "wounds_taken": "Wounds taken that triggered the check",
        "wounds_dealt": "Wounds this unit dealt (for comparison)",
        "morale_roll": "Die roll result for morale check",
        "morale_target": "Target number needed to pass morale",
        "morale_passed": "1 if base morale check passed, 0 otherwise",
        "had_fearless_reroll": "1 if Fearless reroll was available",
        "fearless_roll": "Fearless reroll result (if applicable)",
        "fearless_target": "Target for Fearless reroll",
        "fearless_passed": "1 if Fearless reroll passed",
        "final_passed": "1 if morale was ultimately passed",
        "old_status": "Unit status before morale resolution",
        "new_status": "Unit status after morale resolution",
        "result_description": "Human-readable description of morale result",
    },
    "attacks": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration",
        "round": "Round number within the game",
        "attacker": "Which unit is attacking: 'A' or 'B'",
        "attack_seq": "Sequence number of attack within activation",
        "weapon": "Name of the weapon used",
        "is_melee": "1 if melee attack, 0 if ranged",
        "distance": "Distance to target (for range validation)",
        "models_attacking": "Number of models making attacks",
        "total_attacks": "Total number of attack dice rolled",
        "hit_target": "Target number needed to hit (Quality+)",
        "hits": "Number of successful hit rolls",
        "hit_sixes": "Number of natural 6s on hit rolls (for Rending, etc.)",
        "ap_applied": "Total AP (Armor Piercing) value applied",
        "defense_target": "Target number for defense saves (after AP)",
        "saves": "Number of successful defense saves",
        "wounds": "Number of wounds inflicted (failed saves)",
        "wounds_allocated": "Wounds allocated to models",
        "models_killed": "Number of models killed by this attack",
        "overkill": "Overkill wounds (excess wounds on killed models)",
    },
    "rule_triggers": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration",
        "round": "Round number (0 = deployment phase)",
        "attacker": "Which unit triggered the rule: 'A' or 'B'",
        "attack_seq": "Attack sequence (0 if not attack-related)",
        "rule_name": "Name of the special rule that triggered",
        "effect": "Description of the rule's effect",
        "value": "Numeric value associated with the rule effect",
        "phase": "Game phase when rule triggered",
    },
    "rolls": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration",
        "round": "Round number within the game",
        "attacker": "Which unit made the roll: 'A' or 'B'",
        "attack_seq": "Attack sequence number",
        "phase": "Roll type: 'hit', 'defense', 'rending_defense', 'regeneration'",
        "roll_index": "Index of this roll within the phase",
        "roll_value": "Actual die roll result (1-6)",
        "target": "Target number needed for success",
        "success": "1 if roll was successful, 0 otherwise",
    },
    "spells": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration",
        "round": "Round number within the game",
        "caster": "Which unit is casting: 'A' or 'B'",
        "spell_seq": "Sequence number of spell cast within activation",
        "spell_name": "Name of the spell being cast",
        "spell_cost": "Token cost to cast this spell",
        "tokens_before": "Spell tokens before casting",
        "tokens_after": "Spell tokens after casting",
        "range": "Range of the spell in inches",
        "target_type": "What the spell targets (e.g., 'enemy', 'self')",
        "was_interfered": "1 if opponent used Interference",
        "interference_tokens": "Tokens spent on interference",
        "interference_modifier": "Modifier from interference",
        "roll": "Casting roll result",
        "target_number": "Target number needed to cast",
        "total_modifier": "Total modifier to the roll",
        "success": "1 if spell was successfully cast",
        "effect_type": "Type of spell effect (damage, buff, etc.)",
        "hits_dealt": "Hits dealt by damage spell",
        "wounds_dealt": "Wounds dealt by damage spell",
        "models_killed": "Models killed by damage spell",
        "buff_applied": "Name of buff applied (if applicable)",
    },
    "spell_tokens": {
        "matchup_id": "Foreign key to matchups table",
        "iteration": "Iteration number within the matchup",
        "game": "Game number within the iteration",
        "round": "Round number within the game",
        "unit": "Which unit's tokens changed: 'A' or 'B'",
        "token_seq": "Sequence number of token event",
        "event_type": "Type of token event (grant, spend, etc.)",
        "tokens_changed": "Number of tokens gained or spent",
        "tokens_before": "Token count before the event",
        "tokens_after": "Token count after the event",
        "caster_value": "Casting value that generated tokens",
        "spell_name": "Spell name if tokens spent on spell",
    },
}

# Table-level descriptions
TABLE_DESCRIPTIONS: Dict[str, str] = {
    "matchups": "Configuration data for each test matchup including unit stats and loadouts",
    "iterations": "Summary results for each iteration of a matchup simulation",
    "rounds": "Round-by-round state including initiative and objective control",
    "movements": "Individual movement events with positions and distances",
    "morale": "Detailed morale check information including rolls and status changes",
    "attacks": "Attack resolution details with hit/defense rolls and damage",
    "rule_triggers": "Special rule activations and their effects",
    "rolls": "Individual dice roll results for statistical analysis",
    "spells": "Spell casting attempts with all modifiers and results",
    "spell_tokens": "Spell token economy tracking (grants and spends)",
}


def get_column_description(table_name: str, column_name: str) -> Optional[str]:
    """Get the description for a specific column.

    Args:
        table_name: Name of the table (without .csv extension).
        column_name: Name of the column.

    Returns:
        Description string or None if not found.
    """
    # Normalize table name (remove .csv if present)
    table_name = table_name.replace(".csv", "").lower()

    table_dict = DATA_DICTIONARY.get(table_name)
    if table_dict:
        return table_dict.get(column_name.lower())
    return None


def get_table_description(table_name: str) -> Optional[str]:
    """Get the description for a table.

    Args:
        table_name: Name of the table (without .csv extension).

    Returns:
        Description string or None if not found.
    """
    # Normalize table name
    table_name = table_name.replace(".csv", "").lower()
    return TABLE_DESCRIPTIONS.get(table_name)


def get_all_columns_for_table(table_name: str) -> Dict[str, str]:
    """Get all column descriptions for a table.

    Args:
        table_name: Name of the table (without .csv extension).

    Returns:
        Dictionary of column_name -> description.
    """
    table_name = table_name.replace(".csv", "").lower()
    return DATA_DICTIONARY.get(table_name, {})


def is_known_table(table_name: str) -> bool:
    """Check if a table has data dictionary entries.

    Args:
        table_name: Name of the table.

    Returns:
        True if table is in the data dictionary.
    """
    table_name = table_name.replace(".csv", "").lower()
    return table_name in DATA_DICTIONARY
