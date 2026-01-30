"""Main application class for CSV Query Tool."""

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog, filedialog
import logging
import sys
from pathlib import Path
from typing import Optional, List

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
from .ui.log_panel import LogPanel, setup_gui_logging


class CSVQueryApp:
    """Main application window for CSV Query Tool."""

    def __init__(self, initial_file: Optional[str] = None):
        """Initialize the application.

        Args:
            initial_file: Optional CSV file to load on startup.
        """
        # Set up basic logging (will be replaced by GUI logging after UI setup)
        logging.basicConfig(
            level=logging.INFO,
            format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
        )
        self._logger = logging.getLogger(__name__)
        self._log_panel = None  # Will be set up in _setup_ui

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
        self._autocomplete_enabled = tk.BooleanVar(value=self._config.autocomplete_enabled)

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
            on_execute=self._execute_query,
            autocomplete_enabled=self._autocomplete_enabled
        )
        self._query_editor.pack(fill=tk.BOTH, expand=True)

        # Status bar frame (between editor and results)
        status_container = ttk.Frame(self._right_paned)
        self._right_paned.add(status_container, weight=0)

        self._status_frame = ttk.Frame(status_container)
        self._status_frame.pack(fill=tk.X, padx=5, pady=2)

        # Database status label (left side)
        self._db_status_label = ttk.Label(
            self._status_frame, text="In-Memory",
            relief=tk.SUNKEN, padding=(5, 2), width=30
        )
        self._db_status_label.pack(side=tk.LEFT, padx=(0, 5))

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

        # Log tab
        log_frame = ttk.Frame(self._notebook)
        self._notebook.add(log_frame, text="Log")

        self._log_panel = LogPanel(log_frame)
        self._log_panel.pack(fill=tk.BOTH, expand=True)

        # Set up GUI logging (redirect all logs and stdout/stderr to log panel)
        setup_gui_logging(self._log_panel, capture_stdout=True, capture_stderr=True)
        self._logger.info("GUI logging initialized")

        # Connect file panel insert callback (for double-click to insert)
        self._file_panel.set_insert_callback(self._insert_query_text)

        # Set up menu
        self._setup_menu()

        # Initialize database status display
        self._update_db_status()

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

        # Database menu
        db_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Database", menu=db_menu)
        db_menu.add_command(label="New Database...", command=self._new_database)
        db_menu.add_command(label="Open Database...", command=self._open_database)
        db_menu.add_separator()
        db_menu.add_command(label="Import CSVs...", command=self._import_csvs)
        db_menu.add_command(label="Import Parquet...", command=self._import_parquet)
        db_menu.add_separator()
        db_menu.add_command(label="Create Table from Query...",
                            command=self._create_table_from_query)
        db_menu.add_command(label="Create Custom Table...",
                            command=self._create_custom_table)
        db_menu.add_separator()
        db_menu.add_command(label="Create Stored Query...",
                            command=self._create_stored_query)
        db_menu.add_separator()
        db_menu.add_command(label="Create Index...", command=self._show_create_index)
        db_menu.add_command(label="Manage Indexes...", command=self._show_manage_indexes)
        db_menu.add_separator()
        db_menu.add_command(label="Export Table to Parquet...",
                            command=self._export_table_parquet)
        db_menu.add_command(label="Compact Database", command=self._vacuum_database)
        db_menu.add_separator()
        db_menu.add_command(label="Switch to In-Memory", command=self._switch_to_memory)

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
        # Refresh table list from database to catch DDL changes
        self._db.refresh_tables()
        self._schema_panel.refresh()
        self._file_panel._update_tables_list()
        self._update_db_status()
        self._update_autocomplete_columns()

    def _update_autocomplete_columns(self) -> None:
        """Update the autocomplete with current table and view columns."""
        table_columns = self._db.get_all_columns_including_views()
        self._query_editor.update_table_columns(table_columns)

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
        dialog.geometry("400x350")
        dialog.transient(self._root)
        dialog.grab_set()

        # Query Settings frame
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

        # Editor Settings frame
        editor_frame = ttk.LabelFrame(dialog, text="Editor Settings", padding=10)
        editor_frame.pack(fill=tk.X, padx=10, pady=10)

        # Autocomplete toggle
        autocomplete_var = tk.BooleanVar(value=self._config.autocomplete_enabled)
        autocomplete_check = ttk.Checkbutton(
            editor_frame,
            text="Enable SQL autocomplete (type alias. for column suggestions)",
            variable=autocomplete_var
        )
        autocomplete_check.pack(anchor=tk.W, pady=5)

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def save_settings():
            self._config.query_timeout_seconds = timeout_var.get()
            self._config.results_page_size = page_var.get()
            self._config.max_history_size = history_var.get()
            self._config.autocomplete_enabled = autocomplete_var.get()
            self._query_timeout.set(timeout_var.get())
            self._autocomplete_enabled.set(autocomplete_var.get())
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
        level = severity_map.get(result.severity, "info")
        self._set_status(result.message, level)

        # Also log to log panel
        if self._log_panel:
            log_level_map = {
                "success": "SUCCESS",
                "info": "INFO",
                "warning": "WARNING",
                "error": "ERROR"
            }
            self._log_panel.log(result.message, log_level_map.get(level, "INFO"))

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
        about = """CSV Query Tool v1.2

A SQL-based CSV analysis tool for
Science Battle Simulator.

Powered by DuckDB for fast analytics.

Features:
- Load and query CSV files with SQL
- Persistent database for large datasets
- Create indexes for faster queries
- View and export results
- Create charts for visualization
- Save queries to favorites
- Query timeout controls
- Full query history
"""
        messagebox.showinfo("About", about)

    # =========================================================================
    # Database Management Methods
    # =========================================================================

    def _new_database(self) -> None:
        """Create a new persistent database."""
        path = filedialog.asksaveasfilename(
            title="Create New Database",
            defaultextension=".duckdb",
            filetypes=[("DuckDB Database", "*.duckdb"), ("All Files", "*.*")],
            parent=self._root
        )

        if path:
            if self._db.create_database(path):
                self._update_db_status()
                self._on_table_change()
                messagebox.showinfo(
                    "Database Created",
                    f"Created new database:\n{Path(path).name}\n\n"
                    "Use Database → Import CSVs to import your data."
                )

    def _open_database(self) -> None:
        """Open an existing database."""
        path = filedialog.askopenfilename(
            title="Open Database",
            filetypes=[("DuckDB Database", "*.duckdb"), ("All Files", "*.*")],
            parent=self._root
        )

        if path:
            if self._db.connect_to_database(path):
                self._update_db_status()
                self._on_table_change()

                # Show info about loaded tables
                tables = self._db.get_tables()
                if tables:
                    table_info = "\n".join(
                        f"  • {t.name}: {t.row_count:,} rows"
                        for t in tables
                    )
                    messagebox.showinfo(
                        "Database Opened",
                        f"Opened database with {len(tables)} table(s):\n\n{table_info}"
                    )
                else:
                    messagebox.showinfo(
                        "Database Opened",
                        "Database opened (no tables yet).\n\n"
                        "Use Database → Import CSVs to add data."
                    )

    def _import_csvs(self) -> None:
        """Import multiple CSV files into the database."""
        paths = filedialog.askopenfilenames(
            title="Select CSV Files to Import",
            filetypes=[("CSV Files", "*.csv"), ("All Files", "*.*")],
            parent=self._root
        )

        if not paths:
            return

        # Show progress dialog
        progress_dialog = tk.Toplevel(self._root)
        progress_dialog.title("Importing CSVs")
        progress_dialog.geometry("400x150")
        progress_dialog.transient(self._root)
        progress_dialog.grab_set()

        ttk.Label(progress_dialog, text="Importing CSV files...").pack(pady=10)

        progress_var = tk.DoubleVar(value=0)
        progress_bar = ttk.Progressbar(
            progress_dialog, variable=progress_var,
            maximum=len(paths), length=350
        )
        progress_bar.pack(pady=10, padx=20)

        status_var = tk.StringVar(value="Starting...")
        status_label = ttk.Label(progress_dialog, textvariable=status_var)
        status_label.pack(pady=5)

        def update_progress(msg: str, current: int, total: int):
            progress_var.set(current)
            status_var.set(f"{current}/{total}: {msg}")
            progress_dialog.update()

        # Import files
        self._root.after(100, lambda: self._do_import_csvs(
            paths, progress_dialog, update_progress
        ))

    def _do_import_csvs(self, paths: tuple, dialog: tk.Toplevel,
                        progress_callback) -> None:
        """Perform CSV import (called after dialog is shown)."""
        results = self._db.import_multiple_csvs(list(paths), progress_callback)
        dialog.destroy()

        # Refresh UI
        self._on_table_change()
        self._update_db_status()

        # Show summary
        if results:
            total_rows = sum(t.row_count for t in results)
            table_info = "\n".join(
                f"  • {t.name}: {t.row_count:,} rows"
                for t in results
            )
            messagebox.showinfo(
                "Import Complete",
                f"Imported {len(results)} file(s) ({total_rows:,} total rows):\n\n{table_info}"
            )
        else:
            messagebox.showwarning("Import Failed", "No files were imported.")

    def _import_parquet(self) -> None:
        """Import a Parquet file."""
        path = filedialog.askopenfilename(
            title="Select Parquet File",
            filetypes=[("Parquet Files", "*.parquet"), ("All Files", "*.*")],
            parent=self._root
        )

        if path:
            result = self._db.import_parquet(path)
            if result:
                self._on_table_change()
                self._update_db_status()
                messagebox.showinfo(
                    "Import Complete",
                    f"Imported '{result.name}' ({result.row_count:,} rows)"
                )

    def _show_create_index(self) -> None:
        """Show dialog to create an index."""
        tables = self._db.get_tables()
        if not tables:
            messagebox.showwarning(
                "No Tables",
                "No tables in database. Import some data first."
            )
            return

        # Create dialog
        dialog = tk.Toplevel(self._root)
        dialog.title("Create Index")
        dialog.geometry("450x400")
        dialog.transient(self._root)
        dialog.grab_set()

        # Table selection
        table_frame = ttk.LabelFrame(dialog, text="Select Table", padding=10)
        table_frame.pack(fill=tk.X, padx=10, pady=10)

        table_var = tk.StringVar(value=tables[0].name)
        table_combo = ttk.Combobox(
            table_frame, textvariable=table_var,
            values=[t.name for t in tables], state="readonly"
        )
        table_combo.pack(fill=tk.X)

        # Column selection
        col_frame = ttk.LabelFrame(dialog, text="Select Columns", padding=10)
        col_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # Columns listbox with scrollbar
        col_list_frame = ttk.Frame(col_frame)
        col_list_frame.pack(fill=tk.BOTH, expand=True)

        col_listbox = tk.Listbox(
            col_list_frame, selectmode=tk.MULTIPLE, exportselection=False
        )
        col_scrollbar = ttk.Scrollbar(
            col_list_frame, orient=tk.VERTICAL, command=col_listbox.yview
        )
        col_listbox.configure(yscrollcommand=col_scrollbar.set)
        col_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        col_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        def update_columns(*args):
            col_listbox.delete(0, tk.END)
            table = self._db.get_table(table_var.get())
            if table:
                for col_name, col_type in table.columns:
                    col_listbox.insert(tk.END, f"{col_name} ({col_type})")

        table_combo.bind("<<ComboboxSelected>>", update_columns)
        update_columns()

        # Options
        opt_frame = ttk.Frame(dialog)
        opt_frame.pack(fill=tk.X, padx=10, pady=5)

        unique_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            opt_frame, text="Unique index", variable=unique_var
        ).pack(side=tk.LEFT)

        # Index name
        name_frame = ttk.Frame(dialog)
        name_frame.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(name_frame, text="Index name (optional):").pack(side=tk.LEFT)
        name_var = tk.StringVar()
        ttk.Entry(name_frame, textvariable=name_var, width=30).pack(
            side=tk.LEFT, padx=5
        )

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def create_index():
            selections = col_listbox.curselection()
            if not selections:
                messagebox.showwarning("Select Columns", "Please select at least one column.")
                return

            table = self._db.get_table(table_var.get())
            if not table:
                return

            columns = [table.columns[i][0] for i in selections]
            index_name = name_var.get().strip() or None

            if self._db.create_index(
                table_var.get(), columns, index_name, unique_var.get()
            ):
                dialog.destroy()

        ttk.Button(btn_frame, text="Create Index", command=create_index).pack(
            side=tk.RIGHT, padx=5
        )
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(
            side=tk.RIGHT, padx=5
        )

    def _show_manage_indexes(self) -> None:
        """Show dialog to manage existing indexes."""
        indexes = self._db.get_indexes()

        # Create dialog
        dialog = tk.Toplevel(self._root)
        dialog.title("Manage Indexes")
        dialog.geometry("500x350")
        dialog.transient(self._root)
        dialog.grab_set()

        if not indexes:
            ttk.Label(
                dialog, text="No indexes in database.\n\nUse 'Create Index...' to add one.",
                justify=tk.CENTER
            ).pack(expand=True)
            ttk.Button(dialog, text="Close", command=dialog.destroy).pack(pady=10)
            return

        # Index list
        list_frame = ttk.Frame(dialog)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        columns = ("name", "table", "columns", "unique")
        tree = ttk.Treeview(list_frame, columns=columns, show="headings")
        tree.heading("name", text="Index Name")
        tree.heading("table", text="Table")
        tree.heading("columns", text="Columns")
        tree.heading("unique", text="Unique")

        tree.column("name", width=150)
        tree.column("table", width=120)
        tree.column("columns", width=150)
        tree.column("unique", width=60)

        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=tree.yview)
        tree.configure(yscrollcommand=scrollbar.set)

        tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        for idx in indexes:
            tree.insert("", tk.END, values=(
                idx.name, idx.table_name,
                ", ".join(idx.columns),
                "Yes" if idx.is_unique else "No"
            ))

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def drop_selected():
            selection = tree.selection()
            if not selection:
                return
            item = tree.item(selection[0])
            name = item["values"][0]

            if messagebox.askyesno("Drop Index", f"Drop index '{name}'?"):
                if self._db.drop_index(name):
                    tree.delete(selection[0])

        ttk.Button(btn_frame, text="Drop Selected", command=drop_selected).pack(
            side=tk.LEFT, padx=5
        )
        ttk.Button(btn_frame, text="Close", command=dialog.destroy).pack(
            side=tk.RIGHT, padx=5
        )

    def _export_table_parquet(self) -> None:
        """Export a table to Parquet format."""
        tables = self._db.get_tables()
        if not tables:
            messagebox.showwarning("No Tables", "No tables to export.")
            return

        # Ask which table
        table_names = [t.name for t in tables]

        dialog = tk.Toplevel(self._root)
        dialog.title("Export to Parquet")
        dialog.geometry("350x150")
        dialog.transient(self._root)
        dialog.grab_set()

        ttk.Label(dialog, text="Select table to export:").pack(pady=10)

        table_var = tk.StringVar(value=table_names[0])
        ttk.Combobox(
            dialog, textvariable=table_var,
            values=table_names, state="readonly", width=35
        ).pack(pady=5)

        def do_export():
            dialog.destroy()
            table_name = table_var.get()

            path = filedialog.asksaveasfilename(
                title="Save Parquet File",
                defaultextension=".parquet",
                initialfile=f"{table_name}.parquet",
                filetypes=[("Parquet Files", "*.parquet"), ("All Files", "*.*")],
                parent=self._root
            )

            if path:
                if self._db.export_table_to_parquet(table_name, path):
                    messagebox.showinfo(
                        "Export Complete",
                        f"Table '{table_name}' exported to:\n{path}"
                    )

        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(pady=10)
        ttk.Button(btn_frame, text="Export", command=do_export).pack(
            side=tk.LEFT, padx=5
        )
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(
            side=tk.LEFT, padx=5
        )

    def _vacuum_database(self) -> None:
        """Compact the database file."""
        if not self._db.is_persistent():
            messagebox.showinfo(
                "In-Memory Database",
                "Compacting is only available for persistent databases."
            )
            return

        self._set_status("Compacting database...", "info")
        self._root.update()

        if self._db.vacuum():
            size = self._db.get_database_size()
            if size:
                size_mb = size / (1024 * 1024)
                messagebox.showinfo(
                    "Compact Complete",
                    f"Database compacted.\nSize: {size_mb:.2f} MB"
                )
            self._update_db_status()

    def _create_table_from_query(self) -> None:
        """Create a new table from a SELECT query result."""
        # Get current query
        query = self._query_editor.get_query()

        # Create dialog
        dialog = tk.Toplevel(self._root)
        dialog.title("Create Table from Query")
        dialog.geometry("600x450")
        dialog.transient(self._root)
        dialog.grab_set()

        # Table name input
        name_frame = ttk.Frame(dialog)
        name_frame.pack(fill=tk.X, padx=10, pady=10)

        ttk.Label(name_frame, text="Table Name:").pack(side=tk.LEFT)
        name_var = tk.StringVar(value="custom_table")
        name_entry = ttk.Entry(name_frame, textvariable=name_var, width=30)
        name_entry.pack(side=tk.LEFT, padx=5)

        # Query text
        query_frame = ttk.LabelFrame(dialog, text="SELECT Query", padding=10)
        query_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        query_text = tk.Text(query_frame, wrap=tk.NONE, font=("Consolas", 10))
        query_scroll_y = ttk.Scrollbar(query_frame, orient=tk.VERTICAL,
                                       command=query_text.yview)
        query_scroll_x = ttk.Scrollbar(query_frame, orient=tk.HORIZONTAL,
                                       command=query_text.xview)
        query_text.configure(yscrollcommand=query_scroll_y.set,
                            xscrollcommand=query_scroll_x.set)

        query_text.grid(row=0, column=0, sticky="nsew")
        query_scroll_y.grid(row=0, column=1, sticky="ns")
        query_scroll_x.grid(row=1, column=0, sticky="ew")

        query_frame.grid_rowconfigure(0, weight=1)
        query_frame.grid_columnconfigure(0, weight=1)

        # Pre-fill with current query if it's a SELECT
        if query and query.strip().upper().startswith("SELECT"):
            query_text.insert("1.0", query)

        # Help text
        help_label = ttk.Label(
            dialog,
            text="Enter a SELECT query. The result will be stored as a new table.",
            foreground="gray"
        )
        help_label.pack(padx=10, pady=5, anchor=tk.W)

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def create_table():
            table_name = name_var.get().strip()
            select_query = query_text.get("1.0", tk.END).strip()

            if not table_name:
                messagebox.showwarning("Invalid Name", "Please enter a table name.")
                return

            if not select_query:
                messagebox.showwarning("No Query", "Please enter a SELECT query.")
                return

            # Validate query starts with SELECT
            if not select_query.upper().startswith("SELECT"):
                messagebox.showwarning(
                    "Invalid Query",
                    "Query must be a SELECT statement."
                )
                return

            # Create the table using CREATE TABLE AS
            create_sql = f'CREATE TABLE "{table_name}" AS {select_query}'

            try:
                # Drop existing table if it exists
                self._db.execute_non_query(f'DROP TABLE IF EXISTS "{table_name}"')

                # Create the new table
                if self._db.execute_non_query(create_sql):
                    dialog.destroy()
                    self._on_table_change()

                    # Get row count
                    result = self._db.execute(f'SELECT COUNT(*) FROM "{table_name}"')
                    row_count = result.data.iloc[0, 0] if result.success else 0

                    messagebox.showinfo(
                        "Table Created",
                        f"Created table '{table_name}' with {row_count:,} rows."
                    )
                else:
                    messagebox.showerror(
                        "Error",
                        "Failed to create table. Check the query syntax."
                    )
            except Exception as e:
                messagebox.showerror("Error", f"Failed to create table:\n{str(e)}")

        ttk.Button(btn_frame, text="Create Table", command=create_table).pack(
            side=tk.RIGHT, padx=5
        )
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(
            side=tk.RIGHT, padx=5
        )

    def _create_custom_table(self) -> None:
        """Create a custom table with user-defined columns."""
        # Create dialog
        dialog = tk.Toplevel(self._root)
        dialog.title("Create Custom Table")
        dialog.geometry("500x500")
        dialog.transient(self._root)
        dialog.grab_set()

        # Table name input
        name_frame = ttk.Frame(dialog)
        name_frame.pack(fill=tk.X, padx=10, pady=10)

        ttk.Label(name_frame, text="Table Name:").pack(side=tk.LEFT)
        name_var = tk.StringVar(value="my_table")
        name_entry = ttk.Entry(name_frame, textvariable=name_var, width=30)
        name_entry.pack(side=tk.LEFT, padx=5)

        # Columns definition
        col_frame = ttk.LabelFrame(dialog, text="Columns", padding=10)
        col_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # Columns list
        columns_list = tk.Listbox(col_frame, selectmode=tk.SINGLE)
        col_scrollbar = ttk.Scrollbar(col_frame, orient=tk.VERTICAL,
                                      command=columns_list.yview)
        columns_list.configure(yscrollcommand=col_scrollbar.set)

        columns_list.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        col_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # Column management buttons
        col_btn_frame = ttk.Frame(dialog)
        col_btn_frame.pack(fill=tk.X, padx=10, pady=5)

        # Column input frame
        input_frame = ttk.Frame(col_btn_frame)
        input_frame.pack(fill=tk.X)

        ttk.Label(input_frame, text="Name:").pack(side=tk.LEFT)
        col_name_var = tk.StringVar()
        col_name_entry = ttk.Entry(input_frame, textvariable=col_name_var, width=15)
        col_name_entry.pack(side=tk.LEFT, padx=2)

        ttk.Label(input_frame, text="Type:").pack(side=tk.LEFT, padx=(10, 0))
        col_type_var = tk.StringVar(value="VARCHAR")
        col_type_combo = ttk.Combobox(
            input_frame, textvariable=col_type_var,
            values=["VARCHAR", "INTEGER", "BIGINT", "DOUBLE", "BOOLEAN",
                   "DATE", "TIMESTAMP", "DECIMAL(10,2)"],
            state="readonly", width=15
        )
        col_type_combo.pack(side=tk.LEFT, padx=2)

        def add_column():
            col_name = col_name_var.get().strip()
            col_type = col_type_var.get()
            if col_name:
                columns_list.insert(tk.END, f"{col_name} ({col_type})")
                col_name_var.set("")

        def remove_column():
            selection = columns_list.curselection()
            if selection:
                columns_list.delete(selection[0])

        ttk.Button(input_frame, text="Add", command=add_column).pack(side=tk.LEFT, padx=5)
        ttk.Button(input_frame, text="Remove", command=remove_column).pack(side=tk.LEFT, padx=2)

        # Help text
        help_label = ttk.Label(
            dialog,
            text="Define columns for your custom table. Use INSERT to add data later.",
            foreground="gray"
        )
        help_label.pack(padx=10, pady=5, anchor=tk.W)

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def create_table():
            table_name = name_var.get().strip()
            columns = list(columns_list.get(0, tk.END))

            if not table_name:
                messagebox.showwarning("Invalid Name", "Please enter a table name.")
                return

            if not columns:
                messagebox.showwarning("No Columns", "Please add at least one column.")
                return

            # Parse columns
            col_defs = []
            for col in columns:
                # Parse "col_name (TYPE)"
                match = col.rsplit(" (", 1)
                if len(match) == 2:
                    c_name = match[0]
                    c_type = match[1].rstrip(")")
                    col_defs.append(f'"{c_name}" {c_type}')

            if not col_defs:
                messagebox.showerror("Error", "Could not parse column definitions.")
                return

            # Create the table
            create_sql = f'CREATE TABLE "{table_name}" ({", ".join(col_defs)})'

            try:
                # Drop existing table if it exists
                self._db.execute_non_query(f'DROP TABLE IF EXISTS "{table_name}"')

                # Create the new table
                if self._db.execute_non_query(create_sql):
                    dialog.destroy()
                    self._on_table_change()

                    messagebox.showinfo(
                        "Table Created",
                        f"Created empty table '{table_name}' with {len(col_defs)} column(s).\n\n"
                        "Use INSERT statements to add data."
                    )
                else:
                    messagebox.showerror(
                        "Error",
                        "Failed to create table. Check the column definitions."
                    )
            except Exception as e:
                messagebox.showerror("Error", f"Failed to create table:\n{str(e)}")

        ttk.Button(btn_frame, text="Create Table", command=create_table).pack(
            side=tk.RIGHT, padx=5
        )
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(
            side=tk.RIGHT, padx=5
        )

    def _create_stored_query(self) -> None:
        """Create a stored query (view) from the current SELECT statement."""
        # Get current query
        query = self._query_editor.get_query()

        # Create dialog
        dialog = tk.Toplevel(self._root)
        dialog.title("Create Stored Query")
        dialog.geometry("600x450")
        dialog.transient(self._root)
        dialog.grab_set()

        # Query name input
        name_frame = ttk.Frame(dialog)
        name_frame.pack(fill=tk.X, padx=10, pady=10)

        ttk.Label(name_frame, text="Query Name:").pack(side=tk.LEFT)
        name_var = tk.StringVar(value="my_query")
        name_entry = ttk.Entry(name_frame, textvariable=name_var, width=30)
        name_entry.pack(side=tk.LEFT, padx=5)

        # Query text
        query_frame = ttk.LabelFrame(dialog, text="SELECT Query", padding=10)
        query_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        query_text = tk.Text(query_frame, wrap=tk.NONE, font=("Consolas", 10))
        query_scroll_y = ttk.Scrollbar(query_frame, orient=tk.VERTICAL,
                                       command=query_text.yview)
        query_scroll_x = ttk.Scrollbar(query_frame, orient=tk.HORIZONTAL,
                                       command=query_text.xview)
        query_text.configure(yscrollcommand=query_scroll_y.set,
                            xscrollcommand=query_scroll_x.set)

        query_text.grid(row=0, column=0, sticky="nsew")
        query_scroll_y.grid(row=0, column=1, sticky="ns")
        query_scroll_x.grid(row=1, column=0, sticky="ew")

        query_frame.grid_rowconfigure(0, weight=1)
        query_frame.grid_columnconfigure(0, weight=1)

        # Pre-fill with current query if it's a SELECT
        if query and query.strip().upper().startswith("SELECT"):
            query_text.insert("1.0", query)

        # Help text
        help_label = ttk.Label(
            dialog,
            text="Stored queries can be used in other queries like tables.\n"
                 "Example: SELECT * FROM my_query WHERE column > 10",
            foreground="gray"
        )
        help_label.pack(padx=10, pady=5, anchor=tk.W)

        # Buttons
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(fill=tk.X, padx=10, pady=10)

        def create_view():
            view_name = name_var.get().strip()
            select_query = query_text.get("1.0", tk.END).strip()

            if not view_name:
                messagebox.showwarning("Invalid Name", "Please enter a query name.")
                return

            if not select_query:
                messagebox.showwarning("No Query", "Please enter a SELECT query.")
                return

            # Validate query starts with SELECT
            if not select_query.upper().startswith("SELECT"):
                messagebox.showwarning(
                    "Invalid Query",
                    "Stored queries must be SELECT statements."
                )
                return

            # Create the view
            if self._db.create_view(view_name, select_query):
                dialog.destroy()
                self._on_table_change()
                messagebox.showinfo(
                    "Stored Query Created",
                    f"Created stored query '{view_name}'.\n\n"
                    f"You can now use it in queries like:\n"
                    f'SELECT * FROM "{view_name}"'
                )

        ttk.Button(btn_frame, text="Create Stored Query", command=create_view).pack(
            side=tk.RIGHT, padx=5
        )
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(
            side=tk.RIGHT, padx=5
        )

    def _switch_to_memory(self) -> None:
        """Switch back to in-memory database."""
        if not self._db.is_persistent():
            messagebox.showinfo("Already In-Memory", "Already using in-memory database.")
            return

        if messagebox.askyesno(
            "Switch to In-Memory",
            "This will close the current database.\n"
            "Your data will be preserved in the file.\n\n"
            "Continue?"
        ):
            # Re-create in-memory database
            self._db.close()
            self._db = DatabaseManager(self._error_handler)
            self._update_db_status()
            self._on_table_change()

    def _update_db_status(self) -> None:
        """Update the database status indicator."""
        if self._db.is_persistent():
            path = self._db.get_database_path()
            name = Path(path).name if path else "Unknown"
            size = self._db.get_database_size()
            tables = len(self._db.get_tables())

            if size:
                size_mb = size / (1024 * 1024)
                status = f"📁 {name} ({size_mb:.1f}MB, {tables} tables)"
            else:
                status = f"📁 {name} ({tables} tables)"

            self._db_status_label.config(text=status, foreground="#2e7d32")
        else:
            tables = len(self._db.get_tables())
            status = f"💾 In-Memory ({tables} tables)"
            self._db_status_label.config(text=status, foreground="#1976d2")

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
