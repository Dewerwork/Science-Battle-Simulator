"""Monte Carlo simulation for multi-round combat."""

from dataclasses import dataclass, field
from typing import Callable

from src.engine.multi_round import (
    BattleWinner,
    MultiRoundCombat,
    MultiRoundResult,
    VictoryCondition,
)
from src.models.unit import Unit


@dataclass
class MultiRoundStatistics:
    """Statistics from multi-round combat simulations."""

    attacker_name: str
    defender_name: str
    attacker_points: int
    defender_points: int
    attacker_model_count: int
    defender_model_count: int
    iterations: int

    # Win/loss/draw counts
    attacker_wins: int = 0
    defender_wins: int = 0
    draws: int = 0

    # Win rates
    attacker_win_rate: float = 0.0
    defender_win_rate: float = 0.0
    draw_rate: float = 0.0

    # Victory condition breakdown
    victory_conditions: dict[VictoryCondition, int] = field(default_factory=dict)

    # Round statistics
    total_rounds: int = 0
    min_rounds: int = 0
    max_rounds: int = 0
    avg_rounds: float = 0.0

    # Rounds to win breakdown
    rounds_to_attacker_win: list[int] = field(default_factory=list)
    rounds_to_defender_win: list[int] = field(default_factory=list)
    avg_rounds_to_attacker_win: float = 0.0
    avg_rounds_to_defender_win: float = 0.0

    # Damage statistics
    total_wounds_by_attacker: int = 0
    total_wounds_by_defender: int = 0
    total_kills_by_attacker: int = 0
    total_kills_by_defender: int = 0

    # Averages
    avg_wounds_by_attacker: float = 0.0
    avg_wounds_by_defender: float = 0.0
    avg_kills_by_attacker: float = 0.0
    avg_kills_by_defender: float = 0.0

    # Remaining models
    total_attacker_remaining: int = 0
    total_defender_remaining: int = 0
    avg_attacker_remaining: float = 0.0
    avg_defender_remaining: float = 0.0

    # Rout statistics
    attacker_routs: int = 0
    defender_routs: int = 0
    attacker_rout_rate: float = 0.0
    defender_rout_rate: float = 0.0

    # Points efficiency
    attacker_efficiency: float = 0.0
    defender_efficiency: float = 0.0

    def compute_rates(self) -> None:
        """Compute rates and averages from totals."""
        if self.iterations <= 0:
            return

        # Win rates
        self.attacker_win_rate = self.attacker_wins / self.iterations
        self.defender_win_rate = self.defender_wins / self.iterations
        self.draw_rate = self.draws / self.iterations

        # Round statistics
        self.avg_rounds = self.total_rounds / self.iterations

        if self.rounds_to_attacker_win:
            self.avg_rounds_to_attacker_win = (
                sum(self.rounds_to_attacker_win) / len(self.rounds_to_attacker_win)
            )
        if self.rounds_to_defender_win:
            self.avg_rounds_to_defender_win = (
                sum(self.rounds_to_defender_win) / len(self.rounds_to_defender_win)
            )

        # Damage averages
        self.avg_wounds_by_attacker = self.total_wounds_by_attacker / self.iterations
        self.avg_wounds_by_defender = self.total_wounds_by_defender / self.iterations
        self.avg_kills_by_attacker = self.total_kills_by_attacker / self.iterations
        self.avg_kills_by_defender = self.total_kills_by_defender / self.iterations

        # Remaining averages
        self.avg_attacker_remaining = self.total_attacker_remaining / self.iterations
        self.avg_defender_remaining = self.total_defender_remaining / self.iterations

        # Rout rates
        self.attacker_rout_rate = self.attacker_routs / self.iterations
        self.defender_rout_rate = self.defender_routs / self.iterations

        # Points efficiency: win rate per 100 points
        if self.attacker_points > 0:
            self.attacker_efficiency = self.attacker_win_rate * 100 / self.attacker_points
        if self.defender_points > 0:
            self.defender_efficiency = self.defender_win_rate * 100 / self.defender_points

    def get_round_distribution(self) -> dict[int, int]:
        """Get distribution of battles ending on each round.

        Returns:
            Dict mapping round number to count of battles
        """
        distribution: dict[int, int] = {}
        all_rounds = self.rounds_to_attacker_win + self.rounds_to_defender_win

        for rounds in all_rounds:
            distribution[rounds] = distribution.get(rounds, 0) + 1

        return dict(sorted(distribution.items()))


@dataclass
class MultiRoundSimulationConfig:
    """Configuration for multi-round simulation."""

    iterations: int = 1000
    max_rounds: int = 10
    attacker_charges: bool = True


@dataclass
class MultiRoundSimulationResult:
    """Complete result of a multi-round simulation run."""

    attacker: Unit
    defender: Unit
    config: MultiRoundSimulationConfig
    statistics: MultiRoundStatistics
    raw_results: list[MultiRoundResult] = field(default_factory=list)


class MultiRoundSimulator:
    """Monte Carlo simulator for multi-round combat."""

    def __init__(self, config: MultiRoundSimulationConfig | None = None):
        """Initialize simulator.

        Args:
            config: Simulation configuration
        """
        self.config = config or MultiRoundSimulationConfig()
        self.combat_runner = MultiRoundCombat(max_rounds=self.config.max_rounds)

    def run_simulation(
        self,
        attacker: Unit,
        defender: Unit,
        progress_callback: Callable[[int, int], None] | None = None,
    ) -> MultiRoundSimulationResult:
        """Run a complete multi-round simulation.

        Args:
            attacker: Attacking unit
            defender: Defending unit
            progress_callback: Optional callback for progress updates (current, total)

        Returns:
            MultiRoundSimulationResult with all statistics
        """
        stats = MultiRoundStatistics(
            attacker_name=attacker.name,
            defender_name=defender.name,
            attacker_points=attacker.points_cost,
            defender_points=defender.points_cost,
            attacker_model_count=attacker.model_count,
            defender_model_count=defender.model_count,
            iterations=self.config.iterations,
        )

        all_results: list[MultiRoundResult] = []
        all_rounds: list[int] = []

        for i in range(self.config.iterations):
            result = self.combat_runner.run_battle(
                attacker=attacker,
                defender=defender,
                attacker_charges=self.config.attacker_charges,
            )

            # Accumulate win/loss/draw
            if result.winner == BattleWinner.ATTACKER:
                stats.attacker_wins += 1
                stats.rounds_to_attacker_win.append(result.total_rounds)
            elif result.winner == BattleWinner.DEFENDER:
                stats.defender_wins += 1
                stats.rounds_to_defender_win.append(result.total_rounds)
            else:
                stats.draws += 1

            # Track victory conditions
            vc = result.victory_condition
            stats.victory_conditions[vc] = stats.victory_conditions.get(vc, 0) + 1

            # Track routs
            if vc in (
                VictoryCondition.ATTACKER_ROUTED,
                VictoryCondition.DEFENDER_ROUTED_ENEMY,
            ):
                stats.attacker_routs += 1
            if vc in (
                VictoryCondition.DEFENDER_ROUTED,
                VictoryCondition.ATTACKER_ROUTED_ENEMY,
            ):
                stats.defender_routs += 1

            # Round statistics
            stats.total_rounds += result.total_rounds
            all_rounds.append(result.total_rounds)

            # Damage statistics
            stats.total_wounds_by_attacker += result.total_wounds_by_attacker
            stats.total_wounds_by_defender += result.total_wounds_by_defender
            stats.total_kills_by_attacker += result.total_kills_by_attacker
            stats.total_kills_by_defender += result.total_kills_by_defender

            # Remaining models
            stats.total_attacker_remaining += result.attacker_models_end
            stats.total_defender_remaining += result.defender_models_end

            all_results.append(result)

            if progress_callback:
                progress_callback(i + 1, self.config.iterations)

        # Compute min/max rounds
        if all_rounds:
            stats.min_rounds = min(all_rounds)
            stats.max_rounds = max(all_rounds)

        # Compute rates and averages
        stats.compute_rates()

        return MultiRoundSimulationResult(
            attacker=attacker,
            defender=defender,
            config=self.config,
            statistics=stats,
            raw_results=all_results,
        )


def simulate_multi_round_battle(
    attacker: Unit,
    defender: Unit,
    iterations: int = 1000,
    max_rounds: int = 10,
    attacker_charges: bool = True,
) -> MultiRoundStatistics:
    """Convenience function to run a multi-round simulation.

    Args:
        attacker: Attacking unit
        defender: Defending unit
        iterations: Number of iterations to run
        max_rounds: Maximum rounds per battle
        attacker_charges: Whether attacker charges first

    Returns:
        MultiRoundStatistics with aggregated results
    """
    config = MultiRoundSimulationConfig(
        iterations=iterations,
        max_rounds=max_rounds,
        attacker_charges=attacker_charges,
    )

    simulator = MultiRoundSimulator(config)
    result = simulator.run_simulation(attacker, defender)
    return result.statistics
