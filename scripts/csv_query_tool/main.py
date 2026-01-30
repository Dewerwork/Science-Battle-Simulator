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


def show_error_dialog(title: str, message: str) -> None:
    """Show an error dialog using tkinter.

    Args:
        title: Dialog title.
        message: Error message to display.
    """
    try:
        import tkinter as tk
        from tkinter import messagebox

        # Create hidden root window
        root = tk.Tk()
        root.withdraw()

        # Show error dialog
        messagebox.showerror(title, message)

        root.destroy()
    except Exception:
        # Fallback to print if tkinter fails
        print(f"{title}: {message}", file=sys.stderr)


def show_warning_dialog(title: str, message: str) -> None:
    """Show a warning dialog using tkinter.

    Args:
        title: Dialog title.
        message: Warning message to display.
    """
    try:
        import tkinter as tk
        from tkinter import messagebox

        # Create hidden root window
        root = tk.Tk()
        root.withdraw()

        # Show warning dialog
        messagebox.showwarning(title, message)

        root.destroy()
    except Exception:
        # Fallback to print if tkinter fails
        print(f"{title}: {message}", file=sys.stderr)


def check_dependencies() -> bool:
    """Check if all required dependencies are installed.

    Returns:
        True if all dependencies are available.
    """
    missing = []
    warnings = []

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
        warnings.append("matplotlib not installed. Charts will be disabled.\n\n"
                       "Install with: pip install matplotlib")

    # Show warnings (non-blocking)
    for warning in warnings:
        show_warning_dialog("Optional Dependency", warning)

    # Show errors for missing required dependencies
    if missing:
        error_msg = "Missing required dependencies:\n\n"
        for dep in missing:
            error_msg += f"  - {dep}\n"
        error_msg += f"\nInstall with:\n  pip install {' '.join(missing)}"

        show_error_dialog("Missing Dependencies", error_msg)
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

    # Import and run app (handle both module and direct execution)
    try:
        from .app import CSVQueryApp
    except ImportError:
        from app import CSVQueryApp

    # Validate file if provided
    initial_file = None
    if args.file:
        file_path = Path(args.file)
        if file_path.exists():
            initial_file = str(file_path.absolute())
        else:
            show_warning_dialog("File Not Found", f"File not found: {args.file}")

    # Create and run app with exception handling
    try:
        app = CSVQueryApp(initial_file=initial_file)
        app.run()
    except Exception as e:
        show_error_dialog("Application Error", f"An error occurred:\n\n{str(e)}")
        sys.exit(1)


if __name__ == "__main__":
    main()
