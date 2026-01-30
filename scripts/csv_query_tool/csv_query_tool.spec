# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec file for CSV Query Tool.

Build Commands:
    Windows:    pyinstaller csv_query_tool.spec
    macOS:      pyinstaller csv_query_tool.spec
    Linux:      pyinstaller csv_query_tool.spec

The output will be in the 'dist' folder.
"""

import sys
from pathlib import Path

# Get the directory containing this spec file
spec_dir = Path(SPECPATH)

block_cipher = None

a = Analysis(
    ['main.py'],
    pathex=[str(spec_dir)],
    binaries=[],
    datas=[],
    hiddenimports=[
        'duckdb',
        'pandas',
        'pandas._libs.tslibs.timedeltas',
        'pandas._libs.tslibs.nattype',
        'pandas._libs.tslibs.np_datetime',
        'matplotlib',
        'matplotlib.backends.backend_tkagg',
        'tkinter',
        'tkinter.ttk',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        'pytest',
        'sphinx',
        'IPython',
        'jupyter',
        'notebook',
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='csv_query_tool',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,  # No console window - GUI only
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,  # Add icon path here if you have one
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='csv_query_tool',
)

# macOS app bundle (only on macOS)
if sys.platform == 'darwin':
    app = BUNDLE(
        coll,
        name='CSV Query Tool.app',
        icon=None,  # Add .icns icon path here if you have one
        bundle_identifier='com.sciencebattle.csvquerytool',
        info_plist={
            'CFBundleName': 'CSV Query Tool',
            'CFBundleDisplayName': 'CSV Query Tool',
            'CFBundleVersion': '1.0.0',
            'CFBundleShortVersionString': '1.0.0',
            'NSHighResolutionCapable': 'True',
        },
    )
