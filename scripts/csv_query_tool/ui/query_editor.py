"""Query editor component with history, favorites, and keyboard shortcuts."""

import tkinter as tk
from tkinter import ttk, simpledialog, messagebox
from typing import Callable, Optional, List, Dict, Tuple

from ..utils.query_history import QueryHistory
from ..utils.favorites import QueryFavorites
from ..utils.autocomplete import QueryAutocomplete


class QueryEditor(ttk.Frame):
    """SQL query editor with run button, history, and favorites."""

    def __init__(self, parent: tk.Widget, history: QueryHistory,
                 favorites: Optional[QueryFavorites] = None,
                 on_execute: Optional[Callable[[str], None]] = None,
                 autocomplete_enabled: Optional[tk.BooleanVar] = None):
        """Initialize query editor.

        Args:
            parent: Parent widget.
            history: Query history manager.
            favorites: Optional favorites manager.
            on_execute: Callback when query should be executed.
            autocomplete_enabled: Optional BooleanVar to control autocomplete.
        """
        super().__init__(parent)
        self._history = history
        self._favorites = favorites or QueryFavorites()
        self._on_execute = on_execute
        self._autocomplete_enabled = autocomplete_enabled
        self._autocomplete: Optional[QueryAutocomplete] = None

        self._setup_ui()

    def _setup_ui(self) -> None:
        """Set up the UI components."""
        # Query label with resize hint
        label_frame = ttk.Frame(self)
        label_frame.pack(fill=tk.X, padx=5, pady=(5, 2))

        label = ttk.Label(label_frame, text="SQL Query:")
        label.pack(side=tk.LEFT)

        resize_hint = ttk.Label(
            label_frame,
            text="(Drag the border below to resize)",
            foreground="gray",
            font=("", 9)
        )
        resize_hint.pack(side=tk.RIGHT)

        # Text editor frame - use grid for better control
        editor_frame = ttk.Frame(self)
        editor_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)

        # Text widget with scrollbar - removed fixed height for resizability
        self._text = tk.Text(
            editor_frame,
            wrap=tk.NONE,
            font=("Consolas", 11),
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

        # Initialize autocomplete
        self._autocomplete = QueryAutocomplete(self._text, self._autocomplete_enabled)

        # Bind keyboard shortcuts
        self._text.bind("<Control-Return>", self._on_run)
        self._text.bind("<F5>", self._on_run)
        self._text.bind("<Control-l>", self._on_clear)
        self._text.bind("<Control-L>", self._on_clear)
        self._text.bind("<Control-s>", self._on_save_favorite)
        self._text.bind("<Control-S>", self._on_save_favorite)

        # Button frame - row 1
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

        # Separator
        ttk.Separator(btn_frame, orient=tk.VERTICAL).pack(
            side=tk.LEFT, padx=8, fill=tk.Y, pady=2
        )

        # Favorites dropdown
        ttk.Label(btn_frame, text="Favorites:").pack(side=tk.LEFT, padx=(0, 2))
        self._favorites_var = tk.StringVar(value="Select...")
        self._favorites_menu = ttk.Combobox(
            btn_frame, textvariable=self._favorites_var,
            state="readonly", width=25
        )
        self._favorites_menu.pack(side=tk.LEFT, padx=2)
        self._favorites_menu.bind("<<ComboboxSelected>>", self._on_favorite_selected)
        self._update_favorites_menu()

        # Save to favorites button
        self._save_fav_btn = ttk.Button(
            btn_frame, text="Save to Favorites",
            command=self._save_to_favorites
        )
        self._save_fav_btn.pack(side=tk.LEFT, padx=2)

        # Delete favorite button
        self._del_fav_btn = ttk.Button(
            btn_frame, text="Delete",
            command=self._delete_favorite
        )
        self._del_fav_btn.pack(side=tk.LEFT, padx=2)

        # History dropdown (moved to right side)
        ttk.Label(btn_frame, text="History:").pack(side=tk.RIGHT, padx=(10, 2))
        self._history_var = tk.StringVar(value="Select...")
        self._history_menu = ttk.Combobox(
            btn_frame, textvariable=self._history_var,
            state="readonly", width=30
        )
        self._history_menu.pack(side=tk.RIGHT, padx=2)
        self._history_menu.bind("<<ComboboxSelected>>", self._on_history_selected)
        self._update_history_menu()

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

    def _on_save_favorite(self, event=None) -> str:
        """Handle save favorite keyboard shortcut.

        Args:
            event: Tkinter event.

        Returns:
            'break' to prevent default handling.
        """
        self._save_to_favorites()
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

    def _update_favorites_menu(self) -> None:
        """Update the favorites dropdown."""
        names = self._favorites.get_names()
        if names:
            self._favorites_menu["values"] = names
        else:
            self._favorites_menu["values"] = ["(No favorites)"]

    def _truncate_query(self, query: str, max_len: int = 50) -> str:
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
        self._history_var.set("Select...")

    def _on_favorite_selected(self, event) -> None:
        """Handle favorite selection."""
        name = self._favorites_var.get()
        if name and name != "(No favorites)":
            fav = self._favorites.get(name)
            if fav:
                self.set_query(fav.query)
        # Don't reset - keep showing selected favorite name

    def _save_to_favorites(self) -> None:
        """Save current query to favorites."""
        query = self.get_query()
        if not query:
            messagebox.showwarning("Save Favorite", "No query to save.")
            return

        # Ask for name
        name = simpledialog.askstring(
            "Save to Favorites",
            "Enter a name for this query:",
            parent=self
        )

        if name:
            if self._favorites.add(name, query):
                self._update_favorites_menu()
                self._favorites_var.set(name)
                messagebox.showinfo("Saved", f"Query saved as '{name}'")
            else:
                # Name already exists - ask to overwrite
                if messagebox.askyesno(
                    "Overwrite?",
                    f"A favorite named '{name}' already exists.\nOverwrite it?"
                ):
                    self._favorites.update(name, query)
                    self._update_favorites_menu()
                    self._favorites_var.set(name)
                    messagebox.showinfo("Updated", f"Query '{name}' updated")

    def _delete_favorite(self) -> None:
        """Delete the selected favorite."""
        name = self._favorites_var.get()
        if not name or name == "(No favorites)" or name == "Select...":
            messagebox.showwarning("Delete Favorite", "No favorite selected.")
            return

        if messagebox.askyesno("Delete Favorite", f"Delete '{name}'?"):
            if self._favorites.remove(name):
                self._update_favorites_menu()
                self._favorites_var.set("Select...")
                messagebox.showinfo("Deleted", f"Favorite '{name}' deleted")

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

    def refresh_favorites(self) -> None:
        """Refresh the favorites dropdown."""
        self._update_favorites_menu()

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

    def get_favorites(self) -> QueryFavorites:
        """Get the favorites manager.

        Returns:
            QueryFavorites instance.
        """
        return self._favorites

    def update_table_columns(
        self, table_columns: Dict[str, List[Tuple[str, str]]]
    ) -> None:
        """Update the available table columns for autocomplete.

        Args:
            table_columns: Dict mapping table_name -> [(col_name, col_type), ...]
        """
        if self._autocomplete:
            self._autocomplete.update_table_columns(table_columns)
