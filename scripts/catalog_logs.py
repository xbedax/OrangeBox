"""Compatibility entry point for the stable firmware log catalogue.

Use export_log_messages.py; IDs are never assigned by call-site order.
"""
from export_log_messages import main

if __name__ == "__main__":
    main()
