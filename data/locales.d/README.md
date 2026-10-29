# libgsword locale packs

This directory contains locale data owned and installed by libgsword. The
runtime does not read SWORD's `/usr/share/sword/locales.d` directory.

## Format

Locale files are UTF-8 `GKeyFile` documents.

```ini
[Locale]
Format=1
Id=zh-Hans
Name=简体中文
Language=zh-Hans
Direction=ltr
Version=2026.10
Fallback=en
Aliases=zh_Hans;zh_CN;zh-CN

[Strings]
Chapter=章
Verse=节

[Book Gen]
Name=创世记
Abbrev=创
Aliases=創;创世;Gen;Gn
```

Rules:

- One `[Book OSIS]` section per book.
- `Name` is the localized long name and is also accepted as an input alias.
- `Abbrev` is the single preferred short name and is also accepted as input.
- `Aliases` contains additional input aliases separated by `;`.
- Multiple aliases may point to the same OSIS book.
- Exact aliases must be unique within a locale.
- `Fallback` must name another installed locale, except for `en`.

## Search paths

The runtime lookup order is:

1. built-in English fallback
2. compiled-in `${datadir}/libgsword/locales.d`
3. `${XDG_DATA_DIRS}/libgsword/locales.d`
4. `${XDG_DATA_HOME}/libgsword/locales.d`
5. directories in `GSW_LOCALE_PATH`

Later entries override earlier entries with the same locale ID.

## Regenerating

The initial files are generated from SWORD 1.9.0 locale data:

```sh
python3 tools/import-sword-locales.py
python3 tools/validate-locales.py
```

Generated files are checked in so normal builds do not need the SWORD source
tree. Edit the importer or the generated files deliberately; do not depend on
files under `/usr/share/sword` at runtime.

## Licensing and attribution

The locale data is derived from the SWORD Project locale files. See `COPYING`
and the project README for licensing. SWORD locale files are distributed under
the GNU General Public License, version 2 or later.
