"""Query history management for CSV Query Tool."""

import json
import logging
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import List, Optional

from .config import Config

_logger = logging.getLogger(__name__)


@dataclass
class HistoryEntry:
    """A single query history entry."""
    query: str
    timestamp: str
    execution_time_ms: Optional[float] = None
    row_count: Optional[int] = None
    success: bool = True

    @classmethod
    def create(cls, query: str, execution_time_ms: Optional[float] = None,
               row_count: Optional[int] = None, success: bool = True) -> "HistoryEntry":
        """Create a new history entry with current timestamp.

        Args:
            query: The SQL query.
            execution_time_ms: Execution time in milliseconds.
            row_count: Number of rows returned.
            success: Whether query succeeded.

        Returns:
            New HistoryEntry instance.
        """
        return cls(
            query=query.strip(),
            timestamp=datetime.now().isoformat(),
            execution_time_ms=execution_time_ms,
            row_count=row_count,
            success=success
        )


class QueryHistory:
    """Manages query history with persistence."""

    def __init__(self, max_size: int = 50):
        """Initialize query history.

        Args:
            max_size: Maximum number of entries to keep.
        """
        self.max_size = max_size
        self._entries: List[HistoryEntry] = []
        self._history_path = Config.get_config_dir() / "query_history.json"
        self._load()

    def _load(self) -> None:
        """Load history from file."""
        if self._history_path.exists():
            try:
                with open(self._history_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                self._entries = [
                    HistoryEntry(**entry) for entry in data
                    if isinstance(entry, dict) and "query" in entry
                ]
            except (json.JSONDecodeError, TypeError, KeyError) as e:
                _logger.warning(f"Could not load query history ({e})")
                self._entries = []

    def _save(self) -> None:
        """Save history to file."""
        try:
            with open(self._history_path, "w", encoding="utf-8") as f:
                json.dump([asdict(e) for e in self._entries], f, indent=2)
        except (OSError, IOError) as e:
            _logger.warning(f"Could not save query history: {e}")

    def add(self, query: str, execution_time_ms: Optional[float] = None,
            row_count: Optional[int] = None, success: bool = True) -> None:
        """Add a query to history.

        Args:
            query: The SQL query.
            execution_time_ms: Execution time in milliseconds.
            row_count: Number of rows returned.
            success: Whether query succeeded.
        """
        query = query.strip()
        if not query:
            return

        # Don't add duplicates of the most recent query
        if self._entries and self._entries[0].query == query:
            # Update the existing entry instead
            self._entries[0] = HistoryEntry.create(
                query, execution_time_ms, row_count, success
            )
        else:
            # Add new entry at the front
            entry = HistoryEntry.create(query, execution_time_ms, row_count, success)
            self._entries.insert(0, entry)

            # Trim to max size
            self._entries = self._entries[:self.max_size]

        self._save()

    def get_entries(self) -> List[HistoryEntry]:
        """Get all history entries.

        Returns:
            List of history entries, most recent first.
        """
        return list(self._entries)

    def get_queries(self) -> List[str]:
        """Get just the query strings.

        Returns:
            List of query strings, most recent first.
        """
        return [e.query for e in self._entries]

    def get_successful_queries(self) -> List[str]:
        """Get only successful query strings.

        Returns:
            List of successful query strings, most recent first.
        """
        return [e.query for e in self._entries if e.success]

    def clear(self) -> None:
        """Clear all history."""
        self._entries = []
        self._save()

    def search(self, term: str) -> List[HistoryEntry]:
        """Search history for queries containing a term.

        Args:
            term: Search term (case-insensitive).

        Returns:
            Matching history entries.
        """
        term = term.lower()
        return [e for e in self._entries if term in e.query.lower()]

    def __len__(self) -> int:
        """Return number of history entries."""
        return len(self._entries)

    def __iter__(self):
        """Iterate over history entries."""
        return iter(self._entries)
