"""Schema panel for displaying table schemas."""

import tkinter as tk
from tkinter import ttk
from typing import Dict, List, Tuple, Optional

# Handle imports for both module and frozen/direct execution
try:
    from ..database import DatabaseManager
except ImportError:
    from database import DatabaseManager


class SchemaPanel(ttk.Frame):
    """Panel for displaying database schema information."""

    def __init__(self, parent: tk.Widget, db_manager: DatabaseManager):
        """Initialize schema panel.

        Args:
            parent: Parent widget.
            db_manager: Database manager instance.
        """
        super().__init__(parent)
        self._db = db_manager

        self._setup_ui()

    def _setup_ui(self) -> None:
        """Set up the UI components."""
        # Toolbar
        toolbar = ttk.Frame(self)
        toolbar.pack(fill=tk.X, padx=5, pady=5)

        self._refresh_btn = ttk.Button(
            toolbar, text="Refresh", command=self.refresh
        )
        self._refresh_btn.pack(side=tk.LEFT, padx=2)

        self._expand_btn = ttk.Button(
            toolbar, text="Expand All", command=self._expand_all
        )
        self._expand_btn.pack(side=tk.LEFT, padx=2)

        self._collapse_btn = ttk.Button(
            toolbar, text="Collapse All", command=self._collapse_all
        )
        self._collapse_btn.pack(side=tk.LEFT, padx=2)

        # Info label
        self._info_label = ttk.Label(toolbar, text="")
        self._info_label.pack(side=tk.RIGHT, padx=5)

        # Treeview frame
        tree_frame = ttk.Frame(self)
        tree_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)

        # Treeview for schema
        self._tree = ttk.Treeview(tree_frame, show="tree headings", selectmode="browse")
        self._tree["columns"] = ("type", "info")
        self._tree.heading("#0", text="Name")
        self._tree.heading("type", text="Type")
        self._tree.heading("info", text="Info")
        self._tree.column("#0", width=200, minwidth=100)
        self._tree.column("type", width=120, minwidth=80)
        self._tree.column("info", width=200, minwidth=100)

        scroll_y = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL,
                                 command=self._tree.yview)
        self._tree.configure(yscrollcommand=scroll_y.set)

        self._tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scroll_y.pack(side=tk.RIGHT, fill=tk.Y)

        # Context menu
        self._context_menu = tk.Menu(self, tearoff=0)
        self._context_menu.add_command(
            label="Copy Column Name", command=self._copy_column_name
        )
        self._context_menu.add_command(
            label="Insert SELECT *", command=self._insert_select_all
        )
        self._context_menu.add_command(
            label="Insert Column List", command=self._insert_column_list
        )

        self._tree.bind("<Button-3>", self._show_context_menu)
        self._tree.bind("<Double-1>", self._on_double_click)

        # Callback for inserting into query editor
        self._on_insert_query = None

    def set_insert_callback(self, callback) -> None:
        """Set callback for inserting text into query editor.

        Args:
            callback: Function that takes a string to insert.
        """
        self._on_insert_query = callback

    def refresh(self) -> None:
        """Refresh the schema display."""
        # Clear existing
        self._tree.delete(*self._tree.get_children())

        tables = self._db.get_tables()

        for table_info in tables:
            # Insert table node
            table_id = self._tree.insert(
                "", tk.END,
                text=f"  {table_info.name}",
                values=("TABLE", f"{table_info.row_count:,} rows"),
                open=True,
                tags=("table",)
            )

            # Insert columns
            for col_name, col_type in table_info.columns:
                self._tree.insert(
                    table_id, tk.END,
                    text=f"    {col_name}",
                    values=(col_type, ""),
                    tags=("column",)
                )

        # Update info
        total_cols = sum(len(t.columns) for t in tables)
        self._info_label.config(
            text=f"{len(tables)} tables, {total_cols} columns"
        )

        # Style tags
        self._tree.tag_configure("table", font=("", 10, "bold"))

    def _expand_all(self) -> None:
        """Expand all tree nodes."""
        for item in self._tree.get_children():
            self._tree.item(item, open=True)

    def _collapse_all(self) -> None:
        """Collapse all tree nodes."""
        for item in self._tree.get_children():
            self._tree.item(item, open=False)

    def _show_context_menu(self, event) -> None:
        """Show context menu."""
        item = self._tree.identify_row(event.y)
        if item:
            self._tree.selection_set(item)
            self._context_menu.post(event.x_root, event.y_root)

    def _get_selected_info(self) -> Tuple[Optional[str], Optional[str]]:
        """Get selected table and column names.

        Returns:
            Tuple of (table_name, column_name). Column may be None.
        """
        selection = self._tree.selection()
        if not selection:
            return None, None

        item = selection[0]
        tags = self._tree.item(item, "tags")
        text = self._tree.item(item, "text").strip()

        if "table" in tags:
            return text, None
        elif "column" in tags:
            # Get parent table
            parent = self._tree.parent(item)
            if parent:
                table_name = self._tree.item(parent, "text").strip()
                return table_name, text

        return None, None

    def _copy_column_name(self) -> None:
        """Copy selected column or table name to clipboard."""
        table_name, col_name = self._get_selected_info()
        if col_name:
            self.clipboard_clear()
            self.clipboard_append(col_name)
        elif table_name:
            self.clipboard_clear()
            self.clipboard_append(table_name)

    def _insert_select_all(self) -> None:
        """Insert SELECT * query for selected table."""
        table_name, _ = self._get_selected_info()
        if table_name and self._on_insert_query:
            query = f'SELECT * FROM "{table_name}" LIMIT 100'
            self._on_insert_query(query)

    def _insert_column_list(self) -> None:
        """Insert comma-separated column list for selected table."""
        table_name, _ = self._get_selected_info()
        if table_name:
            table_info = self._db.get_table(table_name)
            if table_info and self._on_insert_query:
                columns = ", ".join(f'"{c[0]}"' for c in table_info.columns)
                self._on_insert_query(columns)

    def _on_double_click(self, event) -> None:
        """Handle double-click on item."""
        table_name, col_name = self._get_selected_info()
        if col_name and self._on_insert_query:
            self._on_insert_query(f'"{col_name}"')
        elif table_name and self._on_insert_query:
            self._insert_select_all()
