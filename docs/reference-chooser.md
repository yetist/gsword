# GswordReferenceChooser

Reusable GTK4 widget for choosing a Bible book, chapter, and verse, with
direct keyboard input and localized book abbreviations.  The gsword reader
uses the full path-bar form, while each workspace pane uses a compact form
with the same picker popover.

## Files

```text
src/
├── gsword-reference-chooser.c
├── gsword-reference-chooser.h
├── gsword-reference-parser.c
└── gsword-reference-parser.h

tests/
├── reference-chooser-demo.c
└── test-reference-chooser.c
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
  ./build/tests/gsword-reference-chooser-demo en KJV "Gen 1:1"
```

Chinese names and abbreviations:

```sh
GDK_BACKEND=x11 \
GSW_LOCALE_PATH="$PWD/data/locales.d" \
LANG=zh_CN.UTF-8 \
  ./build/tests/gsword-reference-chooser-demo zh-Hans KJV "创 1:1"
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
  `[◀] book [▶] [◀] chapter [▶] : [◀] verse [▶]`.
- Click the book, chapter, or verse label to open the picker.
- The picker has separate Old Testament and New Testament tabs, so the New
  Testament group is always directly visible.
- The picker also shows a verse grid for the selected chapter.
- Chapter and verse buttons use a fixed 40x32 layout with a zero-padding
  numeric label, so 1-digit and 3-digit values do not change button size.
- Chapters and verses are laid out in a `GtkGrid` with exactly 10 fixed
  columns; they are left-aligned and are not stretched evenly across the
  available width.
- The book pane and the chapter/verse pane are separated by a fixed-width
  `GtkBox` layout and a vertical separator, so long book names cannot overlap
  the numeric grids.
- Path bar segments use fixed widths:
  - book: 140 px;
  - chapter: 52 px, sized for up to three digits;
  - verse: 44 px, sized for up to two digits.
  Chapter and verse segments show numbers only, separated by a read-only
  colon label.  Long book names are ellipsized with a full tooltip.
- The grid's minimum width is 436 px (10 × 40 px plus 9 × 4 px spacing), and
  the surrounding scrolled area is reserved at 452 px so the tenth column and
  the vertical scrollbar always have enough room.
- For the KJV's 66 books, the chapter-count distribution is:
  - 32 books need 1 row,
  - 42 books need at most 2 rows,
  - 54 books need at most 3 rows,
  - 60 books need at most 4 rows.
  `60 / 66 = 90.9%`, so the chapter area is four rows high.
- Four rows require `4 × 32 + 3 × 4 = 140 px`; both chapter and verse grids
  use that height.
- Select a book; the chapter and verse grids update.
- Click a chapter; the verse grid updates while the picker stays open.
- Click a verse; the reference is confirmed and the path bar returns.
- Press `Ctrl+L` to replace the path bar with an editable text field.
- The demo buttons exercise alternate references.

The compact form used by workspace panes hides the previous/next arrows and
the edit button.  It keeps the book, chapter, and verse segments clickable, so
the same picker popover and parser are used without taking a full toolbar row.

## Keyboard behavior

- `Ctrl+L` switches the path bar to the text-entry state.
- In text-entry state, type a reference and press `Enter` to confirm.
- `Escape` cancels editing and returns to the path bar.
- The apply and cancel buttons provide the same actions with the mouse.
- The picker's filter entry accepts the same abbreviations as the main entry.
- `Enter` in the filter entry confirms the reference.
- Clicking a book row focuses the book list; `Up` / `Down` then switches to
  the previous/next book. The current book stays highlighted and is scrolled
  into view.
- Clicking a chapter number focuses it; `Left` / `Right` switches chapter.
- From the chapter grid, `Down` moves focus to the verse grid; `Left` /
  `Right` then switches verse without closing the picker.
- From the verse grid, `Up` returns to the chapter grid. Clicking a verse
  still confirms the reference and closes the picker.
- The path bar itself remains fully mouse-driven; no text entry is shown until
  `Ctrl+L` or an application accelerator is used.

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
| Open picker | Old Testament and New Testament tabs are both visible |
| Select New Testament | New Testament books are shown directly |
| Click next-verse | 创世记 1:1 → 创世记 1:2 |
| Click previous-verse | 创世记 1:2 → 创世记 1:1 |
| At 创世记 1:1, previous-verse | button disabled |
| At 创世记 1:31, next-verse | 创世记 2:1 |
| Click chapter, then verse | chapter changes, picker remains for verse selection |
| Open picker | the verse grid shows the current chapter's verses |

The chapter and verse boundary behavior is intentional: navigation crosses
book/chapter boundaries instead of stopping at the end of a chapter/book.

## Public API

`GswordReferenceChooser` is a `GtkBox` subclass.

```c
GtkWidget *chooser = gsword_reference_chooser_new("KJV", "zh-Hans");

/* Compact form for narrow containers such as workspace panes. */
GtkWidget *compact = gsword_reference_chooser_new_compact("KJV", "zh-Hans");

gsword_reference_chooser_set_reference(chooser, book, chapter, verse);
gsword_reference_chooser_begin_edit(chooser);

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

## Implementation notes

- Reference parsing is independent of the widget and lives in
  `gsword-reference-parser.c`; book candidate lookup takes only (text, locale,
  versification).
- `gsword-reference-chooser.h` includes the parser header, so applications can
  keep including only the chooser header.
- Main-entry and filter-entry parsing share `update_from_text()` and
  `activate_text()`, avoiding two subtly different commit paths.
- Path text updates go through `update_reference_text()`, keeping the entry,
  filter entry, and `updating` guard in sync.
- When the popover is hidden, `set_reference()` only updates the path bar; the
  picker is refreshed when it is next opened.  This avoids rebuilding book rows
  and number grids while the application synchronizes the reader position.
- Path widths, number-grid columns, button sizes, row count, grid width,
  scrolled-area width, and popover pane dimensions are named constants at the
  top of the C file.
- The widget CSS provider is installed once per process for the default
  display instead of once per widget instance.
- The picker popover is parented to the chapter button so it opens at a stable
  position below the path bar.
- The picker uses two `GtkListBox` pages (`旧约` / `新约`) inside a
  `GtkNotebook`, plus 10-column `GtkGrid` sections for chapters and verses.
- The path bar and the editor are two rows whose visibility is toggled; they
  share the same selection state and occupy the same single toolbar row.
- Book aliases are resolved through `GswLocaleMgr`; the widget does not
  hard-code the 66 book names.
- The main reader and workspace panes share the widget source directly; there
  is no second reference-entry implementation in the application.
- UI labels and parser messages use the `gsword` gettext domain.
