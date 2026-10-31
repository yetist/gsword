# GswordReferenceChooser

Standalone GTK4 widget for choosing a Bible book and chapter, with direct
keyboard input and localized book abbreviations.  It is not wired into the
main application yet.

## Files

```text
widgets/
├── gsword-reference-chooser.c
├── gsword-reference-chooser.h
├── reference-chooser-demo.c
├── test-reference-chooser.c
├── meson.build
└── README.md
```

## Build

```sh
meson setup --reconfigure build
meson compile -C build
```

## Run the demo

English:

```sh
GDK_BACKEND=x11 \
  ./build/widgets/gsword-reference-chooser-demo en KJV "Gen 1:1"
```

Chinese names and abbreviations:

```sh
GDK_BACKEND=x11 \
GSW_LOCALE_PATH="$PWD/data/locales.d" \
LANG=zh_CN.UTF-8 \
  ./build/widgets/gsword-reference-chooser-demo zh-Hans KJV "创 1:1"
```

Demo arguments:

```text
argv[1]  locale          default: en
argv[2]  versification   default: KJV
argv[3]  initial reference
```

## Mouse behavior

- The top navigation row provides one-click previous/next book and
  previous/next chapter buttons.
- Previous/next chapter crosses book boundaries: the last chapter of a book
  advances to the first chapter of the next book, and the first chapter goes
  back to the last chapter of the previous book.
- Click the book or chapter label, or the arrow, to open the picker.
- The picker has separate `旧约 / OT` and `新约 / NT` tabs, so the New
  Testament group is always directly visible.
- Select a book; the chapter grid updates.
- Click a chapter; the reference is confirmed and the popover closes.
- The demo buttons exercise alternate references.

## Keyboard behavior

- Type directly in the main entry when the picker is closed.
- `Enter` parses and confirms.
- `Escape` closes the popover.
- The top navigation row also works with mouse clicks; it does not open the
  popover unless the book or chapter label is clicked.
- Clicking the arrow focuses the filter entry inside the picker.
- The filter entry accepts the same abbreviations as the main entry.
- `Enter` in the filter entry confirms the reference.

Supported examples:

```text
Gen 1
Ge1
Genesis 1:1
John 3:16
Ps 23
1 Cor 13
1co13:4
创1
诗23
林前13:4
```

Ambiguous input such as `1` is rejected by `gsword_reference_parse()` with
`GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK`; the widget shows the matching books
for mouse selection.

## Parser tests

```sh
meson test -C build reference-chooser
```

The test covers English and Chinese aliases, chapter/verse parsing, invalid
chapters/verses, unknown books, and ambiguous numbered books.

## Manual navigation tests

Run the Chinese demo above, then check:

| Operation | Expected |
|---|---|
| Click next-book | 创世记 1:1 → 出埃及记 1:1 |
| Click next-chapter | 出埃及记 1:1 → 出埃及记 2:1 |
| Click previous-book | 出埃及记 2:1 → 创世记 2:1 |
| Click previous-chapter | 创世记 2:1 → 创世记 1:1 |
| At 创世记 1:1, previous-book/previous-chapter | buttons disabled |
| At Genesis 50:1, next-chapter | Exodus 1:1 |
| Open picker | 旧约 / OT and 新约 / NT tabs are both visible |
| Select 新约 / NT | New Testament books are shown directly |

The chapter boundary behavior is intentional: chapter navigation crosses
book boundaries instead of stopping at the end of a book.

## Public API

`GswordReferenceChooser` is a `GtkBox` subclass.

```c
GtkWidget *chooser = gsword_reference_chooser_new("KJV", "zh-Hans");

gsword_reference_chooser_set_reference(chooser, book, chapter, verse);

gint book, chapter, verse;
gsword_reference_chooser_get_reference(chooser, &book, &chapter, &verse);
gchar *osis = gsword_reference_chooser_dup_osis(chooser);
```

`book` in the chooser API is the **0-based global book index** in the selected
versification.  `gsword_reference_parse()` returns the same index plus the
OSIS name.

Signals:

```text
changed   selection changed inside the widget
activate  user confirmed a reference
```

## Notes

- The popover is parented to the entry, not to the enclosing box, so it does
  not affect the entry's layout.
- The picker uses two `GtkListBox` pages (`旧约` / `新约`) inside a
  `GtkNotebook`, plus `GtkFlowBox` for chapter buttons.
- The compact navigation row sits above the text entry and updates the same
  selection state as the popover.
- Book aliases are resolved through `GswLocaleMgr`; the widget does not
  hard-code the 66 book names.
- The main application is deliberately untouched.
