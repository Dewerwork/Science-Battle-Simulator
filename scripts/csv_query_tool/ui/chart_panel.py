"""Chart panel for visualizing query results."""

import tkinter as tk
from tkinter import ttk, messagebox
from typing import Optional, List
import pandas as pd

try:
    import matplotlib
    matplotlib.use("TkAgg")
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
    from matplotlib.figure import Figure
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False


class ChartPanel(ttk.Frame):
    """Panel for creating charts from query results."""

    def __init__(self, parent: tk.Widget):
        """Initialize chart panel.

        Args:
            parent: Parent widget.
        """
        super().__init__(parent)
        self._data: Optional[pd.DataFrame] = None

        if not HAS_MATPLOTLIB:
            self._setup_no_matplotlib()
        else:
            self._setup_ui()

    def _setup_no_matplotlib(self) -> None:
        """Show message when matplotlib is not available."""
        label = ttk.Label(
            self,
            text="Matplotlib is not installed.\n\n"
                 "Install with: pip install matplotlib\n\n"
                 "Then restart the application.",
            justify=tk.CENTER
        )
        label.pack(expand=True)

    def _setup_ui(self) -> None:
        """Set up the UI components."""
        # Controls frame
        controls = ttk.Frame(self)
        controls.pack(fill=tk.X, padx=5, pady=5)

        # Chart type
        ttk.Label(controls, text="Chart Type:").pack(side=tk.LEFT, padx=2)
        self._chart_type = ttk.Combobox(
            controls,
            values=["Bar", "Scatter", "Histogram", "Line", "Box"],
            state="readonly", width=12
        )
        self._chart_type.set("Bar")
        self._chart_type.pack(side=tk.LEFT, padx=2)

        # X axis
        ttk.Label(controls, text="X:").pack(side=tk.LEFT, padx=(10, 2))
        self._x_var = ttk.Combobox(controls, state="readonly", width=20)
        self._x_var.pack(side=tk.LEFT, padx=2)

        # Y axis
        ttk.Label(controls, text="Y:").pack(side=tk.LEFT, padx=(10, 2))
        self._y_var = ttk.Combobox(controls, state="readonly", width=20)
        self._y_var.pack(side=tk.LEFT, padx=2)

        # Plot button
        self._plot_btn = ttk.Button(
            controls, text="Plot", command=self._create_chart
        )
        self._plot_btn.pack(side=tk.LEFT, padx=10)

        # Clear button
        self._clear_btn = ttk.Button(
            controls, text="Clear", command=self._clear_chart
        )
        self._clear_btn.pack(side=tk.LEFT, padx=2)

        # Limit for categorical plots
        ttk.Label(controls, text="Limit:").pack(side=tk.LEFT, padx=(10, 2))
        self._limit_var = tk.StringVar(value="20")
        self._limit_entry = ttk.Entry(controls, textvariable=self._limit_var, width=6)
        self._limit_entry.pack(side=tk.LEFT, padx=2)

        # Chart frame
        chart_frame = ttk.Frame(self)
        chart_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Create matplotlib figure
        self._figure = Figure(figsize=(8, 5), dpi=100)
        self._ax = self._figure.add_subplot(111)

        self._canvas = FigureCanvasTkAgg(self._figure, chart_frame)
        self._canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Toolbar
        toolbar_frame = ttk.Frame(chart_frame)
        toolbar_frame.pack(fill=tk.X)
        self._toolbar = NavigationToolbar2Tk(self._canvas, toolbar_frame)
        self._toolbar.update()

    def set_data(self, df: Optional[pd.DataFrame]) -> None:
        """Set the data for charting.

        Args:
            df: DataFrame to use for charts.
        """
        self._data = df

        if not HAS_MATPLOTLIB:
            return

        if df is None or df.empty:
            self._x_var["values"] = []
            self._y_var["values"] = []
            return

        columns = list(df.columns)
        self._x_var["values"] = columns
        self._y_var["values"] = columns

        # Auto-select first columns if available
        if columns:
            self._x_var.set(columns[0])
            if len(columns) > 1:
                self._y_var.set(columns[1])
            else:
                self._y_var.set(columns[0])

    def _create_chart(self) -> None:
        """Create the selected chart."""
        if not HAS_MATPLOTLIB or self._data is None:
            return

        chart_type = self._chart_type.get()
        x_col = self._x_var.get()
        y_col = self._y_var.get()

        if not x_col:
            messagebox.showwarning("Chart Error", "Please select an X column")
            return

        try:
            limit = int(self._limit_var.get())
        except ValueError:
            limit = 20

        self._ax.clear()

        try:
            if chart_type == "Bar":
                self._create_bar_chart(x_col, y_col, limit)
            elif chart_type == "Scatter":
                self._create_scatter_chart(x_col, y_col)
            elif chart_type == "Histogram":
                self._create_histogram(x_col)
            elif chart_type == "Line":
                self._create_line_chart(x_col, y_col)
            elif chart_type == "Box":
                self._create_box_chart(y_col)

            self._figure.tight_layout()
            self._canvas.draw()

        except Exception as e:
            messagebox.showerror("Chart Error", f"Could not create chart:\n{e}")

    def _create_bar_chart(self, x_col: str, y_col: str, limit: int) -> None:
        """Create a bar chart.

        Args:
            x_col: X-axis column (categories).
            y_col: Y-axis column (values).
            limit: Maximum number of bars.
        """
        data = self._data.head(limit)

        x_vals = data[x_col].astype(str)
        y_vals = pd.to_numeric(data[y_col], errors="coerce")

        self._ax.bar(range(len(x_vals)), y_vals, color="steelblue")
        self._ax.set_xticks(range(len(x_vals)))
        self._ax.set_xticklabels(x_vals, rotation=45, ha="right")
        self._ax.set_xlabel(x_col)
        self._ax.set_ylabel(y_col)
        self._ax.set_title(f"{y_col} by {x_col}")

    def _create_scatter_chart(self, x_col: str, y_col: str) -> None:
        """Create a scatter plot.

        Args:
            x_col: X-axis column.
            y_col: Y-axis column.
        """
        x_vals = pd.to_numeric(self._data[x_col], errors="coerce")
        y_vals = pd.to_numeric(self._data[y_col], errors="coerce")

        self._ax.scatter(x_vals, y_vals, alpha=0.6, c="steelblue")
        self._ax.set_xlabel(x_col)
        self._ax.set_ylabel(y_col)
        self._ax.set_title(f"{y_col} vs {x_col}")

    def _create_histogram(self, col: str) -> None:
        """Create a histogram.

        Args:
            col: Column to plot.
        """
        vals = pd.to_numeric(self._data[col], errors="coerce").dropna()

        self._ax.hist(vals, bins=30, color="steelblue", edgecolor="white")
        self._ax.set_xlabel(col)
        self._ax.set_ylabel("Frequency")
        self._ax.set_title(f"Distribution of {col}")

    def _create_line_chart(self, x_col: str, y_col: str) -> None:
        """Create a line chart.

        Args:
            x_col: X-axis column.
            y_col: Y-axis column.
        """
        # Sort by x column
        data = self._data.sort_values(x_col)
        x_vals = pd.to_numeric(data[x_col], errors="coerce")
        y_vals = pd.to_numeric(data[y_col], errors="coerce")

        self._ax.plot(x_vals, y_vals, color="steelblue", marker="o", markersize=3)
        self._ax.set_xlabel(x_col)
        self._ax.set_ylabel(y_col)
        self._ax.set_title(f"{y_col} over {x_col}")

    def _create_box_chart(self, col: str) -> None:
        """Create a box plot.

        Args:
            col: Column to plot.
        """
        vals = pd.to_numeric(self._data[col], errors="coerce").dropna()

        self._ax.boxplot(vals, vert=True)
        self._ax.set_ylabel(col)
        self._ax.set_title(f"Box Plot of {col}")

    def _clear_chart(self) -> None:
        """Clear the current chart."""
        if not HAS_MATPLOTLIB:
            return

        self._ax.clear()
        self._canvas.draw()
