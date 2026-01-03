#!/usr/bin/env python3
"""
CSV Query Tool - SQL-based CSV analysis application.

A desktop application for querying CSV files using SQL syntax,
designed for analyzing Science Battle Simulator results.

Usage:
    python -m csv_query_tool [csv_file]

Examples:
    python -m csv_query_tool
    python -m csv_query_tool results/simulation_stats.csv

Requirements:
    pip install duckdb pandas matplotlib
"""

import argparse
import sys
from pathlib import Path


def check_dependencies() -> bool:
    """Check if all required dependencies are installed.

    Returns:
        True if all dependencies are available.
    """
    missing = []

    try:
        import duckdb
    except ImportError:
        missing.append("duckdb")

    try:
        import pandas
    except ImportError:
        missing.append("pandas")

    # Matplotlib is optional but recommended
    try:
        import matplotlib
    except ImportError:
        print("Warning: matplotlib not installed. Charts will be disabled.")
        print("Install with: pip install matplotlib")

    if missing:
        print("Error: Missing required dependencies:")
        for dep in missing:
            print(f"  - {dep}")
        print("\nInstall with:")
        print(f"  pip install {' '.join(missing)}")
        return False

    return True


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="CSV Query Tool - SQL-based CSV analysis",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                           # Start with no file loaded
  %(prog)s simulation_stats.csv      # Load a CSV file on startup
  %(prog)s results/*.csv             # Load multiple files (glob)

Keyboard Shortcuts:
  Ctrl+Enter  Run query
  Ctrl+L      Clear query
  Ctrl+O      Open CSV file
  Ctrl+Q      Quit
"""
    )

    parser.add_argument(
        "file",
        nargs="?",
        help="CSV file to load on startup"
    )

    parser.add_argument(
        "--version",
        action="version",
        version="CSV Query Tool 1.0.0"
    )

    args = parser.parse_args()

    # Check dependencies before importing app
    if not check_dependencies():
        sys.exit(1)

    # Import and run app
    from .app import CSVQueryApp

    # Validate file if provided
    initial_file = None
    if args.file:
        file_path = Path(args.file)
        if file_path.exists():
            initial_file = str(file_path.absolute())
        else:
            print(f"Warning: File not found: {args.file}")

    # Create and run app
    app = CSVQueryApp(initial_file=initial_file)
    app.run()


if __name__ == "__main__":
    main()
