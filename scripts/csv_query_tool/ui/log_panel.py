"""Log panel for displaying application logs in the GUI."""

import tkinter as tk
from tkinter import ttk, filedialog
import logging
import sys
from datetime import datetime
from typing import Optional, TextIO
from io import StringIO


class LogPanel(ttk.Frame):
    """Panel for displaying log messages with color coding."""

    # Color scheme for different log levels
    COLORS = {
        "DEBUG": "#808080",      # Gray
        "INFO": "#000000",       # Black
        "WARNING": "#CC6600",    # Orange
        "ERROR": "#CC0000",      # Red
        "CRITICAL": "#990000",   # Dark red
        "SUCCESS": "#006600",    # Green
        "STDOUT": "#000066",     # Dark blue
        "STDERR": "#660000",     # Dark red
    }

    def __init__(self, parent: tk.Widget):
        """Initialize log panel.

        Args:
            parent: Parent widget.
        """
        super().__init__(parent)
        self._setup_ui()
        self._setup_tags()

    def _setup_ui(self) -> None:
        """Set up the UI components."""
        # Toolbar
        toolbar = ttk.Frame(self)
        toolbar.pack(fill=tk.X, padx=5, pady=2)

        ttk.Button(toolbar, text="Clear", command=self.clear).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="Copy All", command=self._copy_all).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="Save...", command=self._save_log).pack(side=tk.LEFT, padx=2)

        # Auto-scroll checkbox
        self._auto_scroll = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            toolbar, text="Auto-scroll",
            variable=self._auto_scroll
        ).pack(side=tk.RIGHT, padx=5)

        # Log text area
        text_frame = ttk.Frame(self)
        text_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)

        self._text = tk.Text(
            text_frame,
            wrap=tk.WORD,
            font=("Consolas", 9),
            state=tk.DISABLED,
            background="#FAFAFA"
        )

        scroll_y = ttk.Scrollbar(text_frame, orient=tk.VERTICAL,
                                  command=self._text.yview)
        self._text.configure(yscrollcommand=scroll_y.set)

        self._text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scroll_y.pack(side=tk.RIGHT, fill=tk.Y)

    def _setup_tags(self) -> None:
        """Set up text tags for color coding."""
        for level, color in self.COLORS.items():
            self._text.tag_configure(level, foreground=color)

        # Bold tag for timestamps
        self._text.tag_configure("timestamp", foreground="#666666")

    def log(self, message: str, level: str = "INFO") -> None:
        """Add a log message.

        Args:
            message: The message to log.
            level: Log level (DEBUG, INFO, WARNING, ERROR, CRITICAL, SUCCESS).
        """
        timestamp = datetime.now().strftime("%H:%M:%S")

        self._text.configure(state=tk.NORMAL)

        # Insert timestamp
        self._text.insert(tk.END, f"[{timestamp}] ", "timestamp")

        # Insert level tag
        level_upper = level.upper()
        if level_upper not in ["STDOUT", "STDERR"]:
            self._text.insert(tk.END, f"[{level_upper}] ", level_upper)

        # Insert message
        self._text.insert(tk.END, f"{message}\n", level_upper)

        self._text.configure(state=tk.DISABLED)

        # Auto-scroll to bottom
        if self._auto_scroll.get():
            self._text.see(tk.END)

    def log_stdout(self, message: str) -> None:
        """Log a stdout message (without level prefix).

        Args:
            message: The message from stdout.
        """
        if message.strip():  # Skip empty lines
            self.log(message.rstrip(), "STDOUT")

    def log_stderr(self, message: str) -> None:
        """Log a stderr message (without level prefix).

        Args:
            message: The message from stderr.
        """
        if message.strip():  # Skip empty lines
            self.log(message.rstrip(), "STDERR")

    def clear(self) -> None:
        """Clear all log messages."""
        self._text.configure(state=tk.NORMAL)
        self._text.delete("1.0", tk.END)
        self._text.configure(state=tk.DISABLED)

    def _copy_all(self) -> None:
        """Copy all log content to clipboard."""
        content = self._text.get("1.0", tk.END)
        self.clipboard_clear()
        self.clipboard_append(content)

    def _save_log(self) -> None:
        """Save log content to a file."""
        path = filedialog.asksaveasfilename(
            title="Save Log",
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        if path:
            content = self._text.get("1.0", tk.END)
            with open(path, "w", encoding="utf-8") as f:
                f.write(content)


class GUILogHandler(logging.Handler):
    """Custom logging handler that sends logs to a LogPanel."""

    def __init__(self, log_panel: LogPanel):
        """Initialize handler.

        Args:
            log_panel: The LogPanel to send logs to.
        """
        super().__init__()
        self._log_panel = log_panel

        # Set format
        self.setFormatter(logging.Formatter("%(message)s"))

    def emit(self, record: logging.LogRecord) -> None:
        """Emit a log record.

        Args:
            record: The log record to emit.
        """
        try:
            msg = self.format(record)
            level = record.levelname

            # Map SUCCESS level (custom)
            if hasattr(record, 'success') and record.success:
                level = "SUCCESS"

            # Use after() to ensure thread safety with tkinter
            self._log_panel.after(0, lambda: self._log_panel.log(msg, level))

        except Exception:
            self.handleError(record)


class TextRedirector:
    """Redirects stdout/stderr to a LogPanel."""

    def __init__(self, log_panel: LogPanel, stream_type: str = "stdout",
                 original_stream: Optional[TextIO] = None):
        """Initialize redirector.

        Args:
            log_panel: The LogPanel to redirect to.
            stream_type: Either "stdout" or "stderr".
            original_stream: The original stream to also write to (optional).
        """
        self._log_panel = log_panel
        self._stream_type = stream_type
        self._original = original_stream
        self._buffer = StringIO()

    def write(self, message: str) -> None:
        """Write a message.

        Args:
            message: The message to write.
        """
        # Write to original stream if available (for debugging)
        if self._original:
            try:
                self._original.write(message)
                self._original.flush()
            except Exception:
                pass

        # Skip empty writes
        if not message or message == "\n":
            return

        # Log to panel (thread-safe)
        if self._stream_type == "stderr":
            self._log_panel.after(0, lambda m=message: self._log_panel.log_stderr(m))
        else:
            self._log_panel.after(0, lambda m=message: self._log_panel.log_stdout(m))

    def flush(self) -> None:
        """Flush the stream."""
        if self._original:
            try:
                self._original.flush()
            except Exception:
                pass

    def isatty(self) -> bool:
        """Check if stream is a tty."""
        return False


def setup_gui_logging(log_panel: LogPanel, capture_stdout: bool = True,
                      capture_stderr: bool = True) -> None:
    """Set up GUI logging with optional stdout/stderr capture.

    Args:
        log_panel: The LogPanel to use.
        capture_stdout: Whether to capture stdout.
        capture_stderr: Whether to capture stderr.
    """
    # Set up logging handler
    handler = GUILogHandler(log_panel)
    handler.setLevel(logging.DEBUG)

    # Get root logger and add handler
    root_logger = logging.getLogger()
    root_logger.addHandler(handler)
    root_logger.setLevel(logging.DEBUG)

    # Remove console handlers to avoid duplicate output
    for h in root_logger.handlers[:]:
        if isinstance(h, logging.StreamHandler) and h.stream in (sys.stdout, sys.stderr):
            root_logger.removeHandler(h)

    # Redirect stdout/stderr
    if capture_stdout:
        sys.stdout = TextRedirector(log_panel, "stdout", sys.__stdout__)

    if capture_stderr:
        sys.stderr = TextRedirector(log_panel, "stderr", sys.__stderr__)

    # Log startup message
    log_panel.log("Application started", "SUCCESS")
