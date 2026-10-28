#include <gsword/gsword.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    GswVerseKey *key;
    const gchar *versification = argc > 2 ? argv[2] : "KJV";

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s <key> [versification]\n", argv[0]);
        return 2;
    }

    key = gsw_verse_key_new(argv[1], versification);
    if (key == NULL) {
        fprintf(stderr, "cannot create key\n");
        return 1;
    }

    printf("Input:          %s\n", argv[1]);
    printf("Normalized:     %s\n", gsw_key_get_text((GswKey *)key));
    printf("Versification:  %s\n", gsw_verse_key_get_versification(key));
    printf("Testament:      %d\n", gsw_verse_key_get_testament(key));
    printf("Book:           %d (%s)\n", gsw_verse_key_get_book(key),
           gsw_verse_key_get_book_name(key));
    printf("Chapter:        %d (max %d)\n", gsw_verse_key_get_chapter(key),
           gsw_verse_key_get_chapter_max(key));
    printf("Verse:          %d (max %d)\n", gsw_verse_key_get_verse(key),
           gsw_verse_key_get_verse_max(key));
    printf("Valid:          %s\n", gsw_verse_key_is_valid(key) ? "yes" : "no");

    gsw_key_unref((GswKey *)key);

    printf("\nVersification systems:\n");
    {
        GPtrArray *systems = gsw_versification_mgr_get_systems(
            gsw_versification_mgr_get_default());
        guint i;
        for (i = 0; i < systems->len; i++) {
            GswVersification *system = g_ptr_array_index(systems, i);
            printf("  %-12s %d books\n", gsw_versification_get_name(system),
                   gsw_versification_get_book_count(system));
        }
        g_ptr_array_unref(systems);
    }

    return 0;
}
