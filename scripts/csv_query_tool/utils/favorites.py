"""Favorites management for CSV Query Tool."""

import json
import logging
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import List, Optional

# Handle imports for both module and frozen/direct execution
try:
    from .config import Config
except ImportError:
    from config import Config

_logger = logging.getLogger(__name__)


@dataclass
class FavoriteQuery:
    """A saved favorite query."""
    name: str
    query: str
    created: str
    description: Optional[str] = None

    @classmethod
    def create(cls, name: str, query: str,
               description: Optional[str] = None) -> "FavoriteQuery":
        """Create a new favorite query with current timestamp.

        Args:
            name: Display name for the query.
            query: The SQL query.
            description: Optional description.

        Returns:
            New FavoriteQuery instance.
        """
        return cls(
            name=name.strip(),
            query=query.strip(),
            created=datetime.now().isoformat(),
            description=description
        )


class QueryFavorites:
    """Manages saved favorite queries with persistence."""

    def __init__(self):
        """Initialize favorites manager."""
        self._favorites: List[FavoriteQuery] = []
        self._favorites_path = Config.get_config_dir() / "favorites.json"
        self._load()

    def _load(self) -> None:
        """Load favorites from file."""
        if self._favorites_path.exists():
            try:
                with open(self._favorites_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                self._favorites = [
                    FavoriteQuery(**entry) for entry in data
                    if isinstance(entry, dict) and "name" in entry and "query" in entry
                ]
            except (json.JSONDecodeError, TypeError, KeyError) as e:
                _logger.warning(f"Could not load favorites ({e})")
                self._favorites = []

    def _save(self) -> None:
        """Save favorites to file."""
        try:
            with open(self._favorites_path, "w", encoding="utf-8") as f:
                json.dump([asdict(f) for f in self._favorites], f, indent=2)
        except (OSError, IOError) as e:
            _logger.warning(f"Could not save favorites: {e}")

    def add(self, name: str, query: str,
            description: Optional[str] = None) -> bool:
        """Add a query to favorites.

        Args:
            name: Display name for the query.
            query: The SQL query.
            description: Optional description.

        Returns:
            True if added, False if name already exists.
        """
        name = name.strip()
        query = query.strip()

        if not name or not query:
            return False

        # Check for duplicate names
        if any(f.name.lower() == name.lower() for f in self._favorites):
            return False

        favorite = FavoriteQuery.create(name, query, description)
        self._favorites.append(favorite)
        self._save()
        return True

    def update(self, name: str, query: str,
               description: Optional[str] = None) -> bool:
        """Update an existing favorite.

        Args:
            name: Name of the favorite to update.
            query: New SQL query.
            description: New description.

        Returns:
            True if updated, False if not found.
        """
        for i, fav in enumerate(self._favorites):
            if fav.name.lower() == name.lower():
                self._favorites[i] = FavoriteQuery.create(name, query, description)
                self._save()
                return True
        return False

    def remove(self, name: str) -> bool:
        """Remove a favorite by name.

        Args:
            name: Name of the favorite to remove.

        Returns:
            True if removed, False if not found.
        """
        for i, fav in enumerate(self._favorites):
            if fav.name.lower() == name.lower():
                del self._favorites[i]
                self._save()
                return True
        return False

    def get(self, name: str) -> Optional[FavoriteQuery]:
        """Get a favorite by name.

        Args:
            name: Name of the favorite.

        Returns:
            FavoriteQuery or None if not found.
        """
        for fav in self._favorites:
            if fav.name.lower() == name.lower():
                return fav
        return None

    def get_all(self) -> List[FavoriteQuery]:
        """Get all favorites.

        Returns:
            List of all favorite queries.
        """
        return list(self._favorites)

    def get_names(self) -> List[str]:
        """Get just the favorite names.

        Returns:
            List of favorite names.
        """
        return [f.name for f in self._favorites]

    def clear(self) -> None:
        """Clear all favorites."""
        self._favorites = []
        self._save()

    def rename(self, old_name: str, new_name: str) -> bool:
        """Rename a favorite.

        Args:
            old_name: Current name.
            new_name: New name.

        Returns:
            True if renamed, False if not found or new name exists.
        """
        new_name = new_name.strip()
        if not new_name:
            return False

        # Check if new name already exists
        if any(f.name.lower() == new_name.lower() for f in self._favorites):
            return False

        for fav in self._favorites:
            if fav.name.lower() == old_name.lower():
                fav.name = new_name
                self._save()
                return True
        return False

    def __len__(self) -> int:
        """Return number of favorites."""
        return len(self._favorites)

    def __iter__(self):
        """Iterate over favorites."""
        return iter(self._favorites)
