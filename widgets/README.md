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

- The top navigation row provides one-click previous/next book,
  chapter, and verse buttons.
- Previous/next chapter crosses book boundaries: the last chapter of a book
  advances to the first chapter of the next book, and the first chapter goes
  back to the last chapter of the previous book.
- Previous/next verse crosses chapter and book boundaries as well.
- The default state is a clickable Nautilus-style path bar:
  `[◀] book [▶] [◀] chapter [▶] [◀] verse [▶] [edit]`.
- Click the book, chapter, or verse label to open the picker.
- The picker has separate `旧约 / OT` and `新约 / NT` tabs, so the New
  Testament group is always directly visible.
- The picker also shows a `Verses / 节` grid for the selected chapter.
- Chapter and verse buttons use a fixed 40x32 layout with a zero-padding
  numeric label, so 1-digit and 3-digit values do not change button size.
- Chapters and verses are laid out in a `GtkGrid` with exactly 10 fixed
  columns; they are left-aligned and are not stretched evenly across the
  available width.
- The grid's minimum width is 436 px (10 × 40 px plus 9 × 4 px spacing), and
  the surrounding scrolled area is reserved at 452 px so the tenth column and
  the vertical scrollbar always have enough room.
- Select a book; the chapter and verse grids update.
- Click a chapter; the verse grid updates while the picker stays open.
- Click a verse; the reference is confirmed and the path bar returns.
- Click the pencil to replace the path bar with an editable text field.
- The demo buttons exercise alternate references.

## Keyboard behavior

- `Ctrl+L` switches the path bar to the text-entry state.
- In text-entry state, type a reference and press `Enter` to confirm.
- `Escape` cancels editing and returns to the path bar.
- The apply and cancel buttons provide the same actions with the mouse.
- The picker's filter entry accepts the same abbreviations as the main entry.
- `Enter` in the filter entry confirms the reference.
- The path bar itself remains fully mouse-driven; no text entry is shown until
  the edit button, `Ctrl+L`, or the pencil is used.

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
| Click next-verse | 创世记 1:1 → 创世记 1:2 |
| Click previous-verse | 创世记 1:2 → 创世记 1:1 |
| At 创世记 1:1, previous-verse | button disabled |
| At 创世记 1:31, next-verse | 创世记 2:1 |
| Click chapter, then verse | chapter changes, picker remains for verse selection |
| Open picker | the `Verses / 节` grid shows the current chapter's verses |

The chapter and verse boundary behavior is intentional: navigation crosses
book/chapter boundaries instead of stopping at the end of a chapter/book.

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
  `GtkNotebook`, plus 10-column `GtkGrid` sections for chapters and verses.
- The path bar and the editor are two pages of a `GtkStack`; they share the
  same selection state and occupy the same single row.
- Book aliases are resolved through `GswLocaleMgr`; the widget does not
  hard-code the 66 book names.
- The main application is deliberately untouched.
