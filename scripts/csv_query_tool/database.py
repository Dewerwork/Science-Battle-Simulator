"""Database manager using DuckDB for CSV querying."""

import os
import re
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

try:
    import duckdb
except ImportError:
    raise ImportError(
        "DuckDB is required. Install with: pip install duckdb"
    )

try:
    import pandas as pd
except ImportError:
    raise ImportError(
        "Pandas is required. Install with: pip install pandas"
    )

from .utils.error_handler import ErrorHandler, ErrorResult, ErrorSeverity


@dataclass
class TableInfo:
    """Information about a loaded table."""
    name: str
    path: str
    row_count: int
    columns: List[Tuple[str, str]]  # (name, type)


@dataclass
class QueryResult:
    """Result of a query execution."""
    success: bool
    data: Optional[pd.DataFrame]
    row_count: int
    execution_time_ms: float
    error: Optional[ErrorResult] = None


class DatabaseManager:
    """Manages DuckDB connection and CSV table loading."""

    def __init__(self, error_handler: Optional[ErrorHandler] = None):
        """Initialize database manager with in-memory DuckDB.

        Args:
            error_handler: Optional error handler for user-friendly messages.
        """
        self._conn = duckdb.connect(":memory:")
        self._tables: Dict[str, TableInfo] = {}
        self._error_handler = error_handler or ErrorHandler()

        # Configure DuckDB for better CSV handling
        self._conn.execute("SET threads TO 4")

    def load_csv(self, path: str, alias: Optional[str] = None) -> Optional[TableInfo]:
        """Load a CSV file as a table.

        Args:
            path: Path to the CSV file.
            alias: Optional table name. If not provided, derived from filename.

        Returns:
            TableInfo if successful, None if failed.
        """
        path = os.path.abspath(path)

        # Validate file exists
        if not os.path.exists(path):
            self._error_handler.handle_csv_load_error(
                FileNotFoundError(f"File not found: {path}"), path
            )
            return None

        if not os.path.isfile(path):
            self._error_handler.handle_csv_load_error(
                IsADirectoryError(f"Not a file: {path}"), path
            )
            return None

        # Derive table name from filename if not provided
        if alias is None:
            alias = self._sanitize_table_name(Path(path).stem)

        # Check for duplicate table name
        if alias in self._tables:
            # Unload existing table first
            self.unload_table(alias)

        try:
            # Use DuckDB's read_csv_auto for automatic type detection
            self._conn.execute(f"""
                CREATE TABLE "{alias}" AS
                SELECT * FROM read_csv_auto('{path}',
                    header=true,
                    sample_size=10000,
                    ignore_errors=false
                )
            """)

            # Get row count
            result = self._conn.execute(f'SELECT COUNT(*) FROM "{alias}"').fetchone()
            row_count = result[0] if result else 0

            # Get column info
            columns = self._get_table_columns(alias)

            table_info = TableInfo(
                name=alias,
                path=path,
                row_count=row_count,
                columns=columns
            )
            self._tables[alias] = table_info

            self._error_handler.success(
                f"Loaded '{alias}' ({row_count:,} rows, {len(columns)} columns)"
            )
            return table_info

        except Exception as e:
            self._error_handler.handle_csv_load_error(e, path)
            return None

    def unload_table(self, name: str) -> bool:
        """Remove a table from the database.

        Args:
            name: Table name to remove.

        Returns:
            True if successful, False if table not found.
        """
        if name not in self._tables:
            return False

        try:
            self._conn.execute(f'DROP TABLE IF EXISTS "{name}"')
            del self._tables[name]
            self._error_handler.info(f"Unloaded table '{name}'")
            return True
        except Exception as e:
            self._error_handler.handle_query_error(e, f"DROP TABLE {name}")
            return False

    def execute(self, sql: str, timeout_seconds: Optional[int] = None) -> QueryResult:
        """Execute a SQL query and return results.

        Args:
            sql: SQL query to execute.
            timeout_seconds: Optional query timeout.

        Returns:
            QueryResult with data or error information.
        """
        sql = sql.strip()
        if not sql:
            return QueryResult(
                success=False,
                data=None,
                row_count=0,
                execution_time_ms=0,
                error=self._error_handler.warning("Empty query")
            )

        start_time = time.perf_counter()

        try:
            result = self._conn.execute(sql)
            df = result.fetchdf()
            execution_time_ms = (time.perf_counter() - start_time) * 1000

            row_count = len(df)

            if row_count == 0:
                self._error_handler.warning(
                    "Query returned no results",
                    suggestion="Check your WHERE conditions or table contents."
                )

            return QueryResult(
                success=True,
                data=df,
                row_count=row_count,
                execution_time_ms=execution_time_ms
            )

        except Exception as e:
            execution_time_ms = (time.perf_counter() - start_time) * 1000
            error_result = self._error_handler.handle_query_error(e, sql)

            return QueryResult(
                success=False,
                data=None,
                row_count=0,
                execution_time_ms=execution_time_ms,
                error=error_result
            )

    def get_tables(self) -> List[TableInfo]:
        """Get list of all loaded tables.

        Returns:
            List of TableInfo for all loaded tables.
        """
        return list(self._tables.values())

    def get_table(self, name: str) -> Optional[TableInfo]:
        """Get info for a specific table.

        Args:
            name: Table name.

        Returns:
            TableInfo or None if not found.
        """
        return self._tables.get(name)

    def get_schema(self, table_name: str) -> List[Tuple[str, str]]:
        """Get column names and types for a table.

        Args:
            table_name: Name of the table.

        Returns:
            List of (column_name, column_type) tuples.
        """
        if table_name not in self._tables:
            return []
        return self._tables[table_name].columns

    def get_all_columns(self) -> Dict[str, List[Tuple[str, str]]]:
        """Get columns for all tables.

        Returns:
            Dict mapping table name to list of (column_name, column_type).
        """
        return {name: info.columns for name, info in self._tables.items()}

    def export_result(self, df: pd.DataFrame, path: str) -> bool:
        """Export a DataFrame to CSV.

        Args:
            df: DataFrame to export.
            path: Output path.

        Returns:
            True if successful, False otherwise.
        """
        try:
            df.to_csv(path, index=False)
            self._error_handler.success(f"Exported {len(df):,} rows to {path}")
            return True
        except Exception as e:
            self._error_handler.handle_export_error(e, path)
            return False

    def get_sample(self, table_name: str, limit: int = 10) -> Optional[pd.DataFrame]:
        """Get a sample of rows from a table.

        Args:
            table_name: Name of the table.
            limit: Number of rows to return.

        Returns:
            DataFrame with sample rows, or None if error.
        """
        result = self.execute(f'SELECT * FROM "{table_name}" LIMIT {limit}')
        return result.data if result.success else None

    def _get_table_columns(self, table_name: str) -> List[Tuple[str, str]]:
        """Get column information for a table.

        Args:
            table_name: Name of the table.

        Returns:
            List of (column_name, column_type) tuples.
        """
        try:
            result = self._conn.execute(f'DESCRIBE "{table_name}"').fetchall()
            return [(row[0], row[1]) for row in result]
        except Exception:
            return []

    def _sanitize_table_name(self, name: str) -> str:
        """Sanitize a string to be a valid table name.

        Args:
            name: Original name.

        Returns:
            Sanitized table name.
        """
        # Replace invalid characters with underscores
        sanitized = re.sub(r"[^a-zA-Z0-9_]", "_", name)
        # Ensure it doesn't start with a number
        if sanitized and sanitized[0].isdigit():
            sanitized = "_" + sanitized
        # Ensure it's not empty
        if not sanitized:
            sanitized = "table"
        return sanitized

    def close(self) -> None:
        """Close the database connection."""
        if self._conn:
            self._conn.close()
            self._conn = None
            self._tables.clear()

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
        return False
