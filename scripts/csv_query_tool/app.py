"""Main application class for CSV Query Tool."""

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import logging
import sys
from pathlib import Path
from typing import Optional

from .database import DatabaseManager
from .utils.config import Config
from .utils.query_history import QueryHistory
from .utils.favorites import QueryFavorites
from .utils.error_handler import ErrorHandler, ErrorSeverity, ErrorResult
from .ui.file_panel import FilePanel
from .ui.query_editor import QueryEditor
from .ui.results_table import ResultsTable
from .ui.chart_panel import ChartPanel
from .ui.schema_panel import SchemaPanel


class CSVQueryApp:
    """Main application window for CSV Query Tool."""

    def __init__(self, initial_file: Optional[str] = None):
        """Initialize the application.

        Args:
            initial_file: Optional CSV file to load on startup.
        """
        # Set up logging
        logging.basicConfig(
            level=logging.INFO,
            format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
        )
        self._logger = logging.getLogger(__name__)

        # Load configuration
        self._config = Config.load()
        self._history = QueryHistory(max_size=self._config.max_history_size)
        self._favorites = QueryFavorites()

        # Create error handler
        self._error_handler = ErrorHandler(self._logger)
        self._error_handler.set_status_callback(self._on_status_update)

        # Create database manager
        self._db = DatabaseManager(self._error_handler)

        # Create main window (must be before tk.IntVar)
        self._root = tk.Tk()
        self._root.title("CSV Query Tool")
        self._root.geometry(self._config.get_geometry())

        # Query settings (must be after tk.Tk())
        self._query_timeout = tk.IntVar(value=self._config.query_timeout_seconds)

        # Set minimum window size
        self._root.minsize(800, 600)

        # Set up UI
        self._setup_ui()

        # Set up bindings
        self._root.protocol("WM_DELETE_WINDOW", self._on_close)
        self._root.bind("<Control-o>", lambda e: self._file_panel._load_csv())
        self._root.bind("<Control-q>", lambda e: self._on_close())
        self._root.bind("<Escape>", lambda e: self._cancel_query())

        # Load initial file if provided
        if initial_file:
            self._load_initial_file(initial_file)

    def _setup_ui(self) -> None:
        """Set up the main UI layout."""
        # Create main paned window
        main_paned = ttk.PanedWindow(self._root, orient=tk.HORIZONTAL)
        main_paned.pack(fill=tk.BOTH, expand=True)

        # Left panel (files and schema)
        left_frame = ttk.Frame(main_paned, width=280)
        main_paned.add(left_frame, weight=0)

        # File panel
        file_frame = ttk.LabelFrame(left_frame, text="Files")
        file_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self._file_panel = FilePanel(
            file_frame, self._db, self._config,
            on_table_change=self._on_table_change
        )
        self._file_panel.pack(fill=tk.BOTH, expand=True)

        # Right panel (editor and results)
        right_frame = ttk.Frame(main_paned)
        main_paned.add(right_frame, weight=1)

        # Vertical paned window for editor and results - RESIZABLE
        self._right_paned = ttk.PanedWindow(right_frame, orient=tk.VERTICAL)
        self._right_paned.pack(fill=tk.BOTH, expand=True)

        # Query editor frame
        editor_frame = ttk.LabelFrame(self._right_paned, text="Query")
        self._right_paned.add(editor_frame, weight=1)  # weight=1 makes it resizable

        self._query_editor = QueryEditor(
            editor_frame, self._history, self._favorites,
            on_execute=self._execute_query
        )
        self._query_editor.pack(fill=tk.BOTH, expand=True)

        # Status bar frame (between editor and results)
        status_container = ttk.Frame(self._right_paned)
        self._right_paned.add(status_container, weight=0)

        self._status_frame = ttk.Frame(status_container)
        self._status_frame.pack(fill=tk.X, padx=5, pady=2)

        # Status label
        self._status_label = ttk.Label(
            self._status_frame, text="Ready",
            relief=tk.SUNKEN, padding=(5, 2)
        )
        self._status_label.pack(side=tk.LEFT, fill=tk.X, expand=True)

        # Timeout control in status bar
        timeout_frame = ttk.Frame(self._status_frame)
        timeout_frame.pack(side=tk.RIGHT, padx=5)

        ttk.Label(timeout_frame, text="Timeout:").pack(side=tk.LEFT, padx=2)
        self._timeout_spin = ttk.Spinbox(
            timeout_frame, from_=0, to=300,
            textvariable=self._query_timeout,
            width=5
        )
        self._timeout_spin.pack(side=tk.LEFT, padx=2)
        ttk.Label(timeout_frame, text="sec (0=off)").pack(side=tk.LEFT)

        # Cancel button (hidden until query runs)
        self._cancel_btn = ttk.Button(
            self._status_frame, text="Cancel",
            command=self._cancel_query
        )

        # Results notebook (tabbed)
        results_frame = ttk.Frame(self._right_paned)
        self._right_paned.add(results_frame, weight=3)  # Larger weight for results

        self._notebook = ttk.Notebook(results_frame)
        self._notebook.pack(fill=tk.BOTH, expand=True)

        # Results table tab
        table_frame = ttk.Frame(self._notebook)
        self._notebook.add(table_frame, text="Results")

        self._results_table = ResultsTable(table_frame, self._config)
        self._results_table.pack(fill=tk.BOTH, expand=True)

        # Chart tab
        chart_frame = ttk.Frame(self._notebook)
        self._notebook.add(chart_frame, text="Chart")

        self._chart_panel = ChartPanel(chart_frame)
        self._chart_panel.pack(fill=tk.BOTH, expand=True)

        # Schema tab
        schema_frame = ttk.Frame(self._notebook)
        self._notebook.add(schema_frame, text="Schema")

        self._schema_panel = SchemaPanel(schema_frame, self._db)
        self._schema_panel.pack(fill=tk.BOTH, expand=True)
        self._schema_panel.set_insert_callback(self._insert_query_text)

        # Set up menu
        self._setup_menu()

    def _setup_menu(self) -> None:
        """Set up the menu bar."""
        menubar = tk.Menu(self._root)
        self._root.config(menu=menubar)

        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="Open CSV...", command=self._file_panel._load_csv,
                              accelerator="Ctrl+O")
        file_menu.add_separator()
        file_menu.add_command(label="Settings...", command=self._show_settings)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self._on_close,
                              accelerator="Ctrl+Q")

        # Query menu
        query_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Query", menu=query_menu)
        query_menu.add_command(label="Run Query", command=self._run_current_query,
                               accelerator="Ctrl+Enter")
        query_menu.add_command(label="Cancel Query", command=self._cancel_query,
                               accelerator="Escape")
        query_menu.add_separator()
        query_menu.add_command(label="Clear Query", command=self._query_editor.clear,
                               accelerator="Ctrl+L")
        query_menu.add_command(label="Save to Favorites", command=self._save_favorite,
                               accelerator="Ctrl+S")
        query_menu.add_separator()
        query_menu.add_command(label="Clear History",
                               command=self._clear_history)
        query_menu.add_command(label="Clear Favorites",
                               command=self._clear_favorites)

        # Favorites menu (dynamic)
        self._favorites_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Favorites", menu=self._favorites_menu)
        self._update_favorites_menu()

        # Examples menu
        examples_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Examples", menu=examples_menu)

        example_queries = self._get_example_queries()
        for name, query in example_queries:
            examples_menu.add_command(
                label=name,
                command=lambda q=query: self._query_editor.set_query(q)
            )

        # Help menu
        help_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Help", menu=help_menu)
        help_menu.add_command(label="Keyboard Shortcuts",
                              command=self._show_shortcuts)
        help_menu.add_command(label="About", command=self._show_about)

    def _update_favorites_menu(self) -> None:
        """Update the Favorites menu with current favorites."""
        self._favorites_menu.delete(0, tk.END)

        favorites = self._favorites.get_all()
        if favorites:
            for fav in favorites:
                self._favorites_menu.add_command(
                    label=fav.name,
                    command=lambda q=fav.query: self._query_editor.set_query(q)
                )
            self._favorites_menu.add_separator()

        self._favorites_menu.add_command(
            label="Manage Favorites...",
            command=self._manage_favorites
        )

    def _get_example_queries(self) -> list:
        """Get example queries.

        Returns:
            List of (name, query) tuples.
        """
        return [
            ("Top units by win rate",
             '''SELECT name, faction, matches, win_rate, points
FROM simulation_stats
WHERE matches >= 100
ORDER BY win_rate DESC
LIMIT 20'''),

            ("Faction performance summary",
             '''SELECT faction,
       COUNT(*) as unit_count,
       ROUND(AVG(win_rate), 4) as avg_win_rate,
       ROUND(AVG(points), 0) as avg_points
FROM simulation_stats
GROUP BY faction
ORDER BY avg_win_rate DESC'''),

            ("Points efficiency",
             '''SELECT name, faction, points, win_rate,
       ROUND(win_rate / points * 100, 4) as efficiency_per_point
FROM simulation_stats
WHERE matches >= 50 AND points > 0
ORDER BY efficiency_per_point DESC
LIMIT 30'''),

            ("Defense vs win rate",
             '''SELECT defense,
       COUNT(*) as unit_count,
       ROUND(AVG(win_rate), 4) as avg_win_rate,
       ROUND(AVG(points), 0) as avg_points
FROM simulation_stats
WHERE matches >= 50
GROUP BY defense
ORDER BY defense'''),

            ("Top winners by total wins",
             '''SELECT name, faction, points, matches, wins, win_rate
FROM simulation_stats
WHERE matches >= 100
ORDER BY wins DESC
LIMIT 25'''),

            ("Units that draw frequently",
             '''SELECT name, faction, points, matches, draws,
       ROUND(draws * 100.0 / matches, 2) as draw_rate
FROM simulation_stats
WHERE matches >= 100
ORDER BY draw_rate DESC
LIMIT 20'''),
        ]

    def _load_initial_file(self, path: str) -> None:
        """Load an initial file.

        Args:
            path: Path to the file.
        """
        if Path(path).exists():
            self._file_panel._load_file(path)
        else:
            self._error_handler.handle_csv_load_error(
                FileNotFoundError(f"File not found: {path}"), path
            )

    def _on_table_change(self) -> None:
        """Handle table changes."""
        self._schema_panel.refresh()

    def _execute_query(self, query: str) -> None:
        """Execute a SQL query.

        Args:
            query: SQL query to execute.
        """
        if not query.strip():
            return

        # Update status and show cancel button
        self._set_status("Executing query...", "info")
        self._cancel_btn.pack(side=tk.RIGHT, padx=5)
        self._root.update()

        # Get timeout
        timeout = self._query_timeout.get()
        if timeout <= 0:
            timeout = None

        # Execute query
        result = self._db.execute(query, timeout_seconds=timeout)

        # Hide cancel button
        self._cancel_btn.pack_forget()

        # Record in history (only successful queries)
        self._history.add(
            query,
            execution_time_ms=result.execution_time_ms,
            row_count=result.row_count,
            success=result.success
        )
        self._query_editor.refresh_history()

        if result.success:
            # Display results
            self._results_table.set_data(result.data)
            self._chart_panel.set_data(result.data)

            # Update status
            time_str = f"{result.execution_time_ms:.1f}ms"
            if result.row_count == 0:
                self._set_status(
                    f"Query returned no results ({time_str})", "warning"
                )
            else:
                self._set_status(
                    f"Query returned {result.row_count:,} rows ({time_str})", "success"
                )

            # Switch to results tab
            self._notebook.select(0)
        elif result.timed_out:
            self._set_status(f"Query timed out after {timeout}s", "warning")
        elif result.cancelled:
            self._set_status("Query cancelled", "info")
        else:
            # Clear results on error
            self._results_table.set_data(None)

    def _run_current_query(self) -> None:
        """Run the current query from the editor."""
        query = self._query_editor.get_query()
        if query:
            self._execute_query(query)

    def _cancel_query(self) -> None:
        """Cancel the running query."""
        if self._db.is_query_running():
            self._db.cancel_query()
            self._set_status("Cancelling query...", "warning")

    def _insert_query_text(self, text: str) -> None:
        """Insert text into the query editor.

        Args:
            text: Text to insert.
        """
        self._query_editor.set_query(text)
        self._query_editor.focus_editor()

    def _save_favorite(self) -> None:
        """Save current query to favorites."""
        query = self._query_editor.get_query()
        if not query:
            messagebox.showwarning("Save Favorite", "No query to save.")
            return

        name = simpledialog.askstring(
            "Save to Favorites",
            "Enter a name for this query:",
            parent=self._root
        )

        if name:
            if self._favorites.add(name, query):
                self._update_favorites_menu()
                self._query_editor.refresh_favorites()
                messagebox.showinfo("Saved", f"Query saved as '{name}'")
            else:
                if messagebox.askyesno(
                    "Overwrite?",
                    f"A favorite named '{name}' already exists.\nOverwrite it?"
                ):
                    self._favorites.update(name, query)
                    self._update_favorites_menu()
                    messagebox.showinfo("Updated", f"Query '{name}' updated")

    def _clear_history(self) -> None:
        """Clear query history."""
        if messagebox.askyesno("Clear History", "Clear all query history?"):
            self._history.clear()
            self._query_editor.refresh_history()

    def _clear_favorites(self) -> None:
        """Clear all favorites."""
        if messagebox.askyesno("Clear Favorites", "Delete all saved favorites?"):
            self._favorites.clear()
            self._update_favorites_menu()
            self._query_editor.refresh_favorites()

    def _manage_favorites(self) -> None:
        """Show favorites management dialog."""
        favorites = self._favorites.get_all()
        if not favorites:
            messagebox.showinfo("Favorites", "No favorites saved yet.")
            return

        # Create dialog
        dialog = tk.Toplevel(self._root)
        dialog.title("Manage Favorites")
        dialog.geometry("500x400")
        dialog.transient(self._root)
        dialog.grab_set()

        # Listbox with favorites
        list_frame = ttk.Frame(dialog)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        listbox = tk.Listbox(list_frame, selectmode=tk.SINGLE)
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=listbox.yview)
        listbox.configure(yscrollcommand=scrollbar.set)

        listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        for fav in favorites:
            listbox.insert(tk.END, fav.name)

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def delete_selected():
            selection = listbox.curselection()
            if selection:
                name = listbox.get(selection[0])
                if messagebox.askyesno("Delete", f"Delete '{name}'?"):
                    self._favorites.remove(name)
                    listbox.delete(selection[0])
                    self._update_favorites_menu()
                    self._query_editor.refresh_favorites()

        def rename_selected():
            selection = listbox.curselection()
            if selection:
                old_name = listbox.get(selection[0])
                new_name = simpledialog.askstring(
                    "Rename", f"New name for '{old_name}':",
                    parent=dialog
                )
                if new_name and self._favorites.rename(old_name, new_name):
                    listbox.delete(selection[0])
                    listbox.insert(selection[0], new_name)
                    self._update_favorites_menu()
                    self._query_editor.refresh_favorites()

        ttk.Button(btn_frame, text="Delete", command=delete_selected).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Rename", command=rename_selected).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Close", command=dialog.destroy).pack(side=tk.RIGHT, padx=5)

    def _show_settings(self) -> None:
        """Show settings dialog."""
        dialog = tk.Toplevel(self._root)
        dialog.title("Settings")
        dialog.geometry("400x300")
        dialog.transient(self._root)
        dialog.grab_set()

        # Settings frame
        settings_frame = ttk.LabelFrame(dialog, text="Query Settings", padding=10)
        settings_frame.pack(fill=tk.X, padx=10, pady=10)

        # Timeout setting
        timeout_frame = ttk.Frame(settings_frame)
        timeout_frame.pack(fill=tk.X, pady=5)

        ttk.Label(timeout_frame, text="Default Query Timeout (seconds):").pack(side=tk.LEFT)
        timeout_var = tk.IntVar(value=self._config.query_timeout_seconds)
        timeout_spin = ttk.Spinbox(timeout_frame, from_=0, to=300,
                                   textvariable=timeout_var, width=10)
        timeout_spin.pack(side=tk.RIGHT)

        ttk.Label(settings_frame, text="(0 = no timeout)",
                  foreground="gray").pack(anchor=tk.W)

        # Results page size
        page_frame = ttk.Frame(settings_frame)
        page_frame.pack(fill=tk.X, pady=5)

        ttk.Label(page_frame, text="Results Page Size:").pack(side=tk.LEFT)
        page_var = tk.IntVar(value=self._config.results_page_size)
        page_spin = ttk.Spinbox(page_frame, from_=100, to=10000,
                                textvariable=page_var, width=10)
        page_spin.pack(side=tk.RIGHT)

        # History size
        history_frame = ttk.Frame(settings_frame)
        history_frame.pack(fill=tk.X, pady=5)

        ttk.Label(history_frame, text="Max History Entries:").pack(side=tk.LEFT)
        history_var = tk.IntVar(value=self._config.max_history_size)
        history_spin = ttk.Spinbox(history_frame, from_=10, to=200,
                                   textvariable=history_var, width=10)
        history_spin.pack(side=tk.RIGHT)

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def save_settings():
            self._config.query_timeout_seconds = timeout_var.get()
            self._config.results_page_size = page_var.get()
            self._config.max_history_size = history_var.get()
            self._query_timeout.set(timeout_var.get())
            self._config.save()
            dialog.destroy()
            messagebox.showinfo("Settings", "Settings saved.")

        ttk.Button(btn_frame, text="Save", command=save_settings).pack(side=tk.RIGHT, padx=5)
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(side=tk.RIGHT, padx=5)

    def _on_status_update(self, result: ErrorResult) -> None:
        """Handle status updates from error handler.

        Args:
            result: Error result with status info.
        """
        severity_map = {
            ErrorSeverity.SUCCESS: "success",
            ErrorSeverity.INFO: "info",
            ErrorSeverity.WARNING: "warning",
            ErrorSeverity.ERROR: "error"
        }
        self._set_status(result.message, severity_map.get(result.severity, "info"))

    def _set_status(self, message: str, level: str = "info") -> None:
        """Set status bar message.

        Args:
            message: Status message.
            level: One of 'success', 'info', 'warning', 'error'.
        """
        colors = {
            "success": "#2e7d32",  # Green
            "info": "#1976d2",     # Blue
            "warning": "#f57c00",  # Orange
            "error": "#c62828"     # Red
        }
        self._status_label.config(
            text=message,
            foreground=colors.get(level, "black")
        )

    def _show_shortcuts(self) -> None:
        """Show keyboard shortcuts dialog."""
        shortcuts = """Keyboard Shortcuts:

Query Editor:
  Ctrl+Enter / F5  - Run query
  Ctrl+L           - Clear query
  Ctrl+S           - Save to favorites
  Escape           - Cancel running query

Application:
  Ctrl+O           - Open CSV file
  Ctrl+Q           - Quit

Results Table:
  Right-click      - Context menu
  Click column     - Sort by column

Tip: Drag the border between Query and Results
     panels to resize the query editor.
"""
        messagebox.showinfo("Keyboard Shortcuts", shortcuts)

    def _show_about(self) -> None:
        """Show about dialog."""
        about = """CSV Query Tool v1.1

A SQL-based CSV analysis tool for
Science Battle Simulator.

Powered by DuckDB for fast analytics.

Features:
- Load and query CSV files with SQL
- View and export results
- Create charts for visualization
- Save queries to favorites
- Query timeout controls
- Full query history
"""
        messagebox.showinfo("About", about)

    def _on_close(self) -> None:
        """Handle window close."""
        # Save window geometry
        self._config.set_geometry(self._root.geometry())
        self._config.query_timeout_seconds = self._query_timeout.get()
        self._config.save()

        # Close database
        self._db.close()

        # Destroy window
        self._root.destroy()

    def run(self) -> None:
        """Start the application main loop."""
        self._logger.info("Starting CSV Query Tool")
        self._root.mainloop()
