"""Query editor component with history and keyboard shortcuts."""

import tkinter as tk
from tkinter import ttk
from typing import Callable, Optional, List

from ..utils.query_history import QueryHistory


class QueryEditor(ttk.Frame):
    """SQL query editor with run button and history."""

    def __init__(self, parent: tk.Widget, history: QueryHistory,
                 on_execute: Optional[Callable[[str], None]] = None):
        """Initialize query editor.

        Args:
            parent: Parent widget.
            history: Query history manager.
            on_execute: Callback when query should be executed.
        """
        super().__init__(parent)
        self._history = history
        self._on_execute = on_execute

        self._setup_ui()

    def _setup_ui(self) -> None:
        """Set up the UI components."""
        # Query label
        label = ttk.Label(self, text="SQL Query:")
        label.pack(anchor=tk.W, padx=5, pady=(5, 2))

        # Text editor frame
        editor_frame = ttk.Frame(self)
        editor_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)

        # Text widget with scrollbar
        self._text = tk.Text(
            editor_frame,
            wrap=tk.NONE,
            font=("Consolas", 11),
            height=8,
            undo=True
        )
        scroll_y = ttk.Scrollbar(editor_frame, orient=tk.VERTICAL,
                                 command=self._text.yview)
        scroll_x = ttk.Scrollbar(editor_frame, orient=tk.HORIZONTAL,
                                 command=self._text.xview)
        self._text.configure(yscrollcommand=scroll_y.set,
                             xscrollcommand=scroll_x.set)

        self._text.grid(row=0, column=0, sticky="nsew")
        scroll_y.grid(row=0, column=1, sticky="ns")
        scroll_x.grid(row=1, column=0, sticky="ew")

        editor_frame.grid_rowconfigure(0, weight=1)
        editor_frame.grid_columnconfigure(0, weight=1)

        # Bind keyboard shortcuts
        self._text.bind("<Control-Return>", self._on_run)
        self._text.bind("<F5>", self._on_run)
        self._text.bind("<Control-l>", self._on_clear)
        self._text.bind("<Control-L>", self._on_clear)

        # Button frame
        btn_frame = ttk.Frame(self)
        btn_frame.pack(fill=tk.X, padx=5, pady=5)

        # Run button
        self._run_btn = ttk.Button(
            btn_frame, text="Run (Ctrl+Enter)",
            command=self._execute_query
        )
        self._run_btn.pack(side=tk.LEFT, padx=2)

        # Clear button
        self._clear_btn = ttk.Button(
            btn_frame, text="Clear",
            command=self.clear
        )
        self._clear_btn.pack(side=tk.LEFT, padx=2)

        # History dropdown
        self._history_var = tk.StringVar(value="History")
        self._history_menu = ttk.Combobox(
            btn_frame, textvariable=self._history_var,
            state="readonly", width=40
        )
        self._history_menu.pack(side=tk.LEFT, padx=10)
        self._history_menu.bind("<<ComboboxSelected>>", self._on_history_selected)
        self._update_history_menu()

        # Shortcuts help
        help_label = ttk.Label(
            btn_frame,
            text="Ctrl+Enter: Run | Ctrl+L: Clear",
            foreground="gray"
        )
        help_label.pack(side=tk.RIGHT, padx=5)

    def _on_run(self, event=None) -> str:
        """Handle run keyboard shortcut.

        Args:
            event: Tkinter event.

        Returns:
            'break' to prevent default handling.
        """
        self._execute_query()
        return "break"

    def _on_clear(self, event=None) -> str:
        """Handle clear keyboard shortcut.

        Args:
            event: Tkinter event.

        Returns:
            'break' to prevent default handling.
        """
        self.clear()
        return "break"

    def _execute_query(self) -> None:
        """Execute the current query."""
        query = self.get_query()
        if query and self._on_execute:
            self._on_execute(query)

    def _update_history_menu(self) -> None:
        """Update the history dropdown."""
        queries = self._history.get_queries()
        if queries:
            # Truncate long queries for display
            display = [self._truncate_query(q) for q in queries]
            self._history_menu["values"] = display
        else:
            self._history_menu["values"] = ["(No history)"]

    def _truncate_query(self, query: str, max_len: int = 60) -> str:
        """Truncate a query for display.

        Args:
            query: Full query string.
            max_len: Maximum display length.

        Returns:
            Truncated query.
        """
        # Replace newlines with spaces
        query = " ".join(query.split())
        if len(query) > max_len:
            return query[:max_len - 3] + "..."
        return query

    def _on_history_selected(self, event) -> None:
        """Handle history selection."""
        selection = self._history_menu.current()
        queries = self._history.get_queries()
        if selection >= 0 and selection < len(queries):
            self.set_query(queries[selection])
        # Reset dropdown display
        self._history_var.set("History")

    def get_query(self) -> str:
        """Get the current query text.

        Returns:
            Query string.
        """
        return self._text.get("1.0", tk.END).strip()

    def set_query(self, query: str) -> None:
        """Set the query text.

        Args:
            query: Query string to set.
        """
        self._text.delete("1.0", tk.END)
        self._text.insert("1.0", query)

    def clear(self) -> None:
        """Clear the query text."""
        self._text.delete("1.0", tk.END)

    def append_query(self, text: str) -> None:
        """Append text to the query.

        Args:
            text: Text to append.
        """
        current = self.get_query()
        if current:
            self._text.insert(tk.END, "\n" + text)
        else:
            self._text.insert("1.0", text)

    def set_enabled(self, enabled: bool) -> None:
        """Enable or disable the editor.

        Args:
            enabled: Whether to enable.
        """
        state = tk.NORMAL if enabled else tk.DISABLED
        self._text.config(state=state)
        self._run_btn.config(state=state)

    def refresh_history(self) -> None:
        """Refresh the history dropdown."""
        self._update_history_menu()

    def focus_editor(self) -> None:
        """Set focus to the text editor."""
        self._text.focus_set()

    def insert_table_query(self, table_name: str) -> None:
        """Insert a basic SELECT query for a table.

        Args:
            table_name: Name of the table.
        """
        query = f'SELECT * FROM "{table_name}" LIMIT 100'
        self.set_query(query)
