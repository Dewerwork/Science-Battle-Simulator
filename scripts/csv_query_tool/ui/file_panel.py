"""File management panel for loading and managing CSV files."""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pathlib import Path
from typing import Callable, Optional, List

from ..database import DatabaseManager, TableInfo
from ..utils.config import Config
from ..utils.data_dictionary import (
    get_column_description,
    get_table_description,
    is_known_table
)


class TreeviewTooltip:
    """Tooltip widget for Treeview items."""

    def __init__(self, treeview: ttk.Treeview, delay: int = 500):
        """Initialize tooltip.

        Args:
            treeview: The treeview widget to attach to.
            delay: Delay in milliseconds before showing tooltip.
        """
        self._tree = treeview
        self._delay = delay
        self._tooltip_window: Optional[tk.Toplevel] = None
        self._after_id: Optional[str] = None
        self._last_item: Optional[str] = None
        self._tooltip_getter: Optional[Callable[[str], Optional[str]]] = None

        # Bind events
        self._tree.bind("<Motion>", self._on_motion)
        self._tree.bind("<Leave>", self._hide_tooltip)
        self._tree.bind("<Button-1>", self._hide_tooltip)

    def set_tooltip_getter(self, getter: Callable[[str], Optional[str]]) -> None:
        """Set the function that returns tooltip text for an item.

        Args:
            getter: Function that takes item ID and returns tooltip text.
        """
        self._tooltip_getter = getter

    def _on_motion(self, event) -> None:
        """Handle mouse motion over treeview."""
        item = self._tree.identify_row(event.y)

        if item != self._last_item:
            self._hide_tooltip()
            self._last_item = item

            if item and self._tooltip_getter:
                # Schedule tooltip display
                self._after_id = self._tree.after(
                    self._delay, lambda: self._show_tooltip(event, item)
                )

    def _show_tooltip(self, event, item: str) -> None:
        """Show tooltip for the given item."""
        if self._tooltip_window or not self._tooltip_getter:
            return

        tooltip_text = self._tooltip_getter(item)
        if not tooltip_text:
            return

        # Create tooltip window
        self._tooltip_window = tk.Toplevel(self._tree)
        self._tooltip_window.wm_overrideredirect(True)

        # Position tooltip near cursor
        x = self._tree.winfo_rootx() + event.x + 20
        y = self._tree.winfo_rooty() + event.y + 10

        self._tooltip_window.wm_geometry(f"+{x}+{y}")

        # Create tooltip content
        frame = ttk.Frame(self._tooltip_window, relief=tk.SOLID, borderwidth=1)
        frame.pack(fill=tk.BOTH, expand=True)

        label = ttk.Label(
            frame,
            text=tooltip_text,
            justify=tk.LEFT,
            wraplength=300,
            padding=(5, 3)
        )
        label.pack()

        # Style the tooltip
        self._tooltip_window.configure(background="#ffffe0")
        frame.configure(style="Tooltip.TFrame")

    def _hide_tooltip(self, event=None) -> None:
        """Hide the tooltip."""
        if self._after_id:
            self._tree.after_cancel(self._after_id)
            self._after_id = None

        if self._tooltip_window:
            self._tooltip_window.destroy()
            self._tooltip_window = None


class FilePanel(ttk.Frame):
    """Panel for loading and managing CSV files with expandable table/column tree."""

    def __init__(self, parent: tk.Widget, db_manager: DatabaseManager,
                 config: Config, on_table_change: Optional[Callable[[], None]] = None):
        """Initialize file panel.

        Args:
            parent: Parent widget.
            db_manager: Database manager instance.
            config: Application configuration.
            on_table_change: Callback when tables change.
        """
        super().__init__(parent)
        self._db = db_manager
        self._config = config
        self._on_table_change = on_table_change

        self._setup_ui()

    def _setup_ui(self) -> None:
        """Set up the UI components."""
        # Button frame
        btn_frame = ttk.Frame(self)
        btn_frame.pack(fill=tk.X, padx=5, pady=5)

        # Load CSV button
        self._load_btn = ttk.Button(
            btn_frame, text="Load CSV...", command=self._load_csv
        )
        self._load_btn.pack(side=tk.LEFT, padx=2)

        # Recent files dropdown
        self._recent_var = tk.StringVar(value="Recent Files")
        self._recent_menu = ttk.Combobox(
            btn_frame, textvariable=self._recent_var,
            state="readonly", width=20
        )
        self._recent_menu.pack(side=tk.LEFT, padx=2)
        self._recent_menu.bind("<<ComboboxSelected>>", self._on_recent_selected)
        self._update_recent_menu()

        # Refresh button (refresh table list from DB)
        self._refresh_btn = ttk.Button(
            btn_frame, text="Refresh", command=self._refresh_tables
        )
        self._refresh_btn.pack(side=tk.LEFT, padx=2)

        # Refresh All CSVs button (reload all CSV files from disk)
        self._refresh_all_btn = ttk.Button(
            btn_frame, text="Reload All", command=self._reload_all_csvs
        )
        self._refresh_all_btn.pack(side=tk.LEFT, padx=2)

        # Tables label
        tables_label = ttk.Label(self, text="Tables & Columns:")
        tables_label.pack(anchor=tk.W, padx=5, pady=(10, 2))

        # Treeview for tables and columns (with expand/collapse)
        tree_frame = ttk.Frame(self)
        tree_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)

        self._tree = ttk.Treeview(tree_frame, show="tree", selectmode="browse")
        scrollbar = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL,
                                  command=self._tree.yview)
        self._tree.configure(yscrollcommand=scrollbar.set)

        self._tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # Set up tooltips for data dictionary
        self._tooltip = TreeviewTooltip(self._tree)
        self._tooltip.set_tooltip_getter(self._get_item_tooltip)

        # Bind events
        self._tree.bind("<Button-3>", self._show_context_menu)
        self._tree.bind("<Double-1>", self._on_double_click)
        self._tree.bind("<<TreeviewSelect>>", self._on_select)

        # Context menu
        self._context_menu = tk.Menu(self, tearoff=0)
        self._context_menu.add_command(label="Insert SELECT *", command=self._insert_select_all)
        self._context_menu.add_command(label="Insert Column Name", command=self._insert_column)
        self._context_menu.add_separator()
        self._context_menu.add_command(label="Preview Data", command=self._preview_data)
        self._context_menu.add_command(label="Show Full Schema", command=self._show_schema)
        self._context_menu.add_separator()
        self._context_menu.add_command(label="Drop Table", command=self._unload_selected)

        # Info label
        self._info_label = ttk.Label(self, text="No tables loaded", foreground="gray")
        self._info_label.pack(anchor=tk.W, padx=5, pady=2)

        # Callback for inserting text into query editor
        self._insert_callback: Optional[Callable[[str], None]] = None

    def set_insert_callback(self, callback: Callable[[str], None]) -> None:
        """Set callback for inserting text into query editor.

        Args:
            callback: Function that takes a string to insert.
        """
        self._insert_callback = callback

    def _get_item_tooltip(self, item_id: str) -> Optional[str]:
        """Get tooltip text for a treeview item.

        Args:
            item_id: The treeview item ID.

        Returns:
            Tooltip text or None.
        """
        if not item_id:
            return None

        tags = self._tree.item(item_id, "tags")
        values = self._tree.item(item_id, "values")

        if not values:
            return None

        if "table" in tags:
            # Table node - show table description
            table_name = values[0]
            table_desc = get_table_description(table_name)
            if table_desc:
                return f"{table_name}\n\n{table_desc}"
            return None

        elif "column" in tags and len(values) >= 3:
            # Column node - show column description
            table_name = values[0]
            col_name = values[1]
            col_type = values[2]
            col_desc = get_column_description(table_name, col_name)
            if col_desc:
                return f"{col_name} ({col_type})\n\n{col_desc}"
            return None

        return None

    def _load_csv(self) -> None:
        """Open file dialog and load selected CSV."""
        initial_dir = self._config.last_open_directory or str(Path.home())

        filetypes = [
            ("CSV files", "*.csv"),
            ("All files", "*.*")
        ]

        path = filedialog.askopenfilename(
            title="Select CSV File",
            initialdir=initial_dir,
            filetypes=filetypes
        )

        if path:
            self._load_file(path)

    def _load_file(self, path: str) -> None:
        """Load a CSV file.

        Args:
            path: Path to the CSV file.
        """
        # Update last directory
        self._config.last_open_directory = str(Path(path).parent)
        self._config.add_recent_file(path)
        self._update_recent_menu()

        # Load into database
        table_info = self._db.load_csv(path)
        if table_info:
            self._update_tables_list()
            if self._on_table_change:
                self._on_table_change()

    def _update_recent_menu(self) -> None:
        """Update the recent files dropdown."""
        recent = self._config.recent_files
        if recent:
            # Show just filenames in dropdown
            display_names = [Path(p).name for p in recent]
            self._recent_menu["values"] = display_names
        else:
            self._recent_menu["values"] = ["(No recent files)"]

    def _on_recent_selected(self, event) -> None:
        """Handle recent file selection."""
        selection = self._recent_menu.current()
        if selection >= 0 and selection < len(self._config.recent_files):
            path = self._config.recent_files[selection]
            if Path(path).exists():
                self._load_file(path)
            else:
                messagebox.showwarning(
                    "File Not Found",
                    f"File no longer exists:\n{path}"
                )
        # Reset the dropdown display
        self._recent_var.set("Recent Files")

    def _refresh_tables(self) -> None:
        """Refresh the table list from the database."""
        self._update_tables_list()
        if self._on_table_change:
            self._on_table_change()

    def _reload_all_csvs(self) -> None:
        """Reload all CSV files from their original paths."""
        tables = self._db.get_tables()

        # Find tables that were loaded from CSV files
        csv_tables = [
            t for t in tables
            if t.path and t.path != "(database)" and t.path.lower().endswith(".csv")
        ]

        if not csv_tables:
            messagebox.showinfo(
                "Reload All CSVs",
                "No CSV files to reload.\n\n"
                "Only tables loaded from CSV files can be reloaded."
            )
            return

        # Check which files still exist
        valid_tables = []
        missing_files = []
        for table in csv_tables:
            if Path(table.path).exists():
                valid_tables.append(table)
            else:
                missing_files.append(table.name)

        if missing_files:
            msg = f"The following files no longer exist and will be skipped:\n\n"
            msg += "\n".join(f"  - {name}" for name in missing_files)
            messagebox.showwarning("Missing Files", msg)

        if not valid_tables:
            messagebox.showwarning(
                "Reload All CSVs",
                "No valid CSV files to reload."
            )
            return

        # Confirm reload
        if not messagebox.askyesno(
            "Reload All CSVs",
            f"Reload {len(valid_tables)} CSV file(s)?\n\n"
            "This will re-import the data from disk,\n"
            "replacing the current table contents."
        ):
            return

        # Reload each CSV
        reloaded = 0
        for table in valid_tables:
            result = self._db.import_csv_to_table(table.path, table.name)
            if result:
                reloaded += 1

        # Update UI
        self._update_tables_list()
        if self._on_table_change:
            self._on_table_change()

        messagebox.showinfo(
            "Reload Complete",
            f"Successfully reloaded {reloaded} of {len(valid_tables)} CSV file(s)."
        )

    def _update_tables_list(self) -> None:
        """Update the tables tree with tables and their columns."""
        # Clear existing items
        for item in self._tree.get_children():
            self._tree.delete(item)

        tables = self._db.get_tables()

        for table_info in tables:
            # Add table as parent node with icon indicator
            table_display = f"📊 {table_info.name} ({table_info.row_count:,} rows)"
            table_id = self._tree.insert(
                "", "end", text=table_display,
                open=False,  # Collapsed by default
                tags=("table",),
                values=(table_info.name,)
            )

            # Add columns as children
            for col_name, col_type in table_info.columns:
                col_display = f"  📋 {col_name} ({col_type})"
                self._tree.insert(
                    table_id, "end", text=col_display,
                    tags=("column",),
                    values=(table_info.name, col_name, col_type)
                )

        # Update info label
        if tables:
            total_rows = sum(t.row_count for t in tables)
            self._info_label.config(
                text=f"{len(tables)} table(s), {total_rows:,} total rows",
                foreground="black"
            )
        else:
            self._info_label.config(text="No tables loaded", foreground="gray")

    def _on_select(self, event) -> None:
        """Handle selection change."""
        pass  # Could update status or enable/disable buttons

    def _on_double_click(self, event) -> None:
        """Handle double-click - insert name into query editor."""
        item = self._tree.focus()
        if not item:
            return

        tags = self._tree.item(item, "tags")
        values = self._tree.item(item, "values")

        if not self._insert_callback:
            return

        if "table" in tags and values:
            # Insert table name
            table_name = values[0]
            self._insert_callback(f'"{table_name}"')
        elif "column" in tags and len(values) >= 2:
            # Insert column name
            col_name = values[1]
            self._insert_callback(f'"{col_name}"')

    def _show_context_menu(self, event) -> None:
        """Show context menu on right-click."""
        # Select item under cursor
        item = self._tree.identify_row(event.y)
        if item:
            self._tree.selection_set(item)
            self._tree.focus(item)
            self._context_menu.post(event.x_root, event.y_root)

    def _get_selected_table(self) -> Optional[TableInfo]:
        """Get the table info for the selected item (table or column).

        Returns:
            TableInfo or None if nothing valid selected.
        """
        item = self._tree.focus()
        if not item:
            return None

        tags = self._tree.item(item, "tags")
        values = self._tree.item(item, "values")

        if not values:
            return None

        # Get table name (first value for both table and column items)
        table_name = values[0]
        return self._db.get_table(table_name)

    def _insert_select_all(self) -> None:
        """Insert SELECT * FROM table query."""
        table_info = self._get_selected_table()
        if table_info and self._insert_callback:
            query = f'SELECT * FROM "{table_info.name}" LIMIT 100'
            self._insert_callback(query)

    def _insert_column(self) -> None:
        """Insert the selected column name."""
        item = self._tree.focus()
        if not item:
            return

        tags = self._tree.item(item, "tags")
        values = self._tree.item(item, "values")

        if "column" in tags and len(values) >= 2 and self._insert_callback:
            col_name = values[1]
            self._insert_callback(f'"{col_name}"')
        elif "table" in tags and values and self._insert_callback:
            table_name = values[0]
            self._insert_callback(f'"{table_name}"')

    def _show_schema(self) -> None:
        """Show schema for selected table."""
        table_info = self._get_selected_table()
        if table_info:
            schema_text = f"Table: {table_info.name}\n"
            schema_text += f"Source: {table_info.path}\n"
            schema_text += f"Rows: {table_info.row_count:,}\n\n"
            schema_text += "Columns:\n"
            for col_name, col_type in table_info.columns:
                schema_text += f"  {col_name}: {col_type}\n"

            messagebox.showinfo("Table Schema", schema_text)

    def _preview_data(self) -> None:
        """Preview data for selected table."""
        table_info = self._get_selected_table()
        if table_info:
            df = self._db.get_sample(table_info.name, limit=10)
            if df is not None:
                preview = df.to_string()
                self._show_preview_dialog(table_info.name, preview)

    def _show_preview_dialog(self, table_name: str, content: str) -> None:
        """Show a preview dialog with scrollable text.

        Args:
            table_name: Name of the table.
            content: Content to display.
        """
        dialog = tk.Toplevel(self)
        dialog.title(f"Preview: {table_name}")
        dialog.geometry("800x400")

        text = tk.Text(dialog, wrap=tk.NONE, font=("Consolas", 10))
        text.insert("1.0", content)
        text.config(state=tk.DISABLED)

        scroll_y = ttk.Scrollbar(dialog, orient=tk.VERTICAL, command=text.yview)
        scroll_x = ttk.Scrollbar(dialog, orient=tk.HORIZONTAL, command=text.xview)
        text.configure(yscrollcommand=scroll_y.set, xscrollcommand=scroll_x.set)

        text.grid(row=0, column=0, sticky="nsew")
        scroll_y.grid(row=0, column=1, sticky="ns")
        scroll_x.grid(row=1, column=0, sticky="ew")

        dialog.grid_rowconfigure(0, weight=1)
        dialog.grid_columnconfigure(0, weight=1)

        ttk.Button(dialog, text="Close", command=dialog.destroy).grid(
            row=2, column=0, columnspan=2, pady=5
        )

    def _unload_selected(self) -> None:
        """Unload the selected table."""
        table_info = self._get_selected_table()
        if table_info:
            if messagebox.askyesno(
                "Confirm Drop",
                f"Drop table '{table_info.name}'?\n\n"
                "This will remove it from the database."
            ):
                self._db.unload_table(table_info.name)
                self._update_tables_list()
                if self._on_table_change:
                    self._on_table_change()

    def get_table_names(self) -> List[str]:
        """Get list of loaded table names.

        Returns:
            List of table names.
        """
        return [t.name for t in self._db.get_tables()]
