#!/usr/bin/env python3
"""Validate libgsword native locale packs."""

from __future__ import annotations

import argparse
import collections
import re
import sys
import unicodedata
from pathlib import Path

BOOK_PREFIX = "Book "
VALID_FORMAT = "1"
VALID_DIRECTIONS = {"ltr", "rtl"}
OSIS_RE = re.compile(r"^[1-4]?[A-Za-z][A-Za-z0-9]*$")
CANON_RE = re.compile(
    r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*(-?\d+)\s*\}'
)
ABBREV_RE = re.compile(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}')


def norm(text: str) -> str:
    text = unicodedata.normalize("NFC", text)
    return " ".join(text.casefold().split())


def unescape(text: str) -> str:
    result: list[str] = []
    escaped = False
    for char in text:
        if escaped:
            if char == "n":
                result.append("\n")
            elif char == "r":
                result.append("\r")
            elif char == "t":
                result.append("\t")
            else:
                result.append(char)
            escaped = False
        elif char == "\\":
            escaped = True
        else:
            result.append(char)
    if escaped:
        result.append("\\")
    return "".join(result)


def split_list(text: str) -> list[str]:
    values: list[str] = []
    current: list[str] = []
    escaped = False
    for char in text:
        if escaped:
            current.append(char)
            escaped = False
        elif char == "\\":
            escaped = True
        elif char == ";":
            value = "".join(current).strip()
            if value:
                values.append(unescape(value))
            current = []
        else:
            current.append(char)
    value = "".join(current).strip()
    if value:
        values.append(unescape(value))
    return values


def parse_conf(path: Path) -> dict[str, dict[str, str]]:
    text = path.read_text(encoding="utf-8")
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
        sections[current][key.strip()] = unescape(value.strip())
    return sections


def parse_known_osis(sword_source: Path) -> set[str]:
    known: set[str] = set()
    canon = sword_source / "include" / "canon.h"
    abbrevs = sword_source / "include" / "canon_abbrevs.h"
    if canon.exists():
        for match in CANON_RE.finditer(
                canon.read_text(encoding="utf-8", errors="replace")):
            known.add(match.group(2))
    if abbrevs.exists():
        for match in ABBREV_RE.finditer(
                abbrevs.read_text(encoding="utf-8", errors="replace")):
            known.add(match.group(2))
    return known


def validate(directory: Path, known_osis: set[str]):
    errors: list[str] = []
    warnings: list[str] = []
    files = sorted(directory.glob("*.conf"))
    if not files:
        errors.append(f"no .conf files in {directory}")
        return errors, warnings, {}

    locales: dict[str, dict] = {}
    for path in files:
        try:
            sections = parse_conf(path)
        except (OSError, UnicodeError) as exc:
            errors.append(f"{path.name}: cannot read UTF-8 file: {exc}")
            continue

        meta = sections.get("Locale")
        if meta is None:
            errors.append(f"{path.name}: missing [Locale]")
            continue

        for key in ("Format", "Id", "Name", "Language"):
            if not meta.get(key):
                errors.append(f"{path.name}: missing Locale.{key}")
        if meta.get("Format") != VALID_FORMAT:
            errors.append(f"{path.name}: unsupported Format={meta.get('Format')}")
        locale_id = meta.get("Id", "")
        if not locale_id:
            continue
        if locale_id in locales:
            errors.append(f"{path.name}: duplicate locale Id {locale_id}")
        if path.stem != locale_id:
            errors.append(f"{path.name}: filename does not match Id={locale_id}")
        if meta.get("Direction", "ltr") not in VALID_DIRECTIONS:
            errors.append(f"{path.name}: invalid Direction={meta.get('Direction')}")

        aliases = split_list(meta.get("Aliases", ""))
        if len({norm(alias) for alias in aliases}) != len(aliases):
            errors.append(f"{path.name}: duplicate Locale.Aliases entry")

        alias_map: dict[str, set[str]] = collections.defaultdict(set)
        abbrev_map: dict[str, list[str]] = collections.defaultdict(list)
        books: dict[str, dict] = {}
        for section, values in sections.items():
            if not section.startswith(BOOK_PREFIX):
                continue
            osis = section[len(BOOK_PREFIX):].strip()
            if not OSIS_RE.match(osis):
                errors.append(f"{path.name}: invalid OSIS book name {osis!r}")
            if osis in books:
                errors.append(f"{path.name}: duplicate [Book {osis}]")
            name = values.get("Name", "")
            abbrev = values.get("Abbrev", "")
            if not name:
                errors.append(f"{path.name}: [Book {osis}] missing Name")
            if not abbrev:
                errors.append(f"{path.name}: [Book {osis}] missing Abbrev")
            if known_osis and osis not in known_osis:
                warnings.append(f"{path.name}: unknown OSIS {osis}")

            effective = [name, abbrev]
            effective.extend(split_list(values.get("Aliases", "")))
            effective = [item for item in effective if item]
            for alias in effective:
                alias_map[norm(alias)].add(osis)
            if abbrev:
                abbrev_map[norm(abbrev)].append(osis)
            books[osis] = {
                "name": name,
                "abbrev": abbrev,
                "aliases": effective,
            }

        if not books:
            errors.append(f"{path.name}: no [Book ...] sections")
        for alias, osis_set in alias_map.items():
            if len(osis_set) > 1:
                errors.append(
                    f"{path.name}: alias {alias!r} maps to {sorted(osis_set)}")
        for abbrev, osis_list in abbrev_map.items():
            if len(osis_list) > 1:
                warnings.append(
                    f"{path.name}: preferred abbreviation {abbrev!r} used by "
                    f"{sorted(osis_list)}")

        locales[locale_id] = {
            "fallback": meta.get("Fallback", ""),
            "aliases": aliases,
            "books": books,
        }

    for locale_id, info in locales.items():
        fallback = info["fallback"]
        if locale_id == "en":
            if fallback and fallback != "en":
                warnings.append(f"en.conf: ignored Fallback={fallback}")
            continue
        if not fallback:
            errors.append(f"{locale_id}.conf: missing Locale.Fallback")
        elif fallback not in locales:
            errors.append(f"{locale_id}.conf: fallback {fallback!r} not found")

        seen = {locale_id}
        current = fallback
        while current:
            if current in seen:
                errors.append(f"{locale_id}.conf: fallback cycle at {current}")
                break
            seen.add(current)
            current = locales.get(current, {}).get("fallback", "")

    return errors, warnings, locales


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--directory", type=Path, default=root / "data" / "locales.d")
    parser.add_argument(
        "--sword-source", type=Path, default=root / "sword-1.9.0",
        help="optional SWORD source tree used for known-OSIS checking")
    args = parser.parse_args()

    known_osis = parse_known_osis(args.sword_source)
    errors, warnings, locales = validate(args.directory, known_osis)

    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)
    for error in errors:
        print(f"error: {error}", file=sys.stderr)

    print(f"validated {len(locales)} locale(s), "
          f"{sum(len(info['books']) for info in locales.values())} book entries")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
