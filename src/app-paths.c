#include "app-paths.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

gchar *
gsword_app_data_dir(void)
{
    return g_build_filename(g_get_user_data_dir(), "gsword", NULL);
}

static gboolean
copy_file_if_missing(const gchar *source, const gchar *destination)
{
    GFile *src;
    GFile *dst;
    GError *error = NULL;
    gboolean ok;

    if (!g_file_test(source, G_FILE_TEST_IS_REGULAR) ||
        g_file_test(destination, G_FILE_TEST_EXISTS))
        return FALSE;

    if (g_rename(source, destination) == 0)
        return TRUE;

    src = g_file_new_for_path(source);
    dst = g_file_new_for_path(destination);
    ok = g_file_copy(src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
    g_clear_error(&error);
    g_object_unref(src);
    g_object_unref(dst);
    return ok;
}

static gboolean
directory_is_empty(const gchar *path)
{
    GDir *dir = g_dir_open(path, 0, NULL);
    const gchar *name;
    gboolean empty = TRUE;

    if (dir == NULL)
        return FALSE;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (g_strcmp0(name, ".") != 0 && g_strcmp0(name, "..") != 0) {
            empty = FALSE;
            break;
        }
    }
    g_dir_close(dir);
    return empty;
}

static void
merge_directory(const gchar *source, const gchar *destination)
{
    GDir *dir;
    const gchar *name;

    g_mkdir_with_parents(destination, 0755);
    dir = g_dir_open(source, 0, NULL);
    if (dir == NULL)
        return;

    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *source_path;
        gchar *destination_path;

        if (g_strcmp0(name, ".") == 0 || g_strcmp0(name, "..") == 0)
            continue;
        source_path = g_build_filename(source, name, NULL);
        destination_path = g_build_filename(destination, name, NULL);
        if (g_file_test(source_path, G_FILE_TEST_IS_DIR)) {
            merge_directory(source_path, destination_path);
        } else {
            copy_file_if_missing(source_path, destination_path);
        }
        g_free(source_path);
        g_free(destination_path);
    }
    g_dir_close(dir);
}

void
gsword_app_migrate_legacy_data(void)
{
    gchar *legacy_dir = g_build_filename(g_get_user_data_dir(),
                                         "gsword-app", NULL);
    gchar *data_dir = gsword_app_data_dir();
    gchar *legacy_db;
    gchar *new_db;
    gchar *legacy_installmgr;
    gchar *new_installmgr;

    if (!g_file_test(legacy_dir, G_FILE_TEST_IS_DIR)) {
        g_free(legacy_dir);
        g_free(data_dir);
        return;
    }

    g_mkdir_with_parents(data_dir, 0755);

    legacy_db = g_build_filename(legacy_dir, "app.db", NULL);
    new_db = g_build_filename(data_dir, "app.db", NULL);
    if (!copy_file_if_missing(legacy_db, new_db)) {
        /* If the new database already exists, leave both untouched. */
    }
    {
        const gchar *suffixes[] = { "-wal", "-shm", "-journal", NULL };
        guint i;

        for (i = 0; suffixes[i] != NULL; i++) {
            gchar *legacy_sidecar = g_strconcat(legacy_db, suffixes[i], NULL);
            gchar *new_sidecar = g_strconcat(new_db, suffixes[i], NULL);

            copy_file_if_missing(legacy_sidecar, new_sidecar);
            g_free(legacy_sidecar);
            g_free(new_sidecar);
        }
    }
    g_free(legacy_db);
    g_free(new_db);

    legacy_installmgr = g_build_filename(legacy_dir, "installmgr", NULL);
    new_installmgr = g_build_filename(data_dir, "installmgr", NULL);
    if (g_file_test(legacy_installmgr, G_FILE_TEST_IS_DIR)) {
        gboolean target_missing =
            !g_file_test(new_installmgr, G_FILE_TEST_EXISTS);
        gboolean target_empty =
            g_file_test(new_installmgr, G_FILE_TEST_IS_DIR) &&
            directory_is_empty(new_installmgr);

        if (target_empty)
            g_rmdir(new_installmgr);
        if (target_missing || target_empty) {
            if (g_rename(legacy_installmgr, new_installmgr) != 0)
                merge_directory(legacy_installmgr, new_installmgr);
        } else {
            merge_directory(legacy_installmgr, new_installmgr);
        }
    }
    g_free(legacy_installmgr);
    g_free(new_installmgr);

    /* Only removes the old directory when it is completely empty. */
    g_rmdir(legacy_dir);
    g_free(legacy_dir);
    g_free(data_dir);
}
