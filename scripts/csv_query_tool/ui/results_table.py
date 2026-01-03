"""Results table component for displaying query results."""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from typing import Optional, List, Any
import pandas as pd

from ..utils.config import Config


class ResultsTable(ttk.Frame):
    """Table widget for displaying query results with pagination."""

    def __init__(self, parent: tk.Widget, config: Config):
        """Initialize results table.

        Args:
            parent: Parent widget.
            config: Application configuration.
        """
        super().__init__(parent)
        self._config = config
        self._data: Optional[pd.DataFrame] = None
        self._current_page = 0
        self._sort_column: Optional[str] = None
        self._sort_reverse = False

        self._setup_ui()

    def _setup_ui(self) -> None:
        """Set up the UI components."""
        # Toolbar frame
        toolbar = ttk.Frame(self)
        toolbar.pack(fill=tk.X, padx=5, pady=2)

        # Export button
        self._export_btn = ttk.Button(
            toolbar, text="Export CSV...",
            command=self._export_csv, state=tk.DISABLED
        )
        self._export_btn.pack(side=tk.LEFT, padx=2)

        # Copy button
        self._copy_btn = ttk.Button(
            toolbar, text="Copy All",
            command=self._copy_all, state=tk.DISABLED
        )
        self._copy_btn.pack(side=tk.LEFT, padx=2)

        # Pagination controls
        self._page_frame = ttk.Frame(toolbar)
        self._page_frame.pack(side=tk.RIGHT, padx=5)

        self._prev_btn = ttk.Button(
            self._page_frame, text="<", width=3,
            command=self._prev_page, state=tk.DISABLED
        )
        self._prev_btn.pack(side=tk.LEFT, padx=1)

        self._page_label = ttk.Label(self._page_frame, text="Page 0/0")
        self._page_label.pack(side=tk.LEFT, padx=5)

        self._next_btn = ttk.Button(
            self._page_frame, text=">", width=3,
            command=self._next_page, state=tk.DISABLED
        )
        self._next_btn.pack(side=tk.LEFT, padx=1)

        # Results info
        self._info_label = ttk.Label(toolbar, text="No results")
        self._info_label.pack(side=tk.LEFT, padx=10)

        # Treeview frame
        tree_frame = ttk.Frame(self)
        tree_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)

        # Treeview with scrollbars
        self._tree = ttk.Treeview(tree_frame, show="headings", selectmode="extended")

        scroll_y = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL,
                                 command=self._tree.yview)
        scroll_x = ttk.Scrollbar(tree_frame, orient=tk.HORIZONTAL,
                                 command=self._tree.xview)
        self._tree.configure(yscrollcommand=scroll_y.set,
                             xscrollcommand=scroll_x.set)

        self._tree.grid(row=0, column=0, sticky="nsew")
        scroll_y.grid(row=0, column=1, sticky="ns")
        scroll_x.grid(row=1, column=0, sticky="ew")

        tree_frame.grid_rowconfigure(0, weight=1)
        tree_frame.grid_columnconfigure(0, weight=1)

        # Context menu
        self._context_menu = tk.Menu(self, tearoff=0)
        self._context_menu.add_command(label="Copy Cell", command=self._copy_cell)
        self._context_menu.add_command(label="Copy Row", command=self._copy_row)
        self._context_menu.add_command(label="Copy Column", command=self._copy_column)

        self._tree.bind("<Button-3>", self._show_context_menu)

    def set_data(self, df: Optional[pd.DataFrame]) -> None:
        """Set the data to display.

        Args:
            df: DataFrame to display, or None to clear.
        """
        self._data = df
        self._current_page = 0
        self._sort_column = None
        self._sort_reverse = False

        if df is None or df.empty:
            self._clear_tree()
            self._info_label.config(text="No results")
            self._export_btn.config(state=tk.DISABLED)
            self._copy_btn.config(state=tk.DISABLED)
            return

        # Set up columns
        columns = list(df.columns)
        self._tree["columns"] = columns

        for col in columns:
            self._tree.heading(
                col, text=col,
                command=lambda c=col: self._sort_by_column(c)
            )
            # Set column width based on content
            max_width = max(
                len(str(col)),
                df[col].astype(str).str.len().max() if len(df) > 0 else 0
            )
            width = min(max(80, max_width * 8), 300)
            self._tree.column(col, width=width, minwidth=50)

        self._refresh_page()

        # Update controls
        self._export_btn.config(state=tk.NORMAL)
        self._copy_btn.config(state=tk.NORMAL)
        self._update_info()
        self._update_pagination()

    def _clear_tree(self) -> None:
        """Clear the treeview."""
        self._tree.delete(*self._tree.get_children())
        self._tree["columns"] = []

    def _refresh_page(self) -> None:
        """Refresh the current page display."""
        if self._data is None:
            return

        # Clear existing rows
        self._tree.delete(*self._tree.get_children())

        # Calculate page bounds
        page_size = self._config.results_page_size
        start = self._current_page * page_size
        end = min(start + page_size, len(self._data))

        # Get data slice
        data = self._data
        if self._sort_column and self._sort_column in data.columns:
            data = data.sort_values(
                by=self._sort_column,
                ascending=not self._sort_reverse,
                na_position="last"
            )

        page_data = data.iloc[start:end]

        # Insert rows
        for idx, row in page_data.iterrows():
            values = [self._format_value(v) for v in row.values]
            self._tree.insert("", tk.END, values=values)

        self._update_pagination()

    def _format_value(self, value: Any) -> str:
        """Format a value for display.

        Args:
            value: Value to format.

        Returns:
            Formatted string.
        """
        if pd.isna(value):
            return ""
        if isinstance(value, float):
            if value == int(value):
                return str(int(value))
            return f"{value:.4f}"
        return str(value)

    def _sort_by_column(self, column: str) -> None:
        """Sort by a column.

        Args:
            column: Column name to sort by.
        """
        if self._sort_column == column:
            self._sort_reverse = not self._sort_reverse
        else:
            self._sort_column = column
            self._sort_reverse = False

        self._refresh_page()

        # Update column headers to show sort indicator
        for col in self._tree["columns"]:
            text = col
            if col == self._sort_column:
                text = f"{col} {'v' if self._sort_reverse else '^'}"
            self._tree.heading(col, text=text)

    def _update_pagination(self) -> None:
        """Update pagination controls."""
        if self._data is None or len(self._data) == 0:
            self._prev_btn.config(state=tk.DISABLED)
            self._next_btn.config(state=tk.DISABLED)
            self._page_label.config(text="Page 0/0")
            return

        page_size = self._config.results_page_size
        total_pages = (len(self._data) - 1) // page_size + 1

        self._page_label.config(
            text=f"Page {self._current_page + 1}/{total_pages}"
        )

        self._prev_btn.config(
            state=tk.NORMAL if self._current_page > 0 else tk.DISABLED
        )
        self._next_btn.config(
            state=tk.NORMAL if self._current_page < total_pages - 1 else tk.DISABLED
        )

    def _update_info(self) -> None:
        """Update the info label."""
        if self._data is None:
            self._info_label.config(text="No results")
        else:
            self._info_label.config(
                text=f"{len(self._data):,} rows, {len(self._data.columns)} columns"
            )

    def _prev_page(self) -> None:
        """Go to previous page."""
        if self._current_page > 0:
            self._current_page -= 1
            self._refresh_page()

    def _next_page(self) -> None:
        """Go to next page."""
        if self._data is not None:
            page_size = self._config.results_page_size
            total_pages = (len(self._data) - 1) // page_size + 1
            if self._current_page < total_pages - 1:
                self._current_page += 1
                self._refresh_page()

    def _export_csv(self) -> None:
        """Export results to CSV."""
        if self._data is None:
            return

        initial_dir = self._config.last_export_directory or str(self._config.last_open_directory)

        path = filedialog.asksaveasfilename(
            title="Export Results",
            initialdir=initial_dir,
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")]
        )

        if path:
            try:
                self._data.to_csv(path, index=False)
                self._config.last_export_directory = str(path)
                messagebox.showinfo(
                    "Export Complete",
                    f"Exported {len(self._data):,} rows to:\n{path}"
                )
            except Exception as e:
                messagebox.showerror("Export Error", str(e))

    def _copy_all(self) -> None:
        """Copy all results to clipboard."""
        if self._data is None:
            return

        try:
            self._data.to_clipboard(index=False)
            messagebox.showinfo(
                "Copied",
                f"Copied {len(self._data):,} rows to clipboard"
            )
        except Exception as e:
            messagebox.showerror("Copy Error", str(e))

    def _show_context_menu(self, event) -> None:
        """Show context menu."""
        # Select row under cursor
        item = self._tree.identify_row(event.y)
        if item:
            self._tree.selection_set(item)
            self._selected_column = self._tree.identify_column(event.x)
            self._context_menu.post(event.x_root, event.y_root)

    def _copy_cell(self) -> None:
        """Copy selected cell to clipboard."""
        selection = self._tree.selection()
        if not selection or not hasattr(self, '_selected_column'):
            return

        try:
            col_idx = int(self._selected_column.replace("#", "")) - 1
            values = self._tree.item(selection[0])["values"]
            if 0 <= col_idx < len(values):
                self.clipboard_clear()
                self.clipboard_append(str(values[col_idx]))
        except (ValueError, IndexError):
            pass

    def _copy_row(self) -> None:
        """Copy selected row to clipboard."""
        selection = self._tree.selection()
        if not selection:
            return

        rows = []
        for item in selection:
            values = self._tree.item(item)["values"]
            rows.append("\t".join(str(v) for v in values))

        self.clipboard_clear()
        self.clipboard_append("\n".join(rows))

    def _copy_column(self) -> None:
        """Copy selected column to clipboard."""
        if not hasattr(self, '_selected_column') or self._data is None:
            return

        try:
            col_idx = int(self._selected_column.replace("#", "")) - 1
            columns = list(self._data.columns)
            if 0 <= col_idx < len(columns):
                col_name = columns[col_idx]
                values = self._data[col_name].astype(str).tolist()
                self.clipboard_clear()
                self.clipboard_append("\n".join(values))
        except (ValueError, IndexError):
            pass

    def get_data(self) -> Optional[pd.DataFrame]:
        """Get the current data.

        Returns:
            Current DataFrame or None.
        """
        return self._data
