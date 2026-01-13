"""Configuration management for CSV Query Tool."""

import json
import os
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import List, Optional


@dataclass
class Config:
    """Application configuration."""

    # Window settings
    window_width: int = 1200
    window_height: int = 800
    window_x: Optional[int] = None
    window_y: Optional[int] = None

    # Query settings
    default_limit: int = 1000
    query_timeout_seconds: int = 60
    max_history_size: int = 50

    # Display settings
    results_page_size: int = 1000
    font_family: str = "Consolas"
    font_size: int = 11

    # Editor settings
    autocomplete_enabled: bool = True

    # Recent files
    recent_files: List[str] = field(default_factory=list)
    max_recent_files: int = 10

    # Paths
    last_open_directory: Optional[str] = None
    last_export_directory: Optional[str] = None

    @classmethod
    def get_config_dir(cls) -> Path:
        """Get the configuration directory path.

        Returns:
            Path to config directory.
        """
        # Use XDG_CONFIG_HOME on Linux, or fallback to ~/.config
        if os.name == "nt":  # Windows
            config_base = Path(os.environ.get("APPDATA", Path.home()))
        else:  # Linux/macOS
            config_base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))

        config_dir = config_base / "csv_query_tool"
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir

    @classmethod
    def get_config_path(cls) -> Path:
        """Get the configuration file path.

        Returns:
            Path to config file.
        """
        return cls.get_config_dir() / "config.json"

    @classmethod
    def load(cls) -> "Config":
        """Load configuration from file.

        Returns:
            Config instance with loaded or default values.
        """
        config_path = cls.get_config_path()

        if config_path.exists():
            try:
                with open(config_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                # Filter out unknown keys
                valid_fields = {f.name for f in cls.__dataclass_fields__.values()}
                filtered_data = {k: v for k, v in data.items() if k in valid_fields}
                return cls(**filtered_data)
            except (json.JSONDecodeError, TypeError, KeyError) as e:
                # If config is corrupted, return defaults
                print(f"Warning: Could not load config ({e}), using defaults")
                return cls()
        return cls()

    def save(self) -> None:
        """Save configuration to file."""
        config_path = self.get_config_path()

        try:
            with open(config_path, "w", encoding="utf-8") as f:
                json.dump(asdict(self), f, indent=2)
        except (OSError, IOError) as e:
            print(f"Warning: Could not save config: {e}")

    def add_recent_file(self, path: str) -> None:
        """Add a file to the recent files list.

        Args:
            path: Path to add.
        """
        # Normalize path
        path = os.path.abspath(path)

        # Remove if already exists
        if path in self.recent_files:
            self.recent_files.remove(path)

        # Add to front
        self.recent_files.insert(0, path)

        # Trim to max size
        self.recent_files = self.recent_files[:self.max_recent_files]

        self.save()

    def get_geometry(self) -> str:
        """Get window geometry string for Tkinter.

        Returns:
            Geometry string like "1200x800+100+100".
        """
        geo = f"{self.window_width}x{self.window_height}"
        if self.window_x is not None and self.window_y is not None:
            geo += f"+{self.window_x}+{self.window_y}"
        return geo

    def set_geometry(self, geometry: str) -> None:
        """Parse and save window geometry from Tkinter.

        Args:
            geometry: Geometry string like "1200x800+100+100".
        """
        try:
            # Parse geometry string
            if "+" in geometry:
                size_part, x, y = geometry.replace("+", " ").split()
                self.window_x = int(x)
                self.window_y = int(y)
            else:
                size_part = geometry

            if "x" in size_part:
                w, h = size_part.split("x")
                self.window_width = int(w)
                self.window_height = int(h)
        except (ValueError, AttributeError):
            pass  # Keep current values if parsing fails
