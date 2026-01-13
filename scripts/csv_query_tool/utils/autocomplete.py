"""SQL autocomplete functionality for query editor."""

import re
import tkinter as tk
from tkinter import ttk
from typing import Dict, List, Optional, Callable, Tuple


class SQLAliasParser:
    """Parses SQL queries to extract table aliases."""

    # Pattern to match table aliases in FROM/JOIN clauses
    # Matches: FROM table AS alias, FROM table alias, JOIN table AS alias, etc.
    ALIAS_PATTERN = re.compile(
        r'(?:FROM|JOIN)\s+'  # FROM or JOIN keyword
        r'["\']?(\w+)["\']?'  # Table name (with optional quotes)
        r'(?:\s+(?:AS\s+)?'  # Optional AS keyword
        r'(\w+))?',  # Alias
        re.IGNORECASE
    )

    # Pattern to match CTE aliases (WITH name AS (...))
    CTE_PATTERN = re.compile(
        r'WITH\s+(\w+)\s+AS\s*\(',
        re.IGNORECASE
    )

    @classmethod
    def extract_aliases(cls, sql: str) -> Dict[str, str]:
        """Extract table aliases from SQL query.

        Args:
            sql: The SQL query text.

        Returns:
            Dictionary mapping alias -> table_name.
            If no alias is specified, the table name itself is used as alias.
        """
        aliases = {}

        # Find all FROM/JOIN table references
        for match in cls.ALIAS_PATTERN.finditer(sql):
            table_name = match.group(1)
            alias = match.group(2) if match.group(2) else table_name
            aliases[alias.lower()] = table_name

        # Also handle CTEs - alias is the CTE name
        for match in cls.CTE_PATTERN.finditer(sql):
            cte_name = match.group(1)
            aliases[cte_name.lower()] = cte_name

        return aliases


class AutocompleteDropdown:
    """Autocomplete dropdown widget for text widgets."""

    def __init__(self, text_widget: tk.Text, enabled_var: Optional[tk.BooleanVar] = None):
        """Initialize autocomplete dropdown.

        Args:
            text_widget: The text widget to attach to.
            enabled_var: Optional BooleanVar to control whether autocomplete is enabled.
        """
        self._text = text_widget
        self._enabled_var = enabled_var
        self._dropdown: Optional[tk.Toplevel] = None
        self._listbox: Optional[tk.Listbox] = None
        self._suggestions: List[str] = []
        self._current_prefix: str = ""
        self._trigger_pos: str = ""
        self._get_suggestions: Optional[Callable[[str, str], List[str]]] = None

        # Bind events
        self._text.bind("<KeyRelease>", self._on_key_release)
        self._text.bind("<Key>", self._on_key_press)
        self._text.bind("<FocusOut>", self._hide_dropdown)
        self._text.bind("<Escape>", self._hide_dropdown)
        self._text.bind("<Button-1>", self._hide_dropdown)

    def set_suggestion_getter(
        self, getter: Callable[[str, str], List[str]]
    ) -> None:
        """Set the function that returns suggestions.

        Args:
            getter: Function that takes (full_query, current_word) and returns
                   list of suggestions.
        """
        self._get_suggestions = getter

    def is_enabled(self) -> bool:
        """Check if autocomplete is enabled."""
        if self._enabled_var:
            return self._enabled_var.get()
        return True

    def _on_key_press(self, event) -> Optional[str]:
        """Handle key press events."""
        if not self._dropdown or not self._listbox:
            return None

        if event.keysym == "Down":
            self._move_selection(1)
            return "break"
        elif event.keysym == "Up":
            self._move_selection(-1)
            return "break"
        elif event.keysym in ("Return", "Tab"):
            self._complete_selection()
            return "break"

        return None

    def _on_key_release(self, event) -> None:
        """Handle key release events."""
        if not self.is_enabled():
            self._hide_dropdown()
            return

        # Ignore navigation keys
        if event.keysym in ("Left", "Right", "Up", "Down", "Return",
                           "Tab", "Escape", "Shift_L", "Shift_R",
                           "Control_L", "Control_R", "Alt_L", "Alt_R"):
            return

        # Get current position and text
        cursor_pos = self._text.index(tk.INSERT)
        full_query = self._text.get("1.0", tk.END)

        # Find the current word being typed
        line_start = self._text.index(f"{cursor_pos} linestart")
        line_text = self._text.get(line_start, cursor_pos)

        # Check if we should show autocomplete (after a dot)
        current_word, trigger_pos = self._get_current_word(line_text, cursor_pos)

        if current_word is None:
            self._hide_dropdown()
            return

        self._current_prefix = current_word
        self._trigger_pos = trigger_pos

        # Get suggestions
        if self._get_suggestions:
            suggestions = self._get_suggestions(full_query, current_word)
        else:
            suggestions = []

        if suggestions:
            self._show_dropdown(suggestions)
        else:
            self._hide_dropdown()

    def _get_current_word(self, line_text: str, cursor_pos: str) -> Tuple[Optional[str], str]:
        """Get the current word being typed and trigger position.

        Returns:
            Tuple of (current_word_after_dot, trigger_position) or (None, "") if not applicable.
        """
        # Look for alias.column pattern
        # Find the most recent dot in the line
        dot_pos = line_text.rfind(".")

        if dot_pos == -1:
            return None, ""

        # Get the alias before the dot
        before_dot = line_text[:dot_pos]
        after_dot = line_text[dot_pos + 1:]

        # Check if after_dot is a valid partial column name (alphanumeric/underscore)
        if after_dot and not re.match(r'^[\w]*$', after_dot):
            return None, ""

        # Get the alias (last word before the dot)
        alias_match = re.search(r'(\w+)$', before_dot)
        if not alias_match:
            return None, ""

        alias = alias_match.group(1)

        # Calculate trigger position (position of the dot)
        line_num = cursor_pos.split(".")[0]
        col_of_dot = len(before_dot)
        trigger_pos = f"{line_num}.{col_of_dot + 1}"  # Position after the dot

        # Return the prefix after dot and the alias for context
        # We encode the alias in the current_word using a special format
        return f"{alias}:{after_dot}", trigger_pos

    def _show_dropdown(self, suggestions: List[str]) -> None:
        """Show the autocomplete dropdown."""
        self._suggestions = suggestions

        if self._dropdown:
            # Update existing dropdown
            self._listbox.delete(0, tk.END)
            for s in suggestions:
                self._listbox.insert(tk.END, s)
            if suggestions:
                self._listbox.selection_set(0)
            return

        # Create dropdown window
        self._dropdown = tk.Toplevel(self._text)
        self._dropdown.wm_overrideredirect(True)

        # Position below cursor
        cursor_pos = self._text.index(tk.INSERT)
        x, y, _, h = self._text.bbox(cursor_pos)
        x += self._text.winfo_rootx()
        y += self._text.winfo_rooty() + h

        self._dropdown.wm_geometry(f"+{x}+{y}")

        # Create listbox
        frame = ttk.Frame(self._dropdown, relief=tk.SOLID, borderwidth=1)
        frame.pack(fill=tk.BOTH, expand=True)

        self._listbox = tk.Listbox(
            frame,
            width=40,
            height=min(8, len(suggestions)),
            selectmode=tk.SINGLE,
            exportselection=False
        )
        self._listbox.pack(fill=tk.BOTH, expand=True)

        scrollbar = ttk.Scrollbar(
            frame, orient=tk.VERTICAL, command=self._listbox.yview
        )
        if len(suggestions) > 8:
            scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self._listbox.configure(yscrollcommand=scrollbar.set)

        # Populate listbox
        for s in suggestions:
            self._listbox.insert(tk.END, s)

        if suggestions:
            self._listbox.selection_set(0)

        # Bind listbox events
        self._listbox.bind("<Double-1>", lambda e: self._complete_selection())
        self._listbox.bind("<Return>", lambda e: self._complete_selection())

    def _hide_dropdown(self, event=None) -> None:
        """Hide the autocomplete dropdown."""
        if self._dropdown:
            self._dropdown.destroy()
            self._dropdown = None
            self._listbox = None
            self._suggestions = []

    def _move_selection(self, delta: int) -> None:
        """Move selection in the listbox."""
        if not self._listbox:
            return

        current = self._listbox.curselection()
        if not current:
            new_idx = 0 if delta > 0 else len(self._suggestions) - 1
        else:
            new_idx = current[0] + delta
            new_idx = max(0, min(new_idx, len(self._suggestions) - 1))

        self._listbox.selection_clear(0, tk.END)
        self._listbox.selection_set(new_idx)
        self._listbox.see(new_idx)

    def _complete_selection(self) -> None:
        """Complete the selected suggestion."""
        if not self._listbox or not self._suggestions:
            self._hide_dropdown()
            return

        selection = self._listbox.curselection()
        if not selection:
            self._hide_dropdown()
            return

        selected = self._suggestions[selection[0]]

        # Extract just the column name (remove type info)
        col_name = selected.split(" ")[0] if " " in selected else selected

        # Delete the current prefix and insert the completion
        if self._trigger_pos:
            cursor_pos = self._text.index(tk.INSERT)
            self._text.delete(self._trigger_pos, cursor_pos)
            self._text.insert(self._trigger_pos, col_name)

        self._hide_dropdown()
        self._text.focus_set()


class QueryAutocomplete:
    """Manages SQL autocomplete with table/column awareness."""

    def __init__(self, text_widget: tk.Text, enabled_var: Optional[tk.BooleanVar] = None):
        """Initialize query autocomplete.

        Args:
            text_widget: The text widget to attach to.
            enabled_var: Optional BooleanVar to control whether autocomplete is enabled.
        """
        self._dropdown = AutocompleteDropdown(text_widget, enabled_var)
        self._dropdown.set_suggestion_getter(self._get_suggestions)
        self._table_columns: Dict[str, List[Tuple[str, str]]] = {}

    def set_table_columns(self, table_columns: Dict[str, List[Tuple[str, str]]]) -> None:
        """Set the available table columns.

        Args:
            table_columns: Dict mapping table_name -> [(col_name, col_type), ...]
        """
        self._table_columns = table_columns

    def update_table_columns(self, table_columns: Dict[str, List[Tuple[str, str]]]) -> None:
        """Update the available table columns (same as set_table_columns).

        Args:
            table_columns: Dict mapping table_name -> [(col_name, col_type), ...]
        """
        self._table_columns = table_columns

    def _get_suggestions(self, full_query: str, current_word: str) -> List[str]:
        """Get autocomplete suggestions.

        Args:
            full_query: The full SQL query text.
            current_word: The current word being typed (format: "alias:prefix").

        Returns:
            List of suggestions.
        """
        if ":" not in current_word:
            return []

        alias, prefix = current_word.split(":", 1)
        prefix_lower = prefix.lower()

        # Extract aliases from the query
        aliases = SQLAliasParser.extract_aliases(full_query)

        # Find the table for this alias
        table_name = aliases.get(alias.lower())
        if not table_name:
            return []

        # Get columns for the table
        columns = self._table_columns.get(table_name)
        if not columns:
            # Try case-insensitive match
            for name, cols in self._table_columns.items():
                if name.lower() == table_name.lower():
                    columns = cols
                    break

        if not columns:
            return []

        # Filter columns by prefix
        suggestions = []
        for col_name, col_type in columns:
            if col_name.lower().startswith(prefix_lower):
                suggestions.append(f"{col_name} ({col_type})")

        return suggestions
