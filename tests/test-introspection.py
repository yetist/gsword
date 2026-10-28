#!/usr/bin/env python3
"""Smoke test for the generated GObject Introspection data."""

import gi

gi.require_version("Gsw", "0.1")
from gi.repository import Gsw


def main():
    mgr = Gsw.VersificationMgr.get_default()
    system = mgr.get_system("NRSV")
    assert system is not None
    assert system.get_name() == "NRSV"
    assert system.get_book_count() == 66
    book = system.get_book(0)
    assert book.get_osis_name() == "Gen"
    print("GIR smoke test OK")


if __name__ == "__main__":
    main()
