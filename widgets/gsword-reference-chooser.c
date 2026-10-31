/*
 * gsword-reference-chooser.c: standalone book/chapter reference chooser
 */

#include "gsword-reference-chooser.h"

#include <string.h>

typedef struct {
    gint index;
    gint testament;
    gint chapter_max;
    gchar *osis;
    gchar *name;
    gchar *abbrev;
} GswordBookInfo;

typedef struct {
    GswordReferenceChooser *self;
    GtkEntry *entry;
    GtkEntry *filter_entry;
    GtkWidget *path_row;
    GtkWidget *edit_row;
    GtkButton *edit_button;
    GtkButton *apply_button;
    GtkButton *cancel_button;
    GtkButton *book_button;
    GtkButton *chapter_button;
    GtkButton *verse_button;
    GtkButton *prev_book_button;
    GtkButton *next_book_button;
    GtkButton *prev_chapter_button;
    GtkButton *next_chapter_button;
    GtkButton *prev_verse_button;
    GtkButton *next_verse_button;
    GtkPopover *popover;
    GtkNotebook *book_notebook;
    GtkListBox *book_lists[2];
    GtkFlowBox *chapter_flow;
    GtkFlowBox *verse_flow;
    GtkLabel *hint;
    GswVersification *v11n;
    gchar *versification_name;
    gchar *locale;
    GPtrArray *books;
    gint book;
    gint chapter;
    gint verse;
    gboolean updating;
    gboolean editing;
} GswordReferenceChooserPrivate;

struct _GswordReferenceChooser {
    GtkBox parent_instance;
};

static void update_navigation_labels(GswordReferenceChooser *self);
static void set_edit_mode(GswordReferenceChooser *self, gboolean editing);
static void on_entry_activate(GtkEntry *entry, gpointer user_data);
static void populate_verse_buttons(GswordReferenceChooserPrivate *priv);
static gint get_verse_max(GswordReferenceChooserPrivate *priv, gint chapter);

G_DEFINE_TYPE_WITH_PRIVATE(GswordReferenceChooser,
                           gsword_reference_chooser, GTK_TYPE_BOX)

enum {
    SIGNAL_CHANGED,
    SIGNAL_ACTIVATE,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

GQuark
gsword_reference_error_quark(void)
{
    return g_quark_from_static_string("gsword-reference-error-quark");
}

void
gsword_reference_clear(GswordReference *reference)
{
    if (reference == NULL)
        return;
    g_free(reference->osis);
    reference->osis = NULL;
    reference->book = 0;
    reference->chapter = 0;
    reference->verse = 0;
}

static GswordReferenceChooserPrivate *
chooser_priv(GswordReferenceChooser *self)
{
    return gsword_reference_chooser_get_instance_private(self);
}

static void
book_info_free(gpointer data)
{
    GswordBookInfo *book = data;

    if (book == NULL)
        return;
    g_free(book->osis);
    g_free(book->name);
    g_free(book->abbrev);
    g_free(book);
}

static const gchar *
book_display_name(GswordReferenceChooserPrivate *priv, gint book)
{
    GswordBookInfo *info;

    if (priv->books == NULL || book < 0 ||
        book >= (gint)priv->books->len)
        return "";
    info = g_ptr_array_index(priv->books, book);
    return info->name != NULL ? info->name : info->osis;
}

static const gchar *
book_osis(GswordReferenceChooserPrivate *priv, gint book)
{
    GswordBookInfo *info;

    if (priv->books == NULL || book < 0 ||
        book >= (gint)priv->books->len)
        return NULL;
    info = g_ptr_array_index(priv->books, book);
    return info->osis;
}

static gboolean
book_matches_filter(GswordBookInfo *book, GPtrArray *filter)
{
    guint i;

    if (filter == NULL || filter->len == 0)
        return TRUE;
    for (i = 0; i < filter->len; i++) {
        if (g_strcmp0(book->osis, g_ptr_array_index(filter, i)) == 0)
            return TRUE;
    }
    return FALSE;
}

static void
clear_book_rows(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL) {
        gtk_list_box_remove(list,
                            gtk_widget_get_first_child(GTK_WIDGET(list)));
    }
}

static void
populate_book_list(GswordReferenceChooserPrivate *priv, GPtrArray *filter)
{
    guint i;
    gint first_page = -1;
    gint current_page = -1;
    gboolean current_selected = FALSE;

    if (priv->book_lists[0] == NULL || priv->book_lists[1] == NULL)
        return;
    priv->updating = TRUE;
    clear_book_rows(priv->book_lists[0]);
    clear_book_rows(priv->book_lists[1]);
    for (i = 0; i < priv->books->len; i++) {
        GswordBookInfo *book = g_ptr_array_index(priv->books, i);
        GtkListBox *list;
        GtkWidget *row;
        GtkWidget *box;
        GtkWidget *name;
        GtkWidget *abbrev;

        if (!book_matches_filter(book, filter))
            continue;
        list = priv->book_lists[book->testament - 1];
        if (first_page < 0)
            first_page = book->testament - 1;
        row = gtk_list_box_row_new();
        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        name = gtk_label_new(book->name != NULL ? book->name : book->osis);
        abbrev = gtk_label_new(book->abbrev != NULL ? book->abbrev : "");
        gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
        gtk_widget_set_hexpand(name, TRUE);
        gtk_label_set_xalign(GTK_LABEL(abbrev), 1.0f);
        gtk_widget_add_css_class(abbrev, "dim-label");
        gtk_box_append(GTK_BOX(box), name);
        gtk_box_append(GTK_BOX(box), abbrev);
        gtk_widget_set_margin_top(box, 4);
        gtk_widget_set_margin_bottom(box, 4);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data(G_OBJECT(row), "book-index",
                          GINT_TO_POINTER(book->index + 1));
        gtk_list_box_append(list, row);
        if (book->index == priv->book) {
            current_page = book->testament - 1;
            current_selected = TRUE;
        }
    }
    if (priv->book_notebook != NULL) {
        gint page = current_selected ? current_page
                   : current_page >= 0 ? current_page
                   : first_page >= 0 ? first_page : 0;

        gtk_notebook_set_current_page(priv->book_notebook, page);
    }
    priv->updating = FALSE;
}

static void
clear_chapter_buttons(GswordReferenceChooserPrivate *priv)
{
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(priv->chapter_flow))) != NULL)
        gtk_flow_box_remove(priv->chapter_flow, child);
}

static void
on_chapter_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    gint chapter = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),
                                                      "chapter"));
    gchar *text;

    if (priv->updating || chapter < 1)
        return;
    priv->chapter = chapter;
    priv->verse = 1;
    text = g_strdup_printf("%s %d:1",
                           book_display_name(priv, priv->book), chapter);
    priv->updating = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
    if (priv->filter_entry != NULL)
        gtk_editable_set_text(GTK_EDITABLE(priv->filter_entry), text);
    priv->updating = FALSE;
    g_free(text);
    populate_book_list(priv, NULL);
    populate_verse_buttons(priv);
    update_navigation_labels(self);
    g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
}

static void
populate_chapter_buttons(GswordReferenceChooserPrivate *priv)
{
    GswordBookInfo *book;
    gint chapter;

    if (priv->chapter_flow == NULL || priv->books == NULL ||
        priv->book < 0 || priv->book >= (gint)priv->books->len)
        return;
    book = g_ptr_array_index(priv->books, priv->book);
    priv->updating = TRUE;
    clear_chapter_buttons(priv);
    for (chapter = 1; chapter <= book->chapter_max; chapter++) {
        gchar *label = g_strdup_printf("%d", chapter);
        GtkWidget *button = gtk_button_new_with_label(label);

        g_free(label);
        gtk_widget_set_size_request(button, 34, 30);
        g_object_set_data(G_OBJECT(button), "chapter",
                          GINT_TO_POINTER(chapter));
        g_signal_connect(button, "clicked",
                         G_CALLBACK(on_chapter_clicked), priv->self);
        if (chapter == priv->chapter)
            gtk_widget_add_css_class(button, "suggested-action");
        gtk_flow_box_append(priv->chapter_flow, button);
    }
    priv->updating = FALSE;
}

static void
update_navigation_labels(GswordReferenceChooser *self)
{
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    gboolean chinese;
    gchar *chapter_text;
    gchar *verse_text;
    gint verse_max;

    if (priv->book_button == NULL || priv->chapter_button == NULL ||
        priv->verse_button == NULL || priv->books == NULL ||
        priv->books->len == 0)
        return;
    chinese = priv->locale != NULL &&
              g_str_has_prefix(priv->locale, "zh");
    gtk_button_set_label(priv->book_button,
                         book_display_name(priv, priv->book));
    chapter_text = chinese
                 ? g_strdup_printf("第 %d 章", priv->chapter)
                 : g_strdup_printf("Chapter %d", priv->chapter);
    verse_text = chinese
               ? g_strdup_printf("第 %d 节", priv->verse)
               : g_strdup_printf("Verse %d", priv->verse);
    gtk_button_set_label(priv->chapter_button, chapter_text);
    gtk_button_set_label(priv->verse_button, verse_text);
    g_free(chapter_text);
    g_free(verse_text);

    verse_max = get_verse_max(priv, priv->chapter);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->prev_book_button),
                             priv->book > 0);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->next_book_button),
                             priv->book + 1 < (gint)priv->books->len);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->prev_chapter_button),
                             priv->book > 0 || priv->chapter > 1);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->prev_verse_button),
                             priv->book > 0 || priv->chapter > 1 ||
                             priv->verse > 1);
    {
        GswordBookInfo *book = g_ptr_array_index(priv->books, priv->book);

        gtk_widget_set_sensitive(GTK_WIDGET(priv->next_chapter_button),
                                 priv->chapter < book->chapter_max ||
                                 priv->book + 1 <
                                 (gint)priv->books->len);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->next_verse_button),
                                 priv->verse < verse_max ||
                                 priv->chapter < book->chapter_max ||
                                 priv->book + 1 <
                                 (gint)priv->books->len);
    }
    if (priv->book_notebook != NULL)
        gtk_notebook_set_current_page(
            priv->book_notebook,
            ((GswordBookInfo *)g_ptr_array_index(
                priv->books, priv->book))->testament - 1);
}

static gint
get_verse_max(GswordReferenceChooserPrivate *priv, gint chapter)
{
    GswBook *book;

    if (priv->v11n == NULL || priv->books == NULL ||
        priv->book < 0 || priv->book >= (gint)priv->books->len ||
        chapter < 1)
        return 0;
    book = gsw_versification_get_book(priv->v11n, priv->book);
    return book != NULL ? gsw_book_get_verse_max(book, chapter) : 0;
}

static void
clear_verse_buttons(GswordReferenceChooserPrivate *priv)
{
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(priv->verse_flow))) != NULL)
        gtk_flow_box_remove(priv->verse_flow, child);
}

static void
on_verse_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    gint verse = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),
                                                    "verse"));
    gchar *text;

    if (priv->updating || verse < 1)
        return;
    priv->verse = verse;
    text = g_strdup_printf("%s %d:%d",
                           book_display_name(priv, priv->book),
                           priv->chapter, verse);
    priv->updating = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
    if (priv->filter_entry != NULL)
        gtk_editable_set_text(GTK_EDITABLE(priv->filter_entry), text);
    priv->updating = FALSE;
    g_free(text);
    update_navigation_labels(self);
    g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
    g_signal_emit(self, signals[SIGNAL_ACTIVATE], 0);
    set_edit_mode(self, FALSE);
}

static void
populate_verse_buttons(GswordReferenceChooserPrivate *priv)
{
    gint verse_max;
    gint verse;

    if (priv->verse_flow == NULL)
        return;
    verse_max = get_verse_max(priv, priv->chapter);
    priv->updating = TRUE;
    clear_verse_buttons(priv);
    for (verse = 1; verse <= verse_max; verse++) {
        gchar *label = g_strdup_printf("%d", verse);
        GtkWidget *button = gtk_button_new_with_label(label);

        g_free(label);
        gtk_widget_set_size_request(button, 34, 30);
        g_object_set_data(G_OBJECT(button), "verse",
                          GINT_TO_POINTER(verse));
        g_signal_connect(button, "clicked",
                         G_CALLBACK(on_verse_clicked), priv->self);
        if (verse == priv->verse)
            gtk_widget_add_css_class(button, "suggested-action");
        gtk_flow_box_append(priv->verse_flow, button);
    }
    priv->updating = FALSE;
}

static void
on_book_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    gpointer book_data;
    gint book;

    (void)list;
    if (row == NULL || priv->updating)
        return;
    book_data = g_object_get_data(G_OBJECT(row), "book-index");
    if (book_data == NULL)
        return;
    book = GPOINTER_TO_INT(book_data) - 1;
    if (book < 0 || book >= (gint)priv->books->len)
        return;
    priv->book = book;
    if (priv->chapter < 1)
        priv->chapter = 1;
    if (priv->chapter >
        ((GswordBookInfo *)g_ptr_array_index(priv->books, book))->chapter_max)
        priv->chapter = 1;
    populate_chapter_buttons(priv);
    populate_verse_buttons(priv);
    update_navigation_labels(self);
    {
        gchar *text = g_strdup_printf("%s %d:%d",
            book_display_name(priv, priv->book), priv->chapter, priv->verse);
        priv->updating = TRUE;
        gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
        if (priv->filter_entry != NULL)
            gtk_editable_set_text(GTK_EDITABLE(priv->filter_entry), text);
        priv->updating = FALSE;
        g_free(text);
    }
    if (priv->filter_entry != NULL)
        gtk_widget_grab_focus(GTK_WIDGET(priv->filter_entry));
}

static void
set_reference_internal(GswordReferenceChooser *self, gint book,
                       gint chapter, gint verse, gboolean emit)
{
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GswordBookInfo *info;
    gchar *text;

    if (priv->books == NULL || priv->books->len == 0)
        return;
    if (book < 0 || book >= (gint)priv->books->len)
        book = 0;
    info = g_ptr_array_index(priv->books, book);
    if (chapter < 1 || chapter > info->chapter_max)
        chapter = 1;
    {
        GswBook *vbook = gsw_versification_get_book(priv->v11n, book);
        gint verse_max = vbook != NULL
                       ? gsw_book_get_verse_max(vbook, chapter) : 0;

        if (verse < 1 || (verse_max > 0 && verse > verse_max))
            verse = 1;
    }

    priv->book = book;
    priv->chapter = chapter;
    priv->verse = verse;
    text = g_strdup_printf("%s %d:%d",
                           book_display_name(priv, book), chapter, verse);
    priv->updating = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
    priv->updating = FALSE;
    g_free(text);
    populate_chapter_buttons(priv);
    populate_verse_buttons(priv);
    update_navigation_labels(self);
    if (emit)
        g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
}

static void
emit_navigation(GswordReferenceChooser *self)
{
    g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
    g_signal_emit(self, signals[SIGNAL_ACTIVATE], 0);
}

static void
on_prev_book_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);

    (void)button;
    if (priv->books == NULL || priv->book <= 0)
        return;
    set_reference_internal(self, priv->book - 1, priv->chapter, 1, FALSE);
    emit_navigation(self);
}

static void
on_next_book_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);

    (void)button;
    if (priv->books == NULL || priv->book + 1 >= (gint)priv->books->len)
        return;
    set_reference_internal(self, priv->book + 1, priv->chapter, 1, FALSE);
    emit_navigation(self);
}

static void
on_prev_chapter_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GswordBookInfo *book;
    gint target_book = priv->book;
    gint target_chapter;

    (void)button;
    if (priv->books == NULL || priv->books->len == 0)
        return;
    if (priv->chapter > 1) {
        target_chapter = priv->chapter - 1;
    } else if (priv->book > 0) {
        target_book = priv->book - 1;
        book = g_ptr_array_index(priv->books, target_book);
        target_chapter = book->chapter_max;
    } else {
        return;
    }
    set_reference_internal(self, target_book, target_chapter, 1, FALSE);
    emit_navigation(self);
}

static void
on_next_chapter_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GswordBookInfo *book;
    gint target_book = priv->book;
    gint target_chapter;

    (void)button;
    if (priv->books == NULL || priv->books->len == 0)
        return;
    book = g_ptr_array_index(priv->books, priv->book);
    if (priv->chapter < book->chapter_max) {
        target_chapter = priv->chapter + 1;
    } else if (priv->book + 1 < (gint)priv->books->len) {
        target_book = priv->book + 1;
        target_chapter = 1;
    } else {
        return;
    }
    set_reference_internal(self, target_book, target_chapter, 1, FALSE);
    emit_navigation(self);
}

static void
on_prev_verse_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GswordBookInfo *book;
    gint target_book = priv->book;
    gint target_chapter = priv->chapter;
    gint target_verse;

    (void)button;
    if (priv->books == NULL || priv->books->len == 0)
        return;
    if (priv->verse > 1) {
        target_verse = priv->verse - 1;
    } else if (priv->chapter > 1) {
        target_chapter = priv->chapter - 1;
        target_verse = get_verse_max(priv, target_chapter);
    } else if (priv->book > 0) {
        target_book = priv->book - 1;
        book = g_ptr_array_index(priv->books, target_book);
        target_chapter = book->chapter_max;
        target_verse = get_verse_max(priv, target_chapter);
    } else {
        return;
    }
    if (target_verse < 1)
        target_verse = 1;
    set_reference_internal(self, target_book, target_chapter,
                           target_verse, FALSE);
    emit_navigation(self);
}

static void
on_next_verse_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GswordBookInfo *book;
    gint target_book = priv->book;
    gint target_chapter = priv->chapter;
    gint target_verse;
    gint verse_max = get_verse_max(priv, priv->chapter);

    (void)button;
    if (priv->books == NULL || priv->books->len == 0)
        return;
    if (priv->verse < verse_max) {
        target_verse = priv->verse + 1;
    } else {
        book = g_ptr_array_index(priv->books, priv->book);
        if (priv->chapter < book->chapter_max) {
            target_chapter = priv->chapter + 1;
        } else if (priv->book + 1 < (gint)priv->books->len) {
            target_book = priv->book + 1;
            target_chapter = 1;
        } else {
            return;
        }
        target_verse = 1;
    }
    set_reference_internal(self, target_book, target_chapter,
                           target_verse, FALSE);
    emit_navigation(self);
}

static GPtrArray *
candidate_osis(GswordReferenceChooserPrivate *priv, const gchar *text)
{
    GswLocaleMgr *locale_mgr = gsw_locale_mgr_get_default();
    GPtrArray *raw;
    GPtrArray *result;
    guint i;

    result = g_ptr_array_new_with_free_func(g_free);
    if (text == NULL || *text == '\0' || priv->v11n == NULL)
        return result;
    raw = gsw_locale_mgr_get_book_candidates(locale_mgr, text,
                                             priv->locale);
    for (i = 0; i < raw->len; i++) {
        const gchar *osis = g_ptr_array_index(raw, i);
        gint index = gsw_versification_get_book_number_by_osis(
            priv->v11n, osis);
        guint j;
        gboolean duplicate = FALSE;

        if (index < 0)
            continue;
        for (j = 0; j < result->len; j++) {
            if (g_strcmp0(g_ptr_array_index(result, j), osis) == 0) {
                duplicate = TRUE;
                break;
            }
        }
        if (!duplicate)
            g_ptr_array_add(result, g_strdup(osis));
    }
    g_ptr_array_unref(raw);
    return result;
}

static gboolean
parse_internal(const gchar *text, const gchar *locale,
               GswVersification *v11n, GswordReference *out,
               GPtrArray **candidates_out, GError **error)
{
    gchar *work;
    gchar *left;
    gchar *right = NULL;
    gchar *book_text;
    gchar *chapter_text = NULL;
    const gchar *colon;
    GswordReferenceChooserPrivate *fake_priv;
    GPtrArray *candidates = NULL;
    gint chapter = 0;
    gint verse = 1;
    gint book_index;
    gint chapter_max;
    gint verse_max;
    gsize len;
    gsize pos;

    if (out == NULL)
        return FALSE;
    memset(out, 0, sizeof(*out));
    if (text == NULL || *text == '\0') {
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_EMPTY,
                            "Reference is empty");
        return FALSE;
    }
    work = g_strdup(text);
    g_strstrip(work);
    if (*work == '\0') {
        g_free(work);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_EMPTY,
                            "Reference is empty");
        return FALSE;
    }
    colon = strchr(work, ':');
    if (colon != NULL) {
        left = g_strndup(work, (gsize)(colon - work));
        right = g_strdup(colon + 1);
        g_strstrip(right);
        if (*right == '\0') {
            g_free(work);
            g_free(left);
            g_free(right);
            g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                                "Verse is empty");
            return FALSE;
        }
    } else {
        left = g_strdup(work);
    }
    g_free(work);
    g_strstrip(left);

    len = strlen(left);
    pos = len;
    while (pos > 0 && g_ascii_isdigit(left[pos - 1]))
        pos--;
    if (pos == len) {
        book_text = g_strdup(left);
    } else if (pos == 0) {
        /* A bare number can match several numbered books.  Keep it as a
         * book token so the caller can present the ambiguity list. */
        book_text = g_strdup(left);
    } else {
        book_text = g_strndup(left, pos);
        chapter_text = g_strdup(left + pos);
        g_strstrip(book_text);
        chapter = (gint)g_ascii_strtoll(chapter_text, NULL, 10);
        if (chapter <= 0) {
            g_free(left);
            g_free(book_text);
            g_free(chapter_text);
            g_free(right);
            g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                GSWORD_REFERENCE_ERROR_INVALID_CHAPTER,
                                "Chapter is invalid");
            return FALSE;
        }
    }
    g_free(left);
    if (*book_text == '\0') {
        g_free(book_text);
        g_free(chapter_text);
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            "Book is missing");
        return FALSE;
    }

    /* Reuse the widget candidate helper without depending on a widget. */
    fake_priv = g_new0(GswordReferenceChooserPrivate, 1);
    fake_priv->v11n = v11n;
    fake_priv->locale = g_strdup(locale != NULL && *locale != '\0'
                                 ? locale : "en");
    candidates = candidate_osis(fake_priv, book_text);
    g_free(fake_priv->locale);
    g_free(fake_priv);
    g_free(book_text);
    g_free(chapter_text);
    if (candidates_out != NULL)
        *candidates_out = g_ptr_array_ref(candidates);
    if (candidates->len == 0) {
        g_ptr_array_unref(candidates);
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            "No matching book");
        return FALSE;
    }
    if (candidates->len > 1) {
        g_ptr_array_unref(candidates);
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK,
                            "Book abbreviation is ambiguous");
        return FALSE;
    }
    {
        const gchar *osis = g_ptr_array_index(candidates, 0);

        book_index = gsw_versification_get_book_number_by_osis(v11n, osis);
        g_free(out->osis);
        out->osis = g_strdup(osis);
    }
    g_ptr_array_unref(candidates);
    if (book_index < 0) {
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            "Book is not in this versification");
        return FALSE;
    }
    {
        GswBook *book = gsw_versification_get_book(v11n, book_index);

        chapter_max = gsw_book_get_chapter_max(book);
    }
    if (chapter < 1)
        chapter = 1;
    if (chapter > chapter_max) {
        g_free(right);
        g_set_error(error, GSWORD_REFERENCE_ERROR,
                    GSWORD_REFERENCE_ERROR_INVALID_CHAPTER,
                    "Chapter %d is out of range", chapter);
        return FALSE;
    }
    if (right != NULL) {
        gchar *end = NULL;

        verse = (gint)g_ascii_strtoll(right, &end, 10);
        if (end == right || (end != NULL && *end != '\0') || verse < 1) {
            g_free(right);
            g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                                "Verse is invalid");
            return FALSE;
        }
    }
    {
        GswBook *book = gsw_versification_get_book(v11n, book_index);

        verse_max = gsw_book_get_verse_max(book, chapter);
    }
    if (verse < 1)
        verse = 1;
    if (verse > verse_max) {
        g_free(right);
        g_set_error(error, GSWORD_REFERENCE_ERROR,
                    GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                    "Verse %d is out of range", verse);
        return FALSE;
    }
    g_free(right);
    out->book = book_index;
    out->chapter = chapter;
    out->verse = verse;
    return TRUE;
}

static gboolean
focus_filter_idle(gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);

    gtk_widget_grab_focus(GTK_WIDGET(priv->filter_entry));
    return G_SOURCE_REMOVE;
}

static void
schedule_filter_focus(GswordReferenceChooser *self)
{
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, focus_filter_idle,
                    g_object_ref(self), g_object_unref);
}

static void
set_edit_mode(GswordReferenceChooser *self, gboolean editing)
{
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    gchar *text;

    priv->editing = editing;
    if (priv->path_row == NULL)
        return;
    if (!editing) {
        gtk_popover_popdown(priv->popover);
        text = g_strdup_printf("%s %d:%d",
            book_display_name(priv, priv->book), priv->chapter, priv->verse);
        priv->updating = TRUE;
        gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
        priv->updating = FALSE;
        g_free(text);
        gtk_widget_set_visible(priv->path_row, TRUE);
        gtk_widget_set_visible(priv->edit_row, FALSE);
        update_navigation_labels(self);
        return;
    }
    text = g_strdup_printf("%s %d:%d",
        book_display_name(priv, priv->book), priv->chapter, priv->verse);
    priv->updating = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
    priv->updating = FALSE;
    g_free(text);
    gtk_widget_set_visible(priv->path_row, FALSE);
    gtk_widget_set_visible(priv->edit_row, TRUE);
    gtk_widget_grab_focus(GTK_WIDGET(priv->entry));
    gtk_editable_select_region(GTK_EDITABLE(priv->entry), 0, -1);
}

static void
on_edit_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    set_edit_mode(GSWORD_REFERENCE_CHOOSER(user_data), TRUE);
}

static void
on_apply_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);

    (void)button;
    on_entry_activate(priv->entry, self);
}

static void
on_cancel_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    set_edit_mode(GSWORD_REFERENCE_CHOOSER(user_data), FALSE);
}

static gboolean
on_key_pressed(GtkEventControllerKey *controller, guint keyval,
               guint keycode, GdkModifierType state, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);

    (void)controller;
    (void)keycode;
    if ((state & GDK_CONTROL_MASK) != 0 &&
        (keyval == GDK_KEY_l || keyval == GDK_KEY_L)) {
        set_edit_mode(self, TRUE);
        return GDK_EVENT_STOP;
    }
    if (priv->editing && keyval == GDK_KEY_Escape) {
        set_edit_mode(self, FALSE);
        return GDK_EVENT_STOP;
    }
    return GDK_EVENT_PROPAGATE;
}

static void
on_entry_activate(GtkEntry *entry, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GError *error = NULL;
    GPtrArray *candidates = NULL;
    GswordReference reference;

    if (!parse_internal(gtk_editable_get_text(GTK_EDITABLE(entry)),
                        priv->locale, priv->v11n, &reference,
                        &candidates, &error)) {
        if (g_error_matches(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK) &&
            candidates != NULL && candidates->len > 1) {
            populate_book_list(priv, candidates);
            gtk_label_set_text(priv->hint,
                               "Multiple books match; choose one.");
            priv->updating = TRUE;
            if (priv->filter_entry != NULL)
                gtk_editable_set_text(
                    GTK_EDITABLE(priv->filter_entry),
                    gtk_editable_get_text(GTK_EDITABLE(entry)));
            priv->updating = FALSE;
            set_edit_mode(self, FALSE);
            gtk_popover_popup(priv->popover);
            schedule_filter_focus(self);
        } else {
            gtk_label_set_text(priv->hint,
                               error != NULL ? error->message
                                             : "Cannot parse reference");
        }
        g_clear_error(&error);
        g_clear_pointer(&candidates, g_ptr_array_unref);
        gsword_reference_clear(&reference);
        return;
    }
    g_clear_error(&error);
    g_clear_pointer(&candidates, g_ptr_array_unref);
    set_reference_internal(self, reference.book, reference.chapter,
                           reference.verse, FALSE);
    gsword_reference_clear(&reference);
    gtk_label_set_text(priv->hint, "");
    g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
    g_signal_emit(self, signals[SIGNAL_ACTIVATE], 0);
    set_edit_mode(self, FALSE);
}

static void
on_entry_changed(GtkEditable *editable, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    const gchar *text = gtk_editable_get_text(editable);
    GError *error = NULL;
    GPtrArray *candidates = NULL;
    GswordReference reference;

    if (priv->updating || !gtk_widget_get_visible(GTK_WIDGET(priv->popover)))
        return;
    if (text == NULL || *text == '\0') {
        populate_book_list(priv, NULL);
        gtk_label_set_text(priv->hint,
                           "Type a reference or choose a book.");
        return;
    }
    if (parse_internal(text, priv->locale, priv->v11n, &reference,
                       &candidates, &error)) {
        priv->book = reference.book;
        priv->chapter = reference.chapter;
        priv->verse = reference.verse;
        populate_chapter_buttons(priv);
        populate_verse_buttons(priv);
        populate_book_list(priv, candidates);
        update_navigation_labels(self);
        gtk_label_set_text(priv->hint, "");
    } else if (g_error_matches(error, GSWORD_REFERENCE_ERROR,
                               GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK) &&
               candidates != NULL && candidates->len > 0) {
        populate_book_list(priv, candidates);
        gtk_label_set_text(priv->hint,
                           "Multiple books match; choose one.");
    } else {
        gtk_label_set_text(priv->hint,
                           error != NULL ? error->message
                                         : "Cannot parse reference");
    }
    g_clear_error(&error);
    g_clear_pointer(&candidates, g_ptr_array_unref);
    gsword_reference_clear(&reference);
}

static void
on_popover_filter_changed(GtkEditable *editable, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    const gchar *text = gtk_editable_get_text(editable);
    GError *error = NULL;
    GPtrArray *candidates = NULL;
    GswordReference reference;

    if (priv->updating)
        return;
    if (text == NULL || *text == '\0') {
        populate_book_list(priv, NULL);
        gtk_label_set_text(priv->hint,
                           "Type a reference or choose a book.");
        return;
    }
    if (parse_internal(text, priv->locale, priv->v11n, &reference,
                       &candidates, &error)) {
        priv->book = reference.book;
        priv->chapter = reference.chapter;
        priv->verse = reference.verse;
        populate_book_list(priv, candidates);
        populate_chapter_buttons(priv);
        populate_verse_buttons(priv);
        update_navigation_labels(self);
        gtk_label_set_text(priv->hint, "");
    } else if (g_error_matches(error, GSWORD_REFERENCE_ERROR,
                               GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK) &&
               candidates != NULL && candidates->len > 0) {
        populate_book_list(priv, candidates);
        gtk_label_set_text(priv->hint,
                           "Multiple books match; choose one.");
    } else {
        gtk_label_set_text(priv->hint,
                           error != NULL ? error->message
                                         : "Cannot parse reference");
    }
    g_clear_error(&error);
    g_clear_pointer(&candidates, g_ptr_array_unref);
    gsword_reference_clear(&reference);
}

static void
on_popover_filter_activate(GtkEntry *entry, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GError *error = NULL;
    GPtrArray *candidates = NULL;
    GswordReference reference;

    if (!parse_internal(gtk_editable_get_text(GTK_EDITABLE(entry)),
                        priv->locale, priv->v11n, &reference,
                        &candidates, &error)) {
        if (g_error_matches(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK) &&
            candidates != NULL && candidates->len > 0) {
            populate_book_list(priv, candidates);
            gtk_label_set_text(priv->hint,
                               "Multiple books match; choose one.");
        } else {
            gtk_label_set_text(priv->hint,
                               error != NULL ? error->message
                                             : "Cannot parse reference");
        }
        g_clear_error(&error);
        g_clear_pointer(&candidates, g_ptr_array_unref);
        gsword_reference_clear(&reference);
        return;
    }
    g_clear_error(&error);
    g_clear_pointer(&candidates, g_ptr_array_unref);
    set_reference_internal(self, reference.book, reference.chapter,
                           reference.verse, FALSE);
    gsword_reference_clear(&reference);
    gtk_label_set_text(priv->hint, "");
    g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
    g_signal_emit(self, signals[SIGNAL_ACTIVATE], 0);
    set_edit_mode(self, FALSE);
}

static void
on_button_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(user_data);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    gchar *text;

    (void)button;
    if (gtk_widget_get_visible(GTK_WIDGET(priv->popover))) {
        gtk_popover_popdown(priv->popover);
        return;
    }
    text = g_strdup_printf("%s %d:%d",
                           book_display_name(priv, priv->book),
                           priv->chapter, priv->verse);
    priv->updating = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
    if (priv->filter_entry != NULL)
        gtk_editable_set_text(GTK_EDITABLE(priv->filter_entry), text);
    priv->updating = FALSE;
    g_free(text);
    populate_book_list(priv, NULL);
    populate_chapter_buttons(priv);
    populate_verse_buttons(priv);
    gtk_label_set_text(priv->hint,
                       "Type a reference or choose a book.");
    gtk_popover_popup(priv->popover);
    schedule_filter_focus(self);
}

static void
load_books(GswordReferenceChooserPrivate *priv)
{
    GswLocaleMgr *locale_mgr = gsw_locale_mgr_get_default();
    gint count;
    gint i;
    gint ot_books = 0;

    g_clear_pointer(&priv->books, g_ptr_array_unref);
    priv->books = g_ptr_array_new_with_free_func(book_info_free);
    if (priv->v11n == NULL)
        return;
    count = gsw_versification_get_book_count(priv->v11n);
    gsw_versification_get_bmax(priv->v11n, &ot_books, NULL);
    for (i = 0; i < count; i++) {
        GswBook *book = gsw_versification_get_book(priv->v11n, i);
        GswordBookInfo *info;
        const gchar *name;
        const gchar *abbrev;

        if (book == NULL)
            continue;
        name = gsw_book_get_name(book);
        if (priv->locale != NULL)
            name = gsw_locale_mgr_translate(locale_mgr, name, priv->locale);
        abbrev = gsw_book_get_preferred_abbreviation(book);
        if (abbrev != NULL && priv->locale != NULL)
            abbrev = gsw_locale_mgr_translate_preferred_abbreviation(
                locale_mgr, abbrev, priv->locale);
        info = g_new0(GswordBookInfo, 1);
        info->index = i;
        info->testament = i < ot_books ? 1 : 2;
        info->chapter_max = gsw_book_get_chapter_max(book);
        info->osis = g_strdup(gsw_book_get_osis_name(book));
        info->name = g_strdup(name != NULL ? name : info->osis);
        info->abbrev = g_strdup(abbrev != NULL ? abbrev : "");
        g_ptr_array_add(priv->books, info);
    }
    if (priv->books->len == 0)
        return;
    if (priv->book < 0 || priv->book >= (gint)priv->books->len)
        priv->book = 0;
    if (priv->chapter < 1 ||
        priv->chapter > ((GswordBookInfo *)g_ptr_array_index(
            priv->books, priv->book))->chapter_max)
        priv->chapter = 1;
    if (priv->verse < 1)
        priv->verse = 1;
}

void
gsword_reference_chooser_set_versification(
    GswordReferenceChooser *self, const gchar *versification)
{
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GswVersificationMgr *mgr = gsw_versification_mgr_get_default();
    GswVersification *v11n;

    if (versification == NULL || *versification == '\0')
        versification = "KJV";
    v11n = gsw_versification_mgr_get_system(mgr, versification);
    if (v11n == NULL)
        v11n = gsw_versification_mgr_get_system(mgr, "KJV");
    priv->v11n = v11n;
    g_free(priv->versification_name);
    priv->versification_name = g_strdup(versification);
    load_books(priv);
    if (priv->books != NULL && priv->books->len > 0) {
        gchar *text = g_strdup_printf("%s %d:%d",
            book_display_name(priv, priv->book), priv->chapter, priv->verse);
        priv->updating = TRUE;
        gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
        priv->updating = FALSE;
        g_free(text);
    }
    populate_book_list(priv, NULL);
    populate_chapter_buttons(priv);
    populate_verse_buttons(priv);
    update_navigation_labels(self);
}

void
gsword_reference_chooser_set_locale(GswordReferenceChooser *self,
                                    const gchar *locale)
{
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    gchar *text;

    g_free(priv->locale);
    priv->locale = g_strdup(locale != NULL && *locale != '\0' ? locale : "en");
    load_books(priv);
    if (priv->books == NULL || priv->books->len == 0)
        return;
    text = g_strdup_printf("%s %d:%d",
                           book_display_name(priv, priv->book),
                           priv->chapter, priv->verse);
    priv->updating = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
    priv->updating = FALSE;
    g_free(text);
    populate_book_list(priv, NULL);
    populate_chapter_buttons(priv);
    populate_verse_buttons(priv);
    update_navigation_labels(self);
}

void
gsword_reference_chooser_set_reference(GswordReferenceChooser *self,
                                       gint book, gint chapter, gint verse)
{
    g_return_if_fail(GSWORD_IS_REFERENCE_CHOOSER(self));
    set_reference_internal(self, book, chapter, verse, FALSE);
}

gboolean
gsword_reference_chooser_get_reference(GswordReferenceChooser *self,
                                       gint *book, gint *chapter, gint *verse)
{
    GswordReferenceChooserPrivate *priv;

    g_return_val_if_fail(GSWORD_IS_REFERENCE_CHOOSER(self), FALSE);
    priv = chooser_priv(self);
    if (book != NULL)
        *book = priv->book;
    if (chapter != NULL)
        *chapter = priv->chapter;
    if (verse != NULL)
        *verse = priv->verse;
    return priv->books != NULL && priv->books->len > 0;
}

gchar *
gsword_reference_chooser_dup_osis(GswordReferenceChooser *self)
{
    GswordReferenceChooserPrivate *priv;

    g_return_val_if_fail(GSWORD_IS_REFERENCE_CHOOSER(self), NULL);
    priv = chooser_priv(self);
    return g_strdup(book_osis(priv, priv->book));
}

static void
gsword_reference_chooser_finalize(GObject *object)
{
    GswordReferenceChooser *self = GSWORD_REFERENCE_CHOOSER(object);
    GswordReferenceChooserPrivate *priv = chooser_priv(self);

    g_clear_pointer(&priv->books, g_ptr_array_unref);
    g_free(priv->locale);
    g_free(priv->versification_name);
    G_OBJECT_CLASS(gsword_reference_chooser_parent_class)->finalize(object);
}

static void
gsword_reference_chooser_class_init(GswordReferenceChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gsword_reference_chooser_finalize;
    signals[SIGNAL_CHANGED] = g_signal_new(
        "changed", G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_LAST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    signals[SIGNAL_ACTIVATE] = g_signal_new(
        "activate", G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_LAST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gsword_reference_chooser_init(GswordReferenceChooser *self)
{
    GswordReferenceChooserPrivate *priv = chooser_priv(self);
    GtkWidget *chapter_scroll;
    GtkWidget *verse_scroll;
    GtkWidget *popover_box;
    GtkWidget *paned;
    GtkWidget *book_page;
    GtkWidget *chapter_page;
    GtkWidget *nav_row;
    GtkWidget *separator;
    GtkWidget *separator2;
    GtkWidget *separator3;

    gtk_orientable_set_orientation(GTK_ORIENTABLE(self),
                                   GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing(GTK_BOX(self), 4);

    priv->self = self;
    priv->v11n = gsw_versification_mgr_get_system(
        gsw_versification_mgr_get_default(), "KJV");
    priv->versification_name = g_strdup("KJV");
    priv->locale = g_strdup("en");
    priv->book = 0;
    priv->chapter = 1;
    priv->verse = 1;

    priv->entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->entry,
                                   "Gen 1:1 / 创 1:1 / 1 Cor 13");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->entry), TRUE);
    gtk_entry_set_activates_default(priv->entry, FALSE);
    g_signal_connect(priv->entry, "activate",
                     G_CALLBACK(on_entry_activate), self);
    g_signal_connect(priv->entry, "changed",
                     G_CALLBACK(on_entry_changed), self);

    nav_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    priv->prev_book_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("go-previous-symbolic"));
    priv->next_book_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("go-next-symbolic"));
    priv->prev_chapter_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("go-previous-symbolic"));
    priv->next_chapter_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("go-next-symbolic"));
    priv->prev_verse_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("go-previous-symbolic"));
    priv->next_verse_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("go-next-symbolic"));
    priv->book_button = GTK_BUTTON(gtk_button_new_with_label(""));
    priv->chapter_button = GTK_BUTTON(gtk_button_new_with_label(""));
    priv->verse_button = GTK_BUTTON(gtk_button_new_with_label(""));
    priv->edit_button = GTK_BUTTON(
        gtk_button_new_from_icon_name("document-edit-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(priv->book_button), "flat");
    gtk_widget_add_css_class(GTK_WIDGET(priv->chapter_button), "flat");
    gtk_widget_add_css_class(GTK_WIDGET(priv->verse_button), "flat");
    gtk_widget_add_css_class(GTK_WIDGET(priv->edit_button), "flat");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->prev_book_button),
                                "Previous book");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->next_book_button),
                                "Next book");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->prev_chapter_button),
                                "Previous chapter");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->next_chapter_button),
                                "Next chapter");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->book_button),
                                "Choose a book");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->chapter_button),
                                "Choose a chapter");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->prev_verse_button),
                                "Previous verse");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->next_verse_button),
                                "Next verse");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->verse_button),
                                "Choose a verse");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->edit_button),
                                "Edit reference (Ctrl+L)");
    g_signal_connect(priv->prev_book_button, "clicked",
                     G_CALLBACK(on_prev_book_clicked), self);
    g_signal_connect(priv->next_book_button, "clicked",
                     G_CALLBACK(on_next_book_clicked), self);
    g_signal_connect(priv->prev_chapter_button, "clicked",
                     G_CALLBACK(on_prev_chapter_clicked), self);
    g_signal_connect(priv->next_chapter_button, "clicked",
                     G_CALLBACK(on_next_chapter_clicked), self);
    g_signal_connect(priv->prev_verse_button, "clicked",
                     G_CALLBACK(on_prev_verse_clicked), self);
    g_signal_connect(priv->next_verse_button, "clicked",
                     G_CALLBACK(on_next_verse_clicked), self);
    g_signal_connect(priv->book_button, "clicked",
                     G_CALLBACK(on_button_clicked), self);
    g_signal_connect(priv->chapter_button, "clicked",
                     G_CALLBACK(on_button_clicked), self);
    g_signal_connect(priv->verse_button, "clicked",
                     G_CALLBACK(on_button_clicked), self);
    g_signal_connect(priv->edit_button, "clicked",
                     G_CALLBACK(on_edit_clicked), self);
    separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    separator2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    separator3 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(GTK_WIDGET(priv->book_button), 4);
    gtk_widget_set_margin_end(GTK_WIDGET(priv->book_button), 4);
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->prev_book_button));
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->book_button));
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->next_book_button));
    gtk_box_append(GTK_BOX(nav_row), separator);
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->prev_chapter_button));
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->chapter_button));
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->next_chapter_button));
    gtk_box_append(GTK_BOX(nav_row), separator2);
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->prev_verse_button));
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->verse_button));
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->next_verse_button));
    gtk_box_append(GTK_BOX(nav_row), separator3);
    gtk_box_append(GTK_BOX(nav_row), GTK_WIDGET(priv->edit_button));

    {
        GtkWidget *edit_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

        priv->apply_button = GTK_BUTTON(
            gtk_button_new_from_icon_name("object-select-symbolic"));
        priv->cancel_button = GTK_BUTTON(
            gtk_button_new_from_icon_name("window-close-symbolic"));
        gtk_widget_add_css_class(GTK_WIDGET(priv->apply_button), "flat");
        gtk_widget_add_css_class(GTK_WIDGET(priv->cancel_button), "flat");
        gtk_widget_set_tooltip_text(GTK_WIDGET(priv->apply_button),
                                    "Apply reference");
        gtk_widget_set_tooltip_text(GTK_WIDGET(priv->cancel_button),
                                    "Cancel editing");
        g_signal_connect(priv->apply_button, "clicked",
                         G_CALLBACK(on_apply_clicked), self);
        g_signal_connect(priv->cancel_button, "clicked",
                         G_CALLBACK(on_cancel_clicked), self);
        gtk_box_append(GTK_BOX(edit_box), GTK_WIDGET(priv->entry));
        gtk_box_append(GTK_BOX(edit_box), GTK_WIDGET(priv->apply_button));
        gtk_box_append(GTK_BOX(edit_box), GTK_WIDGET(priv->cancel_button));

        priv->path_row = nav_row;
        priv->edit_row = edit_box;
        gtk_widget_set_visible(edit_box, FALSE);
        gtk_widget_set_can_target(nav_row, TRUE);
        gtk_box_append(GTK_BOX(self), nav_row);
        gtk_box_append(GTK_BOX(self), edit_box);
    }

    {
        GtkEventController *key_controller = gtk_event_controller_key_new();

        g_signal_connect(key_controller, "key-pressed",
                         G_CALLBACK(on_key_pressed), self);
        gtk_event_controller_set_propagation_phase(
            key_controller, GTK_PHASE_CAPTURE);
        gtk_widget_add_controller(GTK_WIDGET(self), key_controller);
    }

    priv->filter_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->filter_entry,
                                   "Type to filter books / references");
    g_signal_connect(priv->filter_entry, "changed",
                     G_CALLBACK(on_popover_filter_changed), self);
    g_signal_connect(priv->filter_entry, "activate",
                     G_CALLBACK(on_popover_filter_activate), self);

    priv->popover = GTK_POPOVER(gtk_popover_new());
    gtk_popover_set_autohide(priv->popover, TRUE);
    gtk_popover_set_has_arrow(priv->popover, TRUE);
    gtk_popover_set_position(priv->popover, GTK_POS_BOTTOM);
    gtk_widget_set_focusable(GTK_WIDGET(priv->popover), FALSE);
    gtk_widget_set_parent(GTK_WIDGET(priv->popover),
                          GTK_WIDGET(priv->book_button));

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    book_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    chapter_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(book_page),
                   gtk_label_new("Books"));
    gtk_box_append(GTK_BOX(chapter_page),
                   gtk_label_new("Chapters"));
    gtk_widget_add_css_class(gtk_widget_get_last_child(book_page),
                             "heading");
    gtk_widget_add_css_class(gtk_widget_get_last_child(chapter_page),
                             "heading");

    {
        GtkWidget *book_notebook = gtk_notebook_new();
        const gchar *tab_labels[2] = { "旧约 / OT", "新约 / NT" };
        guint i;

        for (i = 0; i < 2; i++) {
            GtkWidget *book_scroll = gtk_scrolled_window_new();

            priv->book_lists[i] = GTK_LIST_BOX(gtk_list_box_new());
            gtk_list_box_set_selection_mode(priv->book_lists[i],
                                            GTK_SELECTION_NONE);
            gtk_list_box_set_activate_on_single_click(priv->book_lists[i],
                                                      TRUE);
            g_signal_connect(priv->book_lists[i], "row-activated",
                             G_CALLBACK(on_book_selected), self);
            gtk_scrolled_window_set_child(
                GTK_SCROLLED_WINDOW(book_scroll),
                GTK_WIDGET(priv->book_lists[i]));
            gtk_widget_set_vexpand(book_scroll, TRUE);
            gtk_notebook_append_page(
                GTK_NOTEBOOK(book_notebook), book_scroll,
                gtk_label_new(tab_labels[i]));
        }
        gtk_widget_set_size_request(book_notebook, 230, 320);
        gtk_widget_set_vexpand(book_notebook, TRUE);
        priv->book_notebook = GTK_NOTEBOOK(book_notebook);
        gtk_box_append(GTK_BOX(book_page), book_notebook);
    }

    priv->chapter_flow = GTK_FLOW_BOX(gtk_flow_box_new());
    gtk_flow_box_set_selection_mode(priv->chapter_flow, GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(priv->chapter_flow, 8);
    gtk_flow_box_set_min_children_per_line(priv->chapter_flow, 8);
    gtk_flow_box_set_column_spacing(priv->chapter_flow, 4);
    gtk_flow_box_set_row_spacing(priv->chapter_flow, 4);
    gtk_flow_box_set_activate_on_single_click(priv->chapter_flow, TRUE);
    chapter_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(chapter_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(chapter_scroll),
                                  GTK_WIDGET(priv->chapter_flow));
    gtk_widget_set_size_request(chapter_scroll, 360, 320);
    gtk_widget_set_vexpand(chapter_scroll, TRUE);
    gtk_box_append(GTK_BOX(chapter_page), chapter_scroll);

    {
        GtkWidget *verse_label = gtk_label_new("Verses / 节");

        gtk_widget_add_css_class(verse_label, "heading");
        gtk_box_append(GTK_BOX(chapter_page), verse_label);
    }
    priv->verse_flow = GTK_FLOW_BOX(gtk_flow_box_new());
    gtk_flow_box_set_selection_mode(priv->verse_flow, GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(priv->verse_flow, 8);
    gtk_flow_box_set_min_children_per_line(priv->verse_flow, 8);
    gtk_flow_box_set_column_spacing(priv->verse_flow, 4);
    gtk_flow_box_set_row_spacing(priv->verse_flow, 4);
    gtk_flow_box_set_activate_on_single_click(priv->verse_flow, TRUE);
    verse_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(verse_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(verse_scroll),
                                  GTK_WIDGET(priv->verse_flow));
    gtk_widget_set_size_request(verse_scroll, 360, 100);
    gtk_box_append(GTK_BOX(chapter_page), verse_scroll);

    gtk_paned_set_start_child(GTK_PANED(paned), book_page);
    gtk_paned_set_end_child(GTK_PANED(paned), chapter_page);
    gtk_paned_set_position(GTK_PANED(paned), 230);

    priv->hint = GTK_LABEL(gtk_label_new(
        "Type a reference or choose a book."));
    gtk_label_set_xalign(priv->hint, 0.0f);
    gtk_label_set_wrap(priv->hint, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(priv->hint), "dim-label");
    gtk_widget_set_margin_start(GTK_WIDGET(priv->hint), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(priv->hint), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(priv->hint), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(priv->hint), 6);

    popover_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(GTK_WIDGET(priv->filter_entry), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(priv->filter_entry), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(priv->filter_entry), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(priv->filter_entry), 8);
    gtk_box_append(GTK_BOX(popover_box), GTK_WIDGET(priv->filter_entry));
    gtk_box_append(GTK_BOX(popover_box), paned);
    gtk_box_append(GTK_BOX(popover_box), GTK_WIDGET(priv->hint));
    gtk_popover_set_child(priv->popover, popover_box);

    load_books(priv);
    populate_book_list(priv, NULL);
    populate_chapter_buttons(priv);
    populate_verse_buttons(priv);
    update_navigation_labels(self);
    if (priv->books != NULL && priv->books->len > 0) {
        gchar *text = g_strdup_printf("%s 1:1",
            book_display_name(priv, priv->book));
        priv->updating = TRUE;
        gtk_editable_set_text(GTK_EDITABLE(priv->entry), text);
        priv->updating = FALSE;
        g_free(text);
    }
}

GtkWidget *
gsword_reference_chooser_new(const gchar *versification,
                             const gchar *locale)
{
    GswordReferenceChooser *self = g_object_new(
        GSWORD_TYPE_REFERENCE_CHOOSER, NULL);

    if (versification != NULL && *versification != '\0')
        gsword_reference_chooser_set_versification(self, versification);
    if (locale != NULL && *locale != '\0')
        gsword_reference_chooser_set_locale(self, locale);
    return GTK_WIDGET(self);
}

gboolean
gsword_reference_parse(const gchar *text, const gchar *locale,
                       const gchar *versification, GswordReference *out,
                       GError **error)
{
    GswVersification *v11n;
    GswordReferenceChooserPrivate fake_priv;
    GError *local_error = NULL;
    gboolean result;

    v11n = gsw_versification_mgr_get_system(
        gsw_versification_mgr_get_default(),
        versification != NULL && *versification != '\0'
        ? versification : "KJV");
    if (v11n == NULL) {
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            "Unknown versification");
        return FALSE;
    }
    memset(&fake_priv, 0, sizeof(fake_priv));
    fake_priv.v11n = v11n;
    fake_priv.locale = g_strdup(locale != NULL && *locale != '\0'
                                ? locale : "en");
    result = parse_internal(text, fake_priv.locale, v11n, out, NULL,
                            &local_error);
    g_free(fake_priv.locale);
    if (!result) {
        g_propagate_error(error, local_error);
        return FALSE;
    }
    g_clear_error(&local_error);
    return TRUE;
}
