#!/usr/bin/env python3
"""
Calculate Elo ratings from chunk_sim sample files.
Reads all chunk_X_samples.bin files and computes Elo for each unit.
"""

import struct
import glob
import os
import sys
from collections import defaultdict

def read_samples(filepath):
    """Read MatchupSample records from a .bin file with LPMS header."""
    samples = []

    with open(filepath, 'rb') as f:
        # Read header (16 bytes minimum)
        header = f.read(16)
        if len(header) < 16:
            return samples

        magic = header[0:4].decode('ascii', errors='replace')

        if magic == 'LPMS':
            # LPMS format: 4 magic + 4 version + 8 sample_rate (double)
            # Samples start at offset 16
            f.seek(16)
        elif magic == 'SMPL':
            # SMPL format: may have different header size
            f.seek(32)
        else:
            # Unknown format, try reading from start
            f.seek(0)

        # Read all samples (16 bytes each)
        while True:
            data = f.read(16)
            if len(data) < 16:
                break

            # MatchupSample format (16 bytes packed):
            # u16 unit_a_id, u16 unit_b_id, u8 outcome, u8 padding
            # u16 rounds, u16 a_models_lost, u16 b_models_lost
            # u16 a_wounds_dealt, u16 b_wounds_dealt
            unit_a, unit_b, outcome, padding, rounds, a_lost, b_lost, a_wounds, b_wounds = \
                struct.unpack('<HHBBHHHhh', data)

            # Outcome: 0=A wins, 1=B wins, 2=Draw
            samples.append((unit_a, unit_b, outcome))

    return samples

def calculate_elo(samples, k=32, initial_elo=1500):
    """Calculate Elo ratings from matchup samples."""

    # Track wins/losses/draws for each unit
    results = defaultdict(lambda: {'wins': 0, 'losses': 0, 'draws': 0})

    for unit_a, unit_b, outcome in samples:
        if outcome == 0:  # A wins
            results[unit_a]['wins'] += 1
            results[unit_b]['losses'] += 1
        elif outcome == 1:  # B wins
            results[unit_a]['losses'] += 1
            results[unit_b]['wins'] += 1
        else:  # Draw
            results[unit_a]['draws'] += 1
            results[unit_b]['draws'] += 1

    # Initialize Elo ratings
    elo = {unit: initial_elo for unit in results.keys()}

    # Run multiple iterations to converge
    for iteration in range(50):
        new_elo = {unit: initial_elo for unit in results.keys()}

        # Process each sample and update ratings
        for unit_a, unit_b, outcome in samples:
            if unit_a not in elo or unit_b not in elo:
                continue

            # Expected scores
            exp_a = 1 / (1 + 10 ** ((elo[unit_b] - elo[unit_a]) / 400))
            exp_b = 1 - exp_a

            # Actual scores
            if outcome == 0:  # A wins
                score_a, score_b = 1.0, 0.0
            elif outcome == 1:  # B wins
                score_a, score_b = 0.0, 1.0
            else:  # Draw
                score_a, score_b = 0.5, 0.5

            # Update (scaled down for stability with many games)
            scale = k / (1 + len(samples) / 10000)
            new_elo[unit_a] += scale * (score_a - exp_a)
            new_elo[unit_b] += scale * (score_b - exp_b)

        elo = new_elo

    return elo, results

def main():
    if len(sys.argv) < 2:
        print("Usage: python elo_from_samples.py <directory_with_sample_files> [units.txt]")
        print("Example: python elo_from_samples.py 'C:/Users/David/Documents/Army Factions/Chunk Planner Agg'")
        sys.exit(1)

    sample_dir = sys.argv[1]
    units_file = sys.argv[2] if len(sys.argv) > 2 else None

    # Load unit names if provided
    unit_names = {}
    if units_file and os.path.exists(units_file):
        with open(units_file, 'r') as f:
            for i, line in enumerate(f):
                unit_names[i] = line.strip()

    # Find all sample files
    pattern = os.path.join(sample_dir, "chunk_*_samples.bin")
    sample_files = sorted(glob.glob(pattern))

    if not sample_files:
        print(f"No sample files found matching: {pattern}")
        sys.exit(1)

    print(f"Found {len(sample_files)} sample files")

    # Read all samples
    all_samples = []
    for filepath in sample_files:
        filename = os.path.basename(filepath)
        samples = read_samples(filepath)
        all_samples.extend(samples)
        print(f"  {filename}: {len(samples):,} samples")

    print(f"\nTotal samples: {len(all_samples):,}")

    if not all_samples:
        print("No samples found!")
        sys.exit(1)

    # Calculate Elo
    print("\nCalculating Elo ratings...")
    elo, results = calculate_elo(all_samples)

    # Sort by Elo
    sorted_units = sorted(elo.items(), key=lambda x: x[1], reverse=True)

    # Print results
    print("\n" + "="*70)
    print(f"{'Rank':<6}{'Unit ID':<10}{'Elo':<10}{'W':<10}{'L':<10}{'D':<10}{'Name'}")
    print("="*70)

    for rank, (unit_id, rating) in enumerate(sorted_units[:50], 1):
        stats = results[unit_id]
        name = unit_names.get(unit_id, "")
        print(f"{rank:<6}{unit_id:<10}{rating:<10.1f}{stats['wins']:<10}{stats['losses']:<10}{stats['draws']:<10}{name}")

    if len(sorted_units) > 50:
        print(f"\n... and {len(sorted_units) - 50} more units")

    # Save to CSV
    csv_path = os.path.join(sample_dir, "elo_ratings.csv")
    with open(csv_path, 'w') as f:
        f.write("rank,unit_id,elo,wins,losses,draws,name\n")
        for rank, (unit_id, rating) in enumerate(sorted_units, 1):
            stats = results[unit_id]
            name = unit_names.get(unit_id, "").replace(",", ";")
            f.write(f"{rank},{unit_id},{rating:.1f},{stats['wins']},{stats['losses']},{stats['draws']},{name}\n")

    print(f"\nFull results saved to: {csv_path}")

if __name__ == "__main__":
    main()
