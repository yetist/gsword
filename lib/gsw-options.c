/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-options.c: render and display option registry
 */

#include "gsw-options.h"

#include <string.h>

typedef struct {
    const gchar *name;
    const gchar *default_value;
    const gchar *values[6];
} GswOptionSpec;

static const gchar *on_off_values[] = { "On", "Off", NULL };
static const gchar *transliteration_values[] = { "Off", "Latin", NULL };
static const gchar *variant_values[] = {
    "Off", "Primary Reading", "Secondary Reading", "All Readings", NULL
};

static const GswOptionSpec option_specs[] = {
    { "Strong's Numbers", "Off", { "On", "Off", NULL } },
    { "Lemmas", "Off", { "On", "Off", NULL } },
    { "Morphology", "Off", { "On", "Off", NULL } },
    { "Morphological Tags", "Off", { "On", "Off", NULL } },
    { "Footnotes", "Off", { "On", "Off", NULL } },
    { "Cross-references", "Off", { "On", "Off", NULL } },
    { "Headings", "Off", { "On", "Off", NULL } },
    { "Italic Headings", "Off", { "On", "Off", NULL } },
    { "Red Letter Words", "Off", { "On", "Off", NULL } },
    { "Words of Christ in Red", "Off", { "On", "Off", NULL } },
    { "Greek Accents", "On", { "On", "Off", NULL } },
    { "Hebrew Vowel Points", "On", { "On", "Off", NULL } },
    { "Hebrew Cantillation", "On", { "On", "Off", NULL } },
    { "Arabic Vowel Points", "On", { "On", "Off", NULL } },
    { "Transliteration", "Off", { "Off", "Latin", NULL } },
    { "Morpheme Segmentation", "Off", { "On", "Off", NULL } },
    { "Textual Variants", "Off",
      { "Off", "Primary Reading", "Secondary Reading", "All Readings", NULL } },
    { "Glosses", "Off", { "On", "Off", NULL } },
    { "Images", "Off", { "On", "Off", NULL } },
    { "Enumerations", "Off", { "On", "Off", NULL } },
    { "Variants", "Off", { "On", "Off", NULL } },
};

void
gsw_options_init(GHashTable *options)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(option_specs); i++)
        g_hash_table_replace(options, g_strdup(option_specs[i].name),
                             g_strdup(option_specs[i].default_value));
}

gboolean
gsw_options_known(const gchar *name)
{
    guint i;

    if (name == NULL)
        return FALSE;
    for (i = 0; i < G_N_ELEMENTS(option_specs); i++) {
        if (g_strcmp0(option_specs[i].name, name) == 0)
            return TRUE;
    }
    return FALSE;
}

GPtrArray *
gsw_options_values(const gchar *name)
{
    const gchar **values = on_off_values;
    GPtrArray *result;
    guint i;

    if (name == NULL)
        return NULL;

    if (g_strcmp0(name, "Transliteration") == 0)
        values = transliteration_values;
    else if (g_strcmp0(name, "Textual Variants") == 0)
        values = variant_values;

    if (!gsw_options_known(name))
        return NULL;

    result = g_ptr_array_new_with_free_func(g_free);
    for (i = 0; values[i] != NULL; i++)
        g_ptr_array_add(result, g_strdup(values[i]));
    return result;
}
