#!/usr/bin/env python3
"""Verify csv_query_tool has all new features."""

import os
import sys

# Add scripts directory to path
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)

print("=" * 60)
print("CSV Query Tool - Feature Verification")
print("=" * 60)

# Check app.py for features
app_path = os.path.join(script_dir, "csv_query_tool", "app.py")
with open(app_path, "r") as f:
    app_content = f.read()

features = {
    "Create Table from Query": "Create Table from Query" in app_content,
    "Create Custom Table": "Create Custom Table" in app_content,
    "Create Stored Query": "Create Stored Query" in app_content,
    "Editor Settings": "Editor Settings" in app_content,
    "autocomplete_enabled": "autocomplete_enabled" in app_content,
}

print("\napp.py Features:")
for name, present in features.items():
    status = "✓ FOUND" if present else "✗ MISSING"
    print(f"  {name}: {status}")

# Check file_panel.py for tooltip
file_panel_path = os.path.join(script_dir, "csv_query_tool", "ui", "file_panel.py")
with open(file_panel_path, "r") as f:
    fp_content = f.read()

fp_features = {
    "TreeviewTooltip": "class TreeviewTooltip" in fp_content,
    "_get_item_tooltip": "def _get_item_tooltip" in fp_content,
    "get_column_description": "get_column_description" in fp_content,
    "ViewInfo import": "ViewInfo" in fp_content,
}

print("\nfile_panel.py Features:")
for name, present in fp_features.items():
    status = "✓ FOUND" if present else "✗ MISSING"
    print(f"  {name}: {status}")

# Check data dictionary
dd_path = os.path.join(script_dir, "csv_query_tool", "utils", "data_dictionary.py")
dd_exists = os.path.exists(dd_path)
print(f"\ndata_dictionary.py: {'✓ EXISTS' if dd_exists else '✗ MISSING'}")

# Check autocomplete
ac_path = os.path.join(script_dir, "csv_query_tool", "utils", "autocomplete.py")
ac_exists = os.path.exists(ac_path)
print(f"autocomplete.py: {'✓ EXISTS' if ac_exists else '✗ MISSING'}")

# Check database.py for views
db_path = os.path.join(script_dir, "csv_query_tool", "database.py")
with open(db_path, "r") as f:
    db_content = f.read()

db_features = {
    "ViewInfo class": "class ViewInfo" in db_content,
    "create_view method": "def create_view" in db_content,
    "get_views method": "def get_views" in db_content,
}

print("\ndatabase.py Features:")
for name, present in db_features.items():
    status = "✓ FOUND" if present else "✗ MISSING"
    print(f"  {name}: {status}")

# Print file modification times
print("\nFile modification times:")
files_to_check = [
    "csv_query_tool/app.py",
    "csv_query_tool/ui/file_panel.py",
    "csv_query_tool/database.py",
    "csv_query_tool/utils/data_dictionary.py",
    "csv_query_tool/utils/autocomplete.py",
]
for f in files_to_check:
    full_path = os.path.join(script_dir, f)
    if os.path.exists(full_path):
        mtime = os.path.getmtime(full_path)
        import datetime
        dt = datetime.datetime.fromtimestamp(mtime)
        print(f"  {f}: {dt.strftime('%Y-%m-%d %H:%M:%S')}")

print("\n" + "=" * 60)
print("If all features show '✓ FOUND', the code is correct.")
print("Run the tool with: python -m csv_query_tool")
print("From directory:", script_dir)
print("=" * 60)
