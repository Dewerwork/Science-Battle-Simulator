#!/usr/bin/env python3
"""
Entry point for PyInstaller executable.

This script sets up the package imports correctly for frozen executables.
"""

import sys
import os

# When running as frozen exe, set up the import path
if getattr(sys, 'frozen', False):
    # Get the directory where the exe is located
    bundle_dir = sys._MEIPASS
    # Add it to the path so csv_query_tool package can be found
    if bundle_dir not in sys.path:
        sys.path.insert(0, bundle_dir)

# Now import and run the app
from csv_query_tool.main import main

if __name__ == '__main__':
    main()
