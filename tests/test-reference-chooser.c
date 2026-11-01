/*
 * test-reference-chooser.c: parser tests for GswordReferenceChooser
 */

#include "gsword-reference-chooser.h"

#include <glib.h>

typedef struct {
    const gchar *text;
    const gchar *locale;
    const gchar *osis;
    gint chapter;
    gint verse;
} ParseCase;

static int failures = 0;

static void
test_parse_case(const ParseCase *test)
{
    GswordReference reference;
    GError *error = NULL;
    gboolean ok;

    memset(&reference, 0, sizeof(reference));
    ok = gsword_reference_parse(test->text, test->locale, "KJV",
                                &reference, &error);
    if (!ok) {
        g_printerr("FAIL parse %s [%s]: %s\n", test->text, test->locale,
                   error != NULL ? error->message : "unknown error");
        failures++;
        g_clear_error(&error);
        gsword_reference_clear(&reference);
        return;
    }
    if (g_strcmp0(reference.osis, test->osis) != 0 ||
        reference.chapter != test->chapter ||
        reference.verse != test->verse) {
        g_printerr("FAIL parse %s [%s]: got %s %d:%d, expected %s %d:%d\n",
                   test->text, test->locale,
                   reference.osis != NULL ? reference.osis : "?",
                   reference.chapter, reference.verse,
                   test->osis, test->chapter, test->verse);
        failures++;
    } else {
        g_print("ok   %-18s -> %-5s %d:%d\n", test->text, reference.osis,
                reference.chapter, reference.verse);
    }
    gsword_reference_clear(&reference);
}

static void
test_invalid(const gchar *text, const gchar *locale,
             GswordReferenceError expected)
{
    GswordReference reference;
    GError *error = NULL;

    memset(&reference, 0, sizeof(reference));
    if (gsword_reference_parse(text, locale, "KJV", &reference, &error)) {
        g_printerr("FAIL expected invalid: %s\n", text);
        failures++;
    } else if (!g_error_matches(error, GSWORD_REFERENCE_ERROR, expected)) {
        g_printerr("FAIL wrong error for %s: %s\n", text,
                   error != NULL ? error->message : "none");
        failures++;
    } else {
        g_print("ok   invalid %-12s -> %s\n", text,
                error != NULL ? error->message : "");
    }
    g_clear_error(&error);
    gsword_reference_clear(&reference);
}

int
main(void)
{
    static const ParseCase cases[] = {
        { "Gen 1", "en", "Gen", 1, 1 },
        { "Ge1", "en", "Gen", 1, 1 },
        { "Genesis 1:1", "en", "Gen", 1, 1 },
        { "John 3:16", "en", "John", 3, 16 },
        { "Ps 23", "en", "Ps", 23, 1 },
        { "1 Cor 13", "en", "1Cor", 13, 1 },
        { "1co13:4", "en", "1Cor", 13, 4 },
        { "创1", "zh-Hans", "Gen", 1, 1 },
        { "诗23", "zh-Hans", "Ps", 23, 1 },
        { "林前13:4", "zh-Hans", "1Cor", 13, 4 },
    };
    guint i;

#ifdef GSW_TEST_LOCALE_DIR
    gsw_locale_mgr_load_directory(gsw_locale_mgr_get_default(),
                                  GSW_TEST_LOCALE_DIR, NULL);
#endif
    for (i = 0; i < G_N_ELEMENTS(cases); i++)
        test_parse_case(&cases[i]);

    test_invalid("Gen 51", "en", GSWORD_REFERENCE_ERROR_INVALID_CHAPTER);
    test_invalid("Gen 1:999", "en", GSWORD_REFERENCE_ERROR_INVALID_VERSE);
    test_invalid("Zzz 1", "en", GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK);
    test_invalid("1", "en", GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK);
    test_invalid("", "en", GSWORD_REFERENCE_ERROR_EMPTY);

    if (failures != 0) {
        g_printerr("%d reference parser test(s) failed\n", failures);
        return 1;
    }
    g_print("all reference parser tests passed\n");
    return 0;
}
