#!/usr/bin/env python3
"""
Build script for CSV Query Tool standalone application.

This script builds a standalone executable using PyInstaller.

Requirements:
    pip install pyinstaller

Usage:
    python build.py              # Build for current platform
    python build.py --onefile    # Build as single file (slower startup)
    python build.py --clean      # Clean build artifacts first

Output:
    Windows: dist/csv_query_tool/csv_query_tool.exe
    macOS:   dist/CSV Query Tool.app
    Linux:   dist/csv_query_tool/csv_query_tool
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def check_pyinstaller():
    """Check if PyInstaller is installed."""
    try:
        import PyInstaller
        return True
    except ImportError:
        return False


def clean_build():
    """Remove build artifacts."""
    dirs_to_remove = ['build', 'dist', '__pycache__']
    files_to_remove = ['*.pyc', '*.pyo']

    script_dir = Path(__file__).parent

    for dir_name in dirs_to_remove:
        dir_path = script_dir / dir_name
        if dir_path.exists():
            print(f"Removing {dir_path}")
            shutil.rmtree(dir_path)

    # Remove __pycache__ in subdirectories
    for pycache in script_dir.rglob('__pycache__'):
        print(f"Removing {pycache}")
        shutil.rmtree(pycache)


def build(onefile=False):
    """Build the application.

    Args:
        onefile: If True, build as single executable file.
    """
    script_dir = Path(__file__).parent

    # Base command
    cmd = [
        sys.executable, '-m', 'PyInstaller',
        '--noconfirm',
        '--clean',
    ]

    if onefile:
        # Single file build
        cmd.extend([
            '--onefile',
            '--windowed',
            '--name', 'csv_query_tool',
            '--hidden-import', 'duckdb',
            '--hidden-import', 'pandas',
            '--hidden-import', 'matplotlib',
            '--hidden-import', 'matplotlib.backends.backend_tkagg',
            str(script_dir / 'main.py'),
        ])
    else:
        # Use spec file for folder build
        cmd.append(str(script_dir / 'csv_query_tool.spec'))

    print(f"Running: {' '.join(cmd)}")
    print()

    # Run PyInstaller
    result = subprocess.run(cmd, cwd=script_dir)

    if result.returncode == 0:
        print()
        print("=" * 60)
        print("Build successful!")
        print("=" * 60)

        dist_dir = script_dir / 'dist'
        if onefile:
            if sys.platform == 'win32':
                print(f"Executable: {dist_dir / 'csv_query_tool.exe'}")
            else:
                print(f"Executable: {dist_dir / 'csv_query_tool'}")
        else:
            if sys.platform == 'darwin':
                print(f"Application: {dist_dir / 'CSV Query Tool.app'}")
            elif sys.platform == 'win32':
                print(f"Folder: {dist_dir / 'csv_query_tool'}")
                print(f"Executable: {dist_dir / 'csv_query_tool' / 'csv_query_tool.exe'}")
            else:
                print(f"Folder: {dist_dir / 'csv_query_tool'}")
                print(f"Executable: {dist_dir / 'csv_query_tool' / 'csv_query_tool'}")
    else:
        print()
        print("Build failed!")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Build CSV Query Tool")
    parser.add_argument('--onefile', action='store_true',
                       help='Build as single executable file')
    parser.add_argument('--clean', action='store_true',
                       help='Clean build artifacts before building')
    parser.add_argument('--clean-only', action='store_true',
                       help='Only clean, do not build')

    args = parser.parse_args()

    # Check PyInstaller
    if not check_pyinstaller():
        print("Error: PyInstaller is not installed.")
        print("Install with: pip install pyinstaller")
        sys.exit(1)

    # Clean if requested
    if args.clean or args.clean_only:
        clean_build()
        if args.clean_only:
            print("Clean complete.")
            return

    # Build
    build(onefile=args.onefile)


if __name__ == '__main__':
    main()
