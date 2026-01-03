"""Centralized error handling for CSV Query Tool."""

import logging
import traceback
from dataclasses import dataclass
from enum import Enum
from typing import Optional, Callable


class ErrorSeverity(Enum):
    """Severity levels for errors and messages."""
    SUCCESS = "success"
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"


@dataclass
class ErrorResult:
    """Result of an error handling operation."""
    severity: ErrorSeverity
    message: str
    details: Optional[str] = None
    suggestion: Optional[str] = None


class CSVLoadError(Exception):
    """Exception raised when CSV loading fails."""

    def __init__(self, message: str, path: str, details: Optional[str] = None):
        self.message = message
        self.path = path
        self.details = details
        super().__init__(message)


class QueryError(Exception):
    """Exception raised when query execution fails."""

    def __init__(self, message: str, query: str, details: Optional[str] = None):
        self.message = message
        self.query = query
        self.details = details
        super().__init__(message)


class ErrorHandler:
    """Centralized error handler with user-friendly messages."""

    def __init__(self, logger: Optional[logging.Logger] = None):
        """Initialize error handler.

        Args:
            logger: Optional logger for error logging.
        """
        self.logger = logger or logging.getLogger(__name__)
        self._status_callback: Optional[Callable[[ErrorResult], None]] = None

    def set_status_callback(self, callback: Callable[[ErrorResult], None]) -> None:
        """Set callback for status updates.

        Args:
            callback: Function to call with ErrorResult.
        """
        self._status_callback = callback

    def _notify(self, result: ErrorResult) -> None:
        """Notify via callback if set."""
        if self._status_callback:
            self._status_callback(result)

    def handle_csv_load_error(self, error: Exception, path: str) -> ErrorResult:
        """Handle CSV loading errors with user-friendly messages.

        Args:
            error: The exception that occurred.
            path: Path to the CSV file.

        Returns:
            ErrorResult with user-friendly message.
        """
        self.logger.error(f"CSV load error for {path}: {error}")
        self.logger.debug(traceback.format_exc())

        if isinstance(error, FileNotFoundError):
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message=f"File not found: {path}",
                details=str(error),
                suggestion="Check that the file path is correct and the file exists."
            )
        elif isinstance(error, PermissionError):
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message=f"Permission denied: {path}",
                details=str(error),
                suggestion="Check file permissions or run with appropriate access."
            )
        elif isinstance(error, IsADirectoryError):
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message=f"Path is a directory, not a file: {path}",
                details=str(error),
                suggestion="Select a CSV file, not a directory."
            )
        elif "Invalid CSV" in str(error) or "parse" in str(error).lower():
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message=f"Invalid CSV format: {path}",
                details=str(error),
                suggestion="Ensure the file is a valid CSV with consistent columns."
            )
        elif isinstance(error, MemoryError):
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="File too large to load into memory",
                details=str(error),
                suggestion="Try loading a smaller file or increase available memory."
            )
        else:
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message=f"Failed to load CSV: {path}",
                details=str(error),
                suggestion="Check the file format and try again."
            )

        self._notify(result)
        return result

    def handle_query_error(self, error: Exception, query: str) -> ErrorResult:
        """Handle query execution errors with user-friendly messages.

        Args:
            error: The exception that occurred.
            query: The SQL query that failed.

        Returns:
            ErrorResult with user-friendly message.
        """
        self.logger.error(f"Query error: {error}")
        self.logger.debug(f"Failed query: {query}")
        self.logger.debug(traceback.format_exc())

        error_str = str(error)
        error_type = type(error).__name__

        # DuckDB-specific error handling
        if "Parser" in error_type or "Syntax" in error_str.lower():
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="SQL syntax error",
                details=error_str,
                suggestion="Check your SQL syntax. Common issues: missing quotes, commas, or keywords."
            )
        elif "Catalog" in error_type or "does not exist" in error_str.lower():
            # Extract table/column name if possible
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="Unknown table or column",
                details=error_str,
                suggestion="Check the Schema tab for available tables and columns."
            )
        elif "Binder" in error_type or "type" in error_str.lower():
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="Type mismatch in query",
                details=error_str,
                suggestion="Check column types in Schema tab. Use CAST() to convert types."
            )
        elif "Conversion" in error_type:
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="Data conversion error",
                details=error_str,
                suggestion="A value couldn't be converted. Check data types and use CAST()."
            )
        elif isinstance(error, MemoryError) or "memory" in error_str.lower():
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="Query result too large",
                details=error_str,
                suggestion="Add a LIMIT clause to reduce result size (e.g., LIMIT 1000)."
            )
        elif "timeout" in error_str.lower():
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="Query timed out",
                details=error_str,
                suggestion="Simplify your query or add filters to reduce data scanned."
            )
        elif "interrupt" in error_str.lower():
            result = ErrorResult(
                severity=ErrorSeverity.WARNING,
                message="Query was cancelled",
                details=None,
                suggestion=None
            )
        else:
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="Query execution failed",
                details=error_str,
                suggestion="Check the query syntax and table/column names."
            )

        self._notify(result)
        return result

    def handle_export_error(self, error: Exception, path: str) -> ErrorResult:
        """Handle export errors with user-friendly messages.

        Args:
            error: The exception that occurred.
            path: Path where export was attempted.

        Returns:
            ErrorResult with user-friendly message.
        """
        self.logger.error(f"Export error to {path}: {error}")

        if isinstance(error, PermissionError):
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message=f"Cannot write to: {path}",
                details=str(error),
                suggestion="Choose a different location or check write permissions."
            )
        elif isinstance(error, IsADirectoryError):
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message="Cannot export to a directory",
                details=str(error),
                suggestion="Specify a file name, not a directory."
            )
        else:
            result = ErrorResult(
                severity=ErrorSeverity.ERROR,
                message=f"Export failed: {path}",
                details=str(error),
                suggestion="Check the path and try again."
            )

        self._notify(result)
        return result

    def success(self, message: str, details: Optional[str] = None) -> ErrorResult:
        """Create a success result.

        Args:
            message: Success message.
            details: Optional details.

        Returns:
            ErrorResult with success severity.
        """
        result = ErrorResult(
            severity=ErrorSeverity.SUCCESS,
            message=message,
            details=details
        )
        self._notify(result)
        return result

    def warning(self, message: str, details: Optional[str] = None,
                suggestion: Optional[str] = None) -> ErrorResult:
        """Create a warning result.

        Args:
            message: Warning message.
            details: Optional details.
            suggestion: Optional suggestion.

        Returns:
            ErrorResult with warning severity.
        """
        result = ErrorResult(
            severity=ErrorSeverity.WARNING,
            message=message,
            details=details,
            suggestion=suggestion
        )
        self._notify(result)
        return result

    def info(self, message: str, details: Optional[str] = None) -> ErrorResult:
        """Create an info result.

        Args:
            message: Info message.
            details: Optional details.

        Returns:
            ErrorResult with info severity.
        """
        result = ErrorResult(
            severity=ErrorSeverity.INFO,
            message=message,
            details=details
        )
        self._notify(result)
        return result
