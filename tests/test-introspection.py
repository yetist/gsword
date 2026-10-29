#!/usr/bin/env python3
"""Smoke test for the generated GObject Introspection data."""

import gi

gi.require_version("GSword", "0.1")
from gi.repository import GSword


def main():
    mgr = GSword.VersificationMgr.get_default()
    system = mgr.get_system("NRSV")
    assert system is not None
    assert system.get_name() == "NRSV"
    assert system.get_book_count() == 66
    book = system.get_book(0)
    assert book.get_osis_name() == "Gen"
    assert hasattr(GSword.Module, "build_search_index")
    assert hasattr(GSword.Module, "delete_search_index")
    assert hasattr(GSword.Module, "has_search_index")
    print("GIR smoke test OK")


if __name__ == "__main__":
    main()
