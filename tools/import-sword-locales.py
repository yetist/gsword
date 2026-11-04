#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com>
"""Import selected SWORD locales into libgsword's native locale format.

The generated .conf files are normal GKeyFile documents:

    [Locale]
    Format=1
    Id=zh-Hans
    ...
    [Book Gen]
    Name=...
    Abbrev=...
    Aliases=...
"""

from __future__ import annotations

import argparse
import collections
import re
import sys
import unicodedata
from pathlib import Path

TARGETS = {
    "en": {
        "name": "English",
        "language": "en",
        "aliases": ["C", "en_US", "en-US"],
        "fallback": "",
        "sources": ["abbr.conf"],
    },
    "zh-Hans": {
        "name": "简体中文",
        "language": "zh-Hans",
        "aliases": ["zh_Hans", "zh_CN", "zh-CN"],
        "fallback": "en",
        "sources": ["zh_Hans-utf8.conf"],
    },
    "zh-Hant": {
        "name": "繁體中文",
        "language": "zh-Hant",
        "aliases": ["zh_Hant", "zh_TW", "zh-TW", "zh_HK", "zh-HK"],
        "fallback": "en",
        "sources": ["zh_Hant-utf8.conf"],
    },
    "de": {
        "name": "Deutsch",
        "language": "de",
        "aliases": ["de_DE", "de-DE"],
        "fallback": "en",
        "sources": ["de-utf8.conf"],
    },
    "fr": {
        "name": "Français",
        "language": "fr",
        "aliases": ["fr_FR", "fr-FR"],
        "fallback": "en",
        "sources": ["fr-utf8.conf"],
    },
    "es": {
        "name": "Español",
        "language": "es",
        "aliases": ["es_ES", "es-ES"],
        "fallback": "en",
        "sources": ["es-utf8.conf"],
    },
    "pt": {
        "name": "Português",
        "language": "pt",
        "aliases": ["pt_PT", "pt-PT"],
        "fallback": "en",
        "sources": ["pt-utf8.conf"],
    },
    "ru": {
        "name": "Русский",
        "language": "ru",
        "aliases": ["ru_RU", "ru-RU"],
        "fallback": "en",
        "sources": ["ru-utf8.conf"],
    },
    "uk": {
        "name": "Українська",
        "language": "uk",
        "aliases": ["uk_UA", "uk-UA"],
        "fallback": "en",
        "sources": ["uk-utf8.conf"],
    },
    "pl": {
        "name": "Polski",
        "language": "pl",
        "aliases": ["pl_PL", "pl-PL"],
        "fallback": "en",
        "sources": ["pl-utf8.conf"],
    },
    "ko": {
        "name": "한국어",
        "language": "ko",
        "aliases": ["ko_KR", "ko-KR"],
        "fallback": "en",
        "sources": ["ko-utf8.conf"],
    },
}

FORMAT_VERSION = 1
LOCALE_VERSION = "2026.10"

CANON_RE = re.compile(
    r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*(-?\d+)\s*\}'
)
ABBREV_RE = re.compile(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}')


def norm(text: str) -> str:
    text = unicodedata.normalize("NFC", text)
    text = " ".join(text.casefold().split())
    return text


def escape_value(text: str) -> str:
    return (text.replace("\\", "\\\\").replace("\n", "\\n")
            .replace("\r", "\\r").replace("\t", "\\t"))


def escape_list_item(text: str) -> str:
    return escape_value(text).replace(";", "\\;")


def write_value(lines: list[str], key: str, value: str) -> None:
    lines.append(f"{key}={escape_value(value)}")


def write_list(lines: list[str], key: str, values: list[str]) -> None:
    escaped = ";".join(escape_list_item(value) for value in values)
    lines.append(f"{key}={escaped}")


def parse_sword_conf(path: Path) -> dict[str, dict[str, str]]:
    raw = path.read_bytes()
    try:
        text = raw.decode("utf-8-sig")
    except UnicodeDecodeError:
        text = raw.decode("latin-1")

    sections: dict[str, dict[str, str]] = {}
    current = ""
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].strip()
            sections.setdefault(current, {})
            continue
        if "=" not in line or not current:
            continue
        key, value = line.split("=", 1)
        sections[current][key.strip()] = value.strip()
    return sections


def parse_canon(path: Path):
    books: list[tuple[str, str, str]] = []
    seen: set[str] = set()
    text = path.read_text(encoding="utf-8", errors="replace")
    for match in CANON_RE.finditer(text):
        name, osis, pref = match.group(1), match.group(2), match.group(3)
        if not osis or osis in seen:
            continue
        seen.add(osis)
        books.append((name, osis, pref))
    return books


def parse_builtin_abbrevs(path: Path) -> list[tuple[str, str]]:
    entries: list[tuple[str, str]] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    for match in ABBREV_RE.finditer(text):
        abbrev, osis = match.group(1), match.group(2)
        if abbrev and osis:
            entries.append((abbrev, osis))
    return entries


def display_fallback_name(osis: str, aliases: list[str]) -> str:
    candidates = [alias for alias in aliases if alias.upper() == alias]
    if candidates:
        return max(candidates, key=len).title()
    return osis


def import_target(
    target: str,
    meta: dict,
    source_dir: Path,
    canon_books: list[tuple[str, str, str]],
    canon_by_osis: dict[str, tuple[str, str, str]],
    aliases_by_osis: dict[str, list[str]],
    name_to_osis: dict[str, str],
    pref_to_osis: dict[str, str],
) -> tuple[str, list[str]]:
    issues: list[str] = []
    config: dict[str, dict[str, str]] = {}
    source_names = meta["sources"]
    for source in source_names:
        path = source_dir / source
        if not path.exists():
            issues.append(f"missing source file: {source}")
            continue
        for section, values in parse_sword_conf(path).items():
            config.setdefault(section, {}).update(values)

    text = config.get("Text", {})
    book_abbrevs = config.get("Book Abbrevs", {})
    pref_abbrevs = config.get("Pref Abbrevs", {})

    localized_names: dict[str, str] = {}
    strings: dict[str, str] = {}
    for english, localized in text.items():
        osis = name_to_osis.get(norm(english))
        if osis is not None:
            localized_names[osis] = localized
        else:
            strings[english] = localized

    localized_prefs: dict[str, str] = {}
    for pref, localized in pref_abbrevs.items():
        osis = pref_to_osis.get(norm(pref))
        if osis is not None:
            localized_prefs[osis] = localized
        else:
            issues.append(f"unknown preferred abbreviation: {pref}")
    if target != "en" and not pref_abbrevs:
        issues.append("no [Pref Abbrevs]; using English preferred abbreviations")

    source_aliases: dict[str, list[str]] = collections.defaultdict(list)
    extra_osis: set[str] = set()
    for alias, osis in book_abbrevs.items():
        if osis not in canon_by_osis and osis not in aliases_by_osis:
            issues.append(f"unknown OSIS in Book Abbrevs: {osis}")
            continue
        source_aliases[osis].append(alias)
        if osis not in canon_by_osis:
            extra_osis.add(osis)

    if target == "en":
        english_prefs: dict[str, str] = {}
        for english, pref in text.items():
            osis = name_to_osis.get(norm(english))
            if osis is not None:
                english_prefs[osis] = pref

    ordered_osis = [book[1] for book in canon_books]
    ordered_osis.extend(sorted(extra_osis))
    seen: set[str] = set()

    lines: list[str] = []
    lines.append("# Generated baseline from SWORD 1.9.0 locale data.")
    lines.append("# Manual maintenance is allowed; --force is required to overwrite.")
    lines.append("")
    lines.append("[Locale]")
    write_value(lines, "Format", str(FORMAT_VERSION))
    write_value(lines, "Id", target)
    write_value(lines, "Name", meta["name"])
    write_value(lines, "Language", meta["language"])
    write_value(lines, "Direction", meta.get("direction", "ltr"))
    write_value(lines, "Version", LOCALE_VERSION)
    if meta.get("fallback"):
        write_value(lines, "Fallback", meta["fallback"])
    if meta.get("aliases"):
        write_list(lines, "Aliases", list(meta["aliases"]))

    if strings:
        lines.append("")
        lines.append("[Strings]")
        for key in sorted(strings):
            write_value(lines, key, strings[key])

    for osis in ordered_osis:
        if osis in seen:
            continue
        seen.add(osis)
        canon = canon_by_osis.get(osis)
        if target == "en":
            name = canon[0] if canon else display_fallback_name(
                osis, aliases_by_osis.get(osis, []))
            abbrev = english_prefs.get(osis)
            if not abbrev and canon:
                abbrev = canon[2]
            if not abbrev:
                candidates = aliases_by_osis.get(osis, [])
                abbrev = min(candidates, key=len) if candidates else osis
        else:
            name = localized_names.get(osis)
            if not name:
                name = canon[0] if canon else display_fallback_name(
                    osis, aliases_by_osis.get(osis, []))
                issues.append(f"missing localized name for {osis}")
            abbrev = localized_prefs.get(osis)
            if not abbrev:
                abbrev = canon[2] if canon else osis
                if pref_abbrevs:
                    issues.append(f"missing preferred abbreviation for {osis}")

        aliases = set(source_aliases.get(osis, []))
        if target == "en":
            aliases.update(aliases_by_osis.get(osis, []))
        aliases.add(osis)
        aliases.discard(name)
        aliases.discard(abbrev)
        aliases = sorted(aliases, key=lambda item: (norm(item), item))

        lines.append("")
        lines.append(f"[Book {osis}]")
        write_value(lines, "Name", name)
        write_value(lines, "Abbrev", abbrev)
        if aliases:
            write_list(lines, "Aliases", aliases)

    return "\n".join(lines) + "\n", issues


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source", type=Path,
        default=Path(__file__).resolve().parent.parent / "sword-1.9.0",
        help="SWORD source tree (contains locales.d and include)")
    parser.add_argument(
        "--output", type=Path,
        default=Path(__file__).resolve().parent.parent / "data" / "locales.d",
        help="output locale directory")
    parser.add_argument(
        "--only", nargs="*", choices=sorted(TARGETS),
        help="only generate the named locales")
    parser.add_argument(
        "--force", action="store_true",
        help="overwrite existing locale files (manual edits are otherwise kept)")
    args = parser.parse_args()

    source_dir = args.source / "locales.d"
    include_dir = args.source / "include"
    if not source_dir.is_dir():
        print(f"missing SWORD locales.d: {source_dir}", file=sys.stderr)
        return 1
    if not include_dir.is_dir():
        print(f"missing SWORD include directory: {include_dir}", file=sys.stderr)
        return 1

    canon_books = parse_canon(include_dir / "canon.h")
    if not canon_books:
        print("no books parsed from canon.h", file=sys.stderr)
        return 1
    builtin = parse_builtin_abbrevs(include_dir / "canon_abbrevs.h")

    canon_by_osis = {osis: (name, osis, pref)
                     for name, osis, pref in canon_books}
    aliases_by_osis: dict[str, list[str]] = collections.defaultdict(list)
    name_to_osis: dict[str, str] = {}
    pref_to_osis: dict[str, str] = {}

    for name, osis, pref in canon_books:
        name_to_osis[norm(name)] = osis
        pref_to_osis.setdefault(norm(pref), osis)
    for abbrev, osis in builtin:
        name_to_osis.setdefault(norm(abbrev), osis)
        pref_to_osis.setdefault(norm(abbrev), osis)
        aliases_by_osis[osis].append(abbrev)

    args.output.mkdir(parents=True, exist_ok=True)
    selected = args.only or list(TARGETS)
    for target in selected:
        path = args.output / f"{target}.conf"
        if path.exists() and not args.force:
            print(f"skip {path} (exists; use --force to overwrite)")
            continue
        content, issues = import_target(
            target, TARGETS[target], source_dir, canon_books, canon_by_osis,
            aliases_by_osis, name_to_osis, pref_to_osis)
        path.write_text(content, encoding="utf-8", newline="\n")
        print(f"wrote {path}")
        for issue in issues:
            print(f"  warning: {issue}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
