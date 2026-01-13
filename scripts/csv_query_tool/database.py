"""Database manager using DuckDB for CSV querying."""

import os
import re
import time
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any, Callable

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
class IndexInfo:
    """Information about an index."""
    name: str
    table_name: str
    columns: List[str]
    is_unique: bool


@dataclass
class QueryResult:
    """Result of a query execution."""
    success: bool
    data: Optional[pd.DataFrame]
    row_count: int
    execution_time_ms: float
    error: Optional[ErrorResult] = None
    timed_out: bool = False
    cancelled: bool = False


class DatabaseManager:
    """Manages DuckDB connection and CSV table loading."""

    def __init__(self, error_handler: Optional[ErrorHandler] = None,
                 db_path: Optional[str] = None):
        """Initialize database manager.

        Args:
            error_handler: Optional error handler for user-friendly messages.
            db_path: Path to persistent DuckDB file, or None for in-memory.
        """
        self._db_path = db_path
        self._error_handler = error_handler or ErrorHandler()
        self._cancel_requested = False
        self._query_running = False
        self._tables: Dict[str, TableInfo] = {}

        # Connect to database
        if db_path:
            self._conn = duckdb.connect(db_path)
            self._refresh_tables_from_db()
        else:
            self._conn = duckdb.connect(":memory:")

        # Configure DuckDB for performance
        self._conn.execute("SET threads TO 4")

    def connect_to_database(self, db_path: str) -> bool:
        """Connect to an existing DuckDB database file.

        Args:
            db_path: Path to the .duckdb file.

        Returns:
            True if successful.
        """
        try:
            # Close existing connection
            if self._conn:
                self._conn.close()

            self._db_path = db_path
            self._conn = duckdb.connect(db_path)
            self._conn.execute("SET threads TO 4")
            self._refresh_tables_from_db()

            self._error_handler.success(
                f"Connected to database: {Path(db_path).name}"
            )
            return True

        except Exception as e:
            self._error_handler.handle_csv_load_error(e, db_path)
            # Reconnect to in-memory
            self._conn = duckdb.connect(":memory:")
            self._db_path = None
            return False

    def create_database(self, db_path: str) -> bool:
        """Create a new persistent DuckDB database.

        Args:
            db_path: Path for the new .duckdb file.

        Returns:
            True if successful.
        """
        try:
            # Close existing connection
            if self._conn:
                self._conn.close()

            self._db_path = db_path
            self._conn = duckdb.connect(db_path)
            self._conn.execute("SET threads TO 4")
            self._tables.clear()

            self._error_handler.success(
                f"Created database: {Path(db_path).name}"
            )
            return True

        except Exception as e:
            self._error_handler.handle_csv_load_error(e, db_path)
            self._conn = duckdb.connect(":memory:")
            self._db_path = None
            return False

    def import_csv_to_table(self, csv_path: str, table_name: Optional[str] = None,
                            progress_callback: Optional[Callable[[str], None]] = None) -> Optional[TableInfo]:
        """Import a CSV file into a permanent table in the database.

        Args:
            csv_path: Path to the CSV file.
            table_name: Table name (derived from filename if not provided).
            progress_callback: Optional callback for progress updates.

        Returns:
            TableInfo if successful, None if failed.
        """
        csv_path = os.path.abspath(csv_path)

        if not os.path.exists(csv_path):
            self._error_handler.handle_csv_load_error(
                FileNotFoundError(f"File not found: {csv_path}"), csv_path
            )
            return None

        if table_name is None:
            table_name = self._sanitize_table_name(Path(csv_path).stem)

        # Drop existing table if it exists
        try:
            self._conn.execute(f'DROP TABLE IF EXISTS "{table_name}"')
        except Exception:
            pass

        try:
            if progress_callback:
                progress_callback(f"Loading {Path(csv_path).name}...")

            # Create table from CSV
            self._conn.execute(f"""
                CREATE TABLE "{table_name}" AS
                SELECT * FROM read_csv_auto('{csv_path}',
                    header=true,
                    sample_size=100000,
                    ignore_errors=false
                )
            """)

            # Get row count
            result = self._conn.execute(f'SELECT COUNT(*) FROM "{table_name}"').fetchone()
            row_count = result[0] if result else 0

            # Get column info
            columns = self._get_table_columns(table_name)

            table_info = TableInfo(
                name=table_name,
                path=csv_path,
                row_count=row_count,
                columns=columns
            )
            self._tables[table_name] = table_info

            if progress_callback:
                progress_callback(f"Loaded {table_name}: {row_count:,} rows")

            self._error_handler.success(
                f"Imported '{table_name}' ({row_count:,} rows, {len(columns)} columns)"
            )
            return table_info

        except Exception as e:
            self._error_handler.handle_csv_load_error(e, csv_path)
            return None

    def import_multiple_csvs(self, csv_paths: List[str],
                             progress_callback: Optional[Callable[[str, int, int], None]] = None) -> List[TableInfo]:
        """Import multiple CSV files into the database.

        Args:
            csv_paths: List of CSV file paths.
            progress_callback: Callback(message, current, total) for progress.

        Returns:
            List of successfully imported TableInfo objects.
        """
        results = []
        total = len(csv_paths)

        for i, path in enumerate(csv_paths):
            if progress_callback:
                progress_callback(f"Importing {Path(path).name}...", i + 1, total)

            table_info = self.import_csv_to_table(path)
            if table_info:
                results.append(table_info)

        return results

    def create_index(self, table_name: str, column_names: List[str],
                     index_name: Optional[str] = None, unique: bool = False) -> bool:
        """Create an index on a table.

        Args:
            table_name: Name of the table.
            column_names: List of column names to index.
            index_name: Optional custom index name.
            unique: Whether to create a unique index.

        Returns:
            True if successful.
        """
        if not index_name:
            cols_str = "_".join(column_names)
            index_name = f"idx_{table_name}_{cols_str}"

        try:
            unique_str = "UNIQUE " if unique else ""
            columns_str = ", ".join(f'"{c}"' for c in column_names)

            self._conn.execute(f"""
                CREATE {unique_str}INDEX IF NOT EXISTS "{index_name}"
                ON "{table_name}" ({columns_str})
            """)

            self._error_handler.success(
                f"Created index '{index_name}' on {table_name}({', '.join(column_names)})"
            )
            return True

        except Exception as e:
            self._error_handler.handle_query_error(
                e, f"CREATE INDEX on {table_name}"
            )
            return False

    def get_indexes(self, table_name: Optional[str] = None) -> List[IndexInfo]:
        """Get list of indexes.

        Args:
            table_name: Optional table name to filter by.

        Returns:
            List of IndexInfo objects.
        """
        try:
            query = "SELECT * FROM duckdb_indexes()"
            if table_name:
                query += f" WHERE table_name = '{table_name}'"

            result = self._conn.execute(query).fetchall()
            indexes = []
            for row in result:
                indexes.append(IndexInfo(
                    name=row[1],  # index_name
                    table_name=row[2],  # table_name
                    columns=[row[4]] if row[4] else [],  # Simplified
                    is_unique=row[3] if len(row) > 3 else False
                ))
            return indexes
        except Exception:
            return []

    def drop_index(self, index_name: str) -> bool:
        """Drop an index.

        Args:
            index_name: Name of the index to drop.

        Returns:
            True if successful.
        """
        try:
            self._conn.execute(f'DROP INDEX IF EXISTS "{index_name}"')
            self._error_handler.success(f"Dropped index '{index_name}'")
            return True
        except Exception as e:
            self._error_handler.handle_query_error(e, f"DROP INDEX {index_name}")
            return False

    def _refresh_tables_from_db(self) -> None:
        """Refresh table list from the database."""
        # Keep track of existing paths so we don't lose CSV source info
        existing_paths = {name: info.path for name, info in self._tables.items()}
        self._tables.clear()

        try:
            # Get all tables from DuckDB
            result = self._conn.execute("""
                SELECT table_name
                FROM information_schema.tables
                WHERE table_schema = 'main'
                  AND table_type = 'BASE TABLE'
            """).fetchall()

            for (table_name,) in result:
                # Get row count
                count_result = self._conn.execute(
                    f'SELECT COUNT(*) FROM "{table_name}"'
                ).fetchone()
                row_count = count_result[0] if count_result else 0

                # Get columns
                columns = self._get_table_columns(table_name)

                # Preserve original path if available
                path = existing_paths.get(table_name, "(database)")

                self._tables[table_name] = TableInfo(
                    name=table_name,
                    path=path,
                    row_count=row_count,
                    columns=columns
                )

        except Exception as e:
            self._error_handler.handle_query_error(e, "refresh tables")

    def refresh_tables(self) -> None:
        """Public method to refresh the table list from the database.

        Call this after DDL operations that create/drop tables.
        """
        self._refresh_tables_from_db()

    def load_csv(self, path: str, alias: Optional[str] = None) -> Optional[TableInfo]:
        """Load a CSV file as a table (legacy method, uses import_csv_to_table).

        Args:
            path: Path to the CSV file.
            alias: Optional table name.

        Returns:
            TableInfo if successful, None if failed.
        """
        return self.import_csv_to_table(path, alias)

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
            self._error_handler.info(f"Dropped table '{name}'")
            return True
        except Exception as e:
            self._error_handler.handle_query_error(e, f"DROP TABLE {name}")
            return False

    def execute(self, sql: str, timeout_seconds: Optional[int] = None) -> QueryResult:
        """Execute a SQL query and return results.

        Args:
            sql: SQL query to execute.
            timeout_seconds: Optional query timeout in seconds.

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

        self._cancel_requested = False
        self._query_running = True
        start_time = time.perf_counter()

        try:
            # If timeout specified, run with timeout
            if timeout_seconds and timeout_seconds > 0:
                result_container = {"result": None, "error": None}

                def run_query():
                    try:
                        result_container["result"] = self._conn.execute(sql)
                    except Exception as e:
                        result_container["error"] = e

                thread = threading.Thread(target=run_query)
                thread.start()
                thread.join(timeout=timeout_seconds)

                if thread.is_alive():
                    # Query timed out
                    self._query_running = False
                    execution_time_ms = (time.perf_counter() - start_time) * 1000
                    self._error_handler.warning(
                        f"Query timed out after {timeout_seconds}s",
                        suggestion="Try adding LIMIT or simplifying the query."
                    )
                    return QueryResult(
                        success=False,
                        data=None,
                        row_count=0,
                        execution_time_ms=execution_time_ms,
                        timed_out=True
                    )

                if result_container["error"]:
                    raise result_container["error"]

                result = result_container["result"]
            else:
                result = self._conn.execute(sql)

            # Check if cancelled
            if self._cancel_requested:
                self._query_running = False
                execution_time_ms = (time.perf_counter() - start_time) * 1000
                self._error_handler.info("Query cancelled")
                return QueryResult(
                    success=False,
                    data=None,
                    row_count=0,
                    execution_time_ms=execution_time_ms,
                    cancelled=True
                )

            df = result.fetchdf()
            execution_time_ms = (time.perf_counter() - start_time) * 1000
            self._query_running = False

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
            self._query_running = False
            error_result = self._error_handler.handle_query_error(e, sql)

            return QueryResult(
                success=False,
                data=None,
                row_count=0,
                execution_time_ms=execution_time_ms,
                error=error_result
            )

    def execute_non_query(self, sql: str) -> bool:
        """Execute a SQL statement that doesn't return results (DDL, etc).

        Args:
            sql: SQL statement to execute.

        Returns:
            True if successful.
        """
        try:
            self._conn.execute(sql)
            return True
        except Exception as e:
            self._error_handler.handle_query_error(e, sql)
            return False

    def cancel_query(self) -> None:
        """Request cancellation of the running query."""
        if self._query_running:
            self._cancel_requested = True
            try:
                self._conn.interrupt()
            except Exception:
                pass

    def is_query_running(self) -> bool:
        """Check if a query is currently running."""
        return self._query_running

    def get_database_path(self) -> Optional[str]:
        """Get the current database file path.

        Returns:
            Path to database file, or None if in-memory.
        """
        return self._db_path

    def is_persistent(self) -> bool:
        """Check if using a persistent database.

        Returns:
            True if using a file-based database.
        """
        return self._db_path is not None

    def get_tables(self) -> List[TableInfo]:
        """Get list of all loaded tables."""
        return list(self._tables.values())

    def get_table(self, name: str) -> Optional[TableInfo]:
        """Get info for a specific table."""
        return self._tables.get(name)

    def get_schema(self, table_name: str) -> List[Tuple[str, str]]:
        """Get column names and types for a table."""
        if table_name not in self._tables:
            return []
        return self._tables[table_name].columns

    def get_all_columns(self) -> Dict[str, List[Tuple[str, str]]]:
        """Get columns for all tables."""
        return {name: info.columns for name, info in self._tables.items()}

    def export_result(self, df: pd.DataFrame, path: str) -> bool:
        """Export a DataFrame to CSV."""
        try:
            df.to_csv(path, index=False)
            self._error_handler.success(f"Exported {len(df):,} rows to {path}")
            return True
        except Exception as e:
            self._error_handler.handle_export_error(e, path)
            return False

    def export_table_to_parquet(self, table_name: str, path: str) -> bool:
        """Export a table to Parquet format.

        Args:
            table_name: Name of the table to export.
            path: Output path for Parquet file.

        Returns:
            True if successful.
        """
        try:
            self._conn.execute(f"""
                COPY "{table_name}" TO '{path}' (FORMAT PARQUET)
            """)
            self._error_handler.success(f"Exported {table_name} to {path}")
            return True
        except Exception as e:
            self._error_handler.handle_export_error(e, path)
            return False

    def import_parquet(self, path: str, table_name: Optional[str] = None) -> Optional[TableInfo]:
        """Import a Parquet file as a table.

        Args:
            path: Path to the Parquet file.
            table_name: Optional table name.

        Returns:
            TableInfo if successful.
        """
        path = os.path.abspath(path)

        if not os.path.exists(path):
            self._error_handler.handle_csv_load_error(
                FileNotFoundError(f"File not found: {path}"), path
            )
            return None

        if table_name is None:
            table_name = self._sanitize_table_name(Path(path).stem)

        try:
            self._conn.execute(f'DROP TABLE IF EXISTS "{table_name}"')
            self._conn.execute(f"""
                CREATE TABLE "{table_name}" AS
                SELECT * FROM read_parquet('{path}')
            """)

            result = self._conn.execute(f'SELECT COUNT(*) FROM "{table_name}"').fetchone()
            row_count = result[0] if result else 0

            columns = self._get_table_columns(table_name)

            table_info = TableInfo(
                name=table_name,
                path=path,
                row_count=row_count,
                columns=columns
            )
            self._tables[table_name] = table_info

            self._error_handler.success(
                f"Imported '{table_name}' ({row_count:,} rows) from Parquet"
            )
            return table_info

        except Exception as e:
            self._error_handler.handle_csv_load_error(e, path)
            return None

    def get_sample(self, table_name: str, limit: int = 10) -> Optional[pd.DataFrame]:
        """Get a sample of rows from a table."""
        result = self.execute(f'SELECT * FROM "{table_name}" LIMIT {limit}')
        return result.data if result.success else None

    def get_database_size(self) -> Optional[int]:
        """Get the size of the database file in bytes.

        Returns:
            Size in bytes, or None if in-memory.
        """
        if self._db_path and os.path.exists(self._db_path):
            return os.path.getsize(self._db_path)
        return None

    def vacuum(self) -> bool:
        """Compact the database file.

        Returns:
            True if successful.
        """
        try:
            self._conn.execute("VACUUM")
            self._error_handler.success("Database compacted")
            return True
        except Exception as e:
            self._error_handler.handle_query_error(e, "VACUUM")
            return False

    def _get_table_columns(self, table_name: str) -> List[Tuple[str, str]]:
        """Get column information for a table."""
        try:
            result = self._conn.execute(f'DESCRIBE "{table_name}"').fetchall()
            return [(row[0], row[1]) for row in result]
        except Exception:
            return []

    def _sanitize_table_name(self, name: str) -> str:
        """Sanitize a string to be a valid table name."""
        sanitized = re.sub(r"[^a-zA-Z0-9_]", "_", name)
        if sanitized and sanitized[0].isdigit():
            sanitized = "_" + sanitized
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
