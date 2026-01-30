"""Allow running as python -m csv_query_tool."""

# Handle imports for both module and frozen/direct execution
try:
    from .main import main
except ImportError:
    from main import main

if __name__ == "__main__":
    main()
