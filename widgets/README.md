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

- Click the arrow to open the picker.
- Select a book; the chapter grid updates.
- Click a chapter; the reference is confirmed and the popover closes.
- The demo buttons exercise alternate references.

## Keyboard behavior

- Type directly in the main entry when the picker is closed.
- `Enter` parses and confirms.
- `Escape` closes the popover.
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
- The picker uses `GtkListBox` for books and `GtkFlowBox` for chapter buttons.
- Book aliases are resolved through `GswLocaleMgr`; the widget does not
  hard-code the 66 book names.
- The main application is deliberately untouched.
