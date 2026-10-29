/*
 * gsw-utils.c: internal helpers
 *
 * This file is part of gsword.
 */

#include "gsw-utils.h"
#include <gsword/gsw-error.h>

#include <errno.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

gchar *
gsw_strdup_trim(const gchar *s)
{
    gchar *copy;

    if (s == NULL)
        return NULL;

    copy = g_strdup(s);
    g_strstrip(copy);
    return copy;
}

gchar *
gsw_str_remove_trailing_slashes(const gchar *s)
{
    gchar *copy;
    gsize len;

    if (s == NULL)
        return NULL;

    copy = g_strdup(s);
    len = strlen(copy);
    while (len > 1 && (copy[len - 1] == '/' || copy[len - 1] == '\\'))
        copy[--len] = '\0';
    return copy;
}

gboolean
gsw_str_ends_with_ci(const gchar *s, const gchar *suffix)
{
    gsize slen, tlen;

    if (s == NULL || suffix == NULL)
        return FALSE;

    slen = strlen(s);
    tlen = strlen(suffix);
    if (tlen > slen)
        return FALSE;

    return g_ascii_strcasecmp(s + slen - tlen, suffix) == 0;
}

gboolean
gsw_str_is_true(const gchar *s)
{
    if (s == NULL)
        return FALSE;

    return g_ascii_strcasecmp(s, "true") == 0 ||
           g_ascii_strcasecmp(s, "yes") == 0 ||
           g_ascii_strcasecmp(s, "on") == 0 ||
           strcmp(s, "1") == 0;
}

guint64
gsw_str_parse_u64(const gchar *s, guint64 fallback)
{
    gchar *end = NULL;
    guint64 value;

    if (s == NULL || *s == '\0')
        return fallback;

    value = g_ascii_strtoull(s, &end, 10);
    if (end == s || (end != NULL && *end != '\0'))
        return fallback;
    return value;
}


gchar *
gsw_utf8_strup(const gchar *s)
{
    return s != NULL ? g_utf8_strup(s, -1) : g_strdup("");
}

/* SWORD SWLD::strongsPad: pad a numeric Strong's key to five digits. */
gchar *
gsw_strongs_pad(const gchar *key)
{
    const gchar *p;
    const gchar *prefix_end;
    const gchar *digits_end;
    const gchar *suffix;
    GString *result;
    gsize digits;
    gint pad;
    gint i;

    if (key == NULL || *key == '\0')
        return g_strdup("");

    p = key;
    if (*p == 'G' || *p == 'H' || *p == 'g' || *p == 'h') {
        prefix_end = p + 1;
    } else {
        prefix_end = p;
    }

    digits_end = prefix_end;
    while (g_ascii_isdigit(*digits_end))
        digits_end++;

    suffix = digits_end;
    if (*suffix != '\0' && g_ascii_isalpha(*suffix))
        suffix++;
    if (*suffix != '\0')
        return g_strdup(key);

    digits = (gsize)(digits_end - prefix_end);
    if (digits == 0 || digits >= 5)
        return g_strdup(key);

    result = g_string_new(NULL);
    if (prefix_end != p)
        g_string_append_c(result, *p);
    pad = 5 - (gint)digits;
    for (i = 0; i < pad; i++)
        g_string_append_c(result, '0');
    g_string_append_len(result, prefix_end, (gssize)digits);
    if (*digits_end != '\0')
        g_string_append_c(result, *digits_end);
    return g_string_free(result, FALSE);
}

gboolean
gsw_path_is_safe_relative(const gchar *path)
{
    const gchar *p;

    if (path == NULL || *path == '\0')
        return FALSE;

    if (g_path_is_absolute(path))
        return FALSE;

    if (path[0] == '/' || path[0] == '\\')
        return FALSE;

    if (g_ascii_isalpha(path[0]) && path[1] == ':')
        return FALSE;

    p = path;
    while (*p != '\0') {
        const gchar *start;
        gsize len;

        while (*p == '/' || *p == '\\')
            p++;
        if (*p == '\0')
            break;

        start = p;
        while (*p != '\0' && *p != '/' && *p != '\\')
            p++;
        len = (gsize)(p - start);

        if (len == 2 && start[0] == '.' && start[1] == '.')
            return FALSE;
    }

    return TRUE;
}

gchar *
gsw_path_make_absolute(const gchar *path, const gchar *base)
{
    if (path == NULL)
        return NULL;

    if (g_path_is_absolute(path))
        return g_canonicalize_filename(path, NULL);

    return g_canonicalize_filename(path, base != NULL ? base : g_get_current_dir());
}

gboolean
gsw_path_is_within(const gchar *root, const gchar *path)
{
    gchar *canon_root;
    gchar *canon_path;
    gchar *prefix;
    gboolean result;

    if (root == NULL || path == NULL)
        return FALSE;

    canon_root = g_canonicalize_filename(root, NULL);
    canon_path = g_canonicalize_filename(path, NULL);
    prefix = g_strconcat(canon_root, G_DIR_SEPARATOR_S, NULL);

    result = g_str_has_prefix(canon_path, prefix) ||
             g_strcmp0(canon_root, canon_path) == 0;

    g_free(prefix);
    g_free(canon_path);
    g_free(canon_root);
    return result;
}

gchar *
gsw_path_join_under(const gchar *root, const gchar *relative, GError **error)
{
    gchar *joined;
    gchar *canonical;
    gchar *canon_root;
    gchar *prefix;

    if (!gsw_path_is_safe_relative(relative)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Unsafe relative path: %s", relative != NULL ? relative : "(null)");
        return NULL;
    }

    joined = g_build_filename(root, relative, NULL);
    canonical = g_canonicalize_filename(joined, NULL);
    g_free(joined);

    canon_root = g_canonicalize_filename(root, NULL);
    prefix = g_strconcat(canon_root, G_DIR_SEPARATOR_S, NULL);

    if (!g_str_has_prefix(canonical, prefix) &&
        g_strcmp0(canonical, canon_root) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Path escapes root: %s", relative);
        g_free(prefix);
        g_free(canon_root);
        g_free(canonical);
        return NULL;
    }

    g_free(prefix);
    g_free(canon_root);
    return canonical;
}

gchar *
gsw_path_make_temp_dir(const gchar *parent, const gchar *prefix, GError **error)
{
    const gchar *base;
    guint i;

    base = parent != NULL ? parent : g_get_tmp_dir();

    if (g_mkdir_with_parents(base, 0755) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot create directory %s: %s", base, g_strerror(errno));
        return NULL;
    }

    for (i = 0; i < 32; i++) {
        gchar *name = g_strdup_printf("%s-%08x", prefix,
                                      (guint)g_random_int());
        gchar *path = g_build_filename(base, name, NULL);

        g_free(name);
        if (g_mkdir(path, 0700) == 0)
            return path;
        if (errno != EEXIST) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot create temporary directory %s: %s",
                        path, g_strerror(errno));
            g_free(path);
            return NULL;
        }
        g_free(path);
    }

    g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                "Cannot create a unique temporary directory under %s", base);
    return NULL;
}

gboolean
gsw_mkdir_with_parents(const gchar *path, GError **error)
{
    if (path == NULL || *path == '\0')
        return TRUE;

    if (g_mkdir_with_parents(path, 0755) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot create directory %s: %s", path, g_strerror(errno));
        return FALSE;
    }
    return TRUE;
}

gboolean
gsw_file_copy(const gchar *src, const gchar *dst, GError **error)
{
    int in_fd = -1;
    int out_fd = -1;
    gchar buffer[64 * 1024];
    gboolean ok = FALSE;

    in_fd = open(src, O_RDONLY);
    if (in_fd < 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot open %s: %s", src, g_strerror(errno));
        return FALSE;
    }

    out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot create %s: %s", dst, g_strerror(errno));
        close(in_fd);
        return FALSE;
    }

    for (;;) {
        ssize_t n = read(in_fd, buffer, sizeof(buffer));
        ssize_t off = 0;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot read %s: %s", src, g_strerror(errno));
            goto out;
        }
        if (n == 0)
            break;

        while (off < n) {
            ssize_t written = write(out_fd, buffer + off, (size_t)(n - off));
            if (written < 0) {
                if (errno == EINTR)
                    continue;
                g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                            "Cannot write %s: %s", dst, g_strerror(errno));
                goto out;
            }
            off += written;
        }
    }

    ok = TRUE;

out:
    if (!ok)
        unlink(dst);
    close(in_fd);
    close(out_fd);
    return ok;
}

static gboolean
gsw_dir_copy_internal(const gchar *src, const gchar *dst, GError **error)
{
    GDir *dir;
    const gchar *name;
    GError *local_error = NULL;

    if (!gsw_mkdir_with_parents(dst, error))
        return FALSE;

    dir = g_dir_open(src, 0, &local_error);
    if (dir == NULL) {
        g_propagate_error(error, local_error);
        return FALSE;
    }

    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *src_child;
        gchar *dst_child;
        GStatBuf st;

        if (g_strcmp0(name, ".") == 0 || g_strcmp0(name, "..") == 0)
            continue;

        src_child = g_build_filename(src, name, NULL);
        dst_child = g_build_filename(dst, name, NULL);

        if (g_lstat(src_child, &st) != 0) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot stat %s: %s", src_child, g_strerror(errno));
            g_free(src_child);
            g_free(dst_child);
            g_dir_close(dir);
            return FALSE;
        }

        if (S_ISLNK(st.st_mode)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                        "Refusing to copy symbolic link %s", src_child);
            g_free(src_child);
            g_free(dst_child);
            g_dir_close(dir);
            return FALSE;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!gsw_dir_copy_internal(src_child, dst_child, error)) {
                g_free(src_child);
                g_free(dst_child);
                g_dir_close(dir);
                return FALSE;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (!gsw_file_copy(src_child, dst_child, error)) {
                g_free(src_child);
                g_free(dst_child);
                g_dir_close(dir);
                return FALSE;
            }
        }

        g_free(src_child);
        g_free(dst_child);
    }

    g_dir_close(dir);
    return TRUE;
}

gboolean
gsw_dir_copy_recursive(const gchar *src, const gchar *dst, GError **error)
{
    GStatBuf st;

    if (g_lstat(src, &st) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot stat %s: %s", src, g_strerror(errno));
        return FALSE;
    }
    if (!S_ISDIR(st.st_mode)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "%s is not a directory", src);
        return FALSE;
    }
    return gsw_dir_copy_internal(src, dst, error);
}

gboolean
gsw_dir_remove_recursive(const gchar *path, GError **error)
{
    GStatBuf st;

    if (path == NULL || g_lstat(path, &st) != 0) {
        if (errno == ENOENT)
            return TRUE;
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot stat %s: %s", path, g_strerror(errno));
        return FALSE;
    }

    if (S_ISDIR(st.st_mode)) {
        GDir *dir;
        const gchar *name;
        GError *local_error = NULL;
        gboolean ok = TRUE;

        dir = g_dir_open(path, 0, &local_error);
        if (dir == NULL) {
            g_propagate_error(error, local_error);
            return FALSE;
        }

        while ((name = g_dir_read_name(dir)) != NULL) {
            gchar *child;

            if (g_strcmp0(name, ".") == 0 || g_strcmp0(name, "..") == 0)
                continue;

            child = g_build_filename(path, name, NULL);
            if (!gsw_dir_remove_recursive(child, error)) {
                ok = FALSE;
                g_free(child);
                break;
            }
            g_free(child);
        }
        g_dir_close(dir);

        if (!ok)
            return FALSE;

        if (g_rmdir(path) != 0) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot remove directory %s: %s", path, g_strerror(errno));
            return FALSE;
        }
    } else {
        if (g_unlink(path) != 0) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot remove %s: %s", path, g_strerror(errno));
            return FALSE;
        }
    }

    return TRUE;
}

gchar *
gsw_build_url(const gchar *scheme, const gchar *host,
              const gchar *directory, const gchar *path)
{
    GString *url;

    g_return_val_if_fail(scheme != NULL, NULL);
    g_return_val_if_fail(host != NULL, NULL);

    url = g_string_new(scheme);
    g_string_append(url, "://");
    g_string_append(url, host);

    if (directory != NULL && *directory != '\0') {
        gchar *dir = gsw_str_remove_trailing_slashes(directory);
        const gchar *p = dir;

        while (*p == '/')
            p++;
        g_string_append_c(url, '/');
        g_string_append(url, p);
        g_free(dir);
    }

    if (path != NULL && *path != '\0') {
        const gchar *p = path;
        while (*p == '/')
            p++;
        if (url->len > 0 && url->str[url->len - 1] != '/')
            g_string_append_c(url, '/');
        g_string_append(url, p);
    }

    return g_string_free(url, FALSE);
}

GswDriver
gsw_driver_from_string(const gchar *name)
{
    if (name == NULL)
        return GSW_DRIVER_UNKNOWN;

    if (g_ascii_strcasecmp(name, "RawText") == 0)
        return GSW_DRIVER_RAW_TEXT;
    if (g_ascii_strcasecmp(name, "RawText4") == 0)
        return GSW_DRIVER_RAW_TEXT4;
    if (g_ascii_strcasecmp(name, "zText") == 0)
        return GSW_DRIVER_Z_TEXT;
    if (g_ascii_strcasecmp(name, "zText4") == 0)
        return GSW_DRIVER_Z_TEXT4;
    if (g_ascii_strcasecmp(name, "RawCom") == 0)
        return GSW_DRIVER_RAW_COM;
    if (g_ascii_strcasecmp(name, "RawCom4") == 0)
        return GSW_DRIVER_RAW_COM4;
    if (g_ascii_strcasecmp(name, "zCom") == 0)
        return GSW_DRIVER_Z_COM;
    if (g_ascii_strcasecmp(name, "zCom4") == 0)
        return GSW_DRIVER_Z_COM4;
    if (g_ascii_strcasecmp(name, "RawLD") == 0)
        return GSW_DRIVER_RAW_LD;
    if (g_ascii_strcasecmp(name, "RawLD4") == 0)
        return GSW_DRIVER_RAW_LD4;
    if (g_ascii_strcasecmp(name, "zLD") == 0)
        return GSW_DRIVER_Z_LD;
    if (g_ascii_strcasecmp(name, "RawGenBook") == 0)
        return GSW_DRIVER_RAW_GENBOOK;
    if (g_ascii_strcasecmp(name, "RawFiles") == 0)
        return GSW_DRIVER_RAW_FILES;
    if (g_ascii_strcasecmp(name, "HREFCom") == 0)
        return GSW_DRIVER_HREF_COM;

    return GSW_DRIVER_UNKNOWN;
}

const gchar *
gsw_driver_to_string(GswDriver driver)
{
    switch (driver) {
    case GSW_DRIVER_RAW_TEXT:     return "RawText";
    case GSW_DRIVER_RAW_TEXT4:    return "RawText4";
    case GSW_DRIVER_Z_TEXT:       return "zText";
    case GSW_DRIVER_Z_TEXT4:      return "zText4";
    case GSW_DRIVER_RAW_COM:      return "RawCom";
    case GSW_DRIVER_RAW_COM4:     return "RawCom4";
    case GSW_DRIVER_Z_COM:        return "zCom";
    case GSW_DRIVER_Z_COM4:       return "zCom4";
    case GSW_DRIVER_RAW_LD:       return "RawLD";
    case GSW_DRIVER_RAW_LD4:      return "RawLD4";
    case GSW_DRIVER_Z_LD:         return "zLD";
    case GSW_DRIVER_RAW_GENBOOK:  return "RawGenBook";
    case GSW_DRIVER_RAW_FILES:    return "RawFiles";
    case GSW_DRIVER_HREF_COM:     return "HREFCom";
    case GSW_DRIVER_UNKNOWN:
    default:                      return "";
    }
}

gboolean
gsw_driver_is_supported(GswDriver driver)
{
    switch (driver) {
    case GSW_DRIVER_RAW_TEXT:
    case GSW_DRIVER_RAW_TEXT4:
    case GSW_DRIVER_Z_TEXT:
    case GSW_DRIVER_Z_TEXT4:
    case GSW_DRIVER_RAW_COM:
    case GSW_DRIVER_RAW_COM4:
    case GSW_DRIVER_Z_COM:
    case GSW_DRIVER_Z_COM4:
    case GSW_DRIVER_RAW_LD:
    case GSW_DRIVER_RAW_LD4:
    case GSW_DRIVER_Z_LD:
    case GSW_DRIVER_RAW_GENBOOK:
    case GSW_DRIVER_RAW_FILES:
    case GSW_DRIVER_HREF_COM:
        return TRUE;
    default:
        return FALSE;
    }
}

GswModuleType
gsw_module_type_from_string(const gchar *name)
{
    if (name == NULL)
        return GSW_MODULE_TYPE_UNKNOWN;

    if (g_ascii_strcasecmp(name, "Biblical Texts") == 0)
        return GSW_MODULE_TYPE_BIBLES;
    if (g_ascii_strcasecmp(name, "Commentaries") == 0)
        return GSW_MODULE_TYPE_COMMENTARIES;
    if (g_ascii_strcasecmp(name, "Lexicons / Dictionaries") == 0)
        return GSW_MODULE_TYPE_LEXDICTS;
    if (g_ascii_strcasecmp(name, "Generic Books") == 0)
        return GSW_MODULE_TYPE_GENBOOKS;
    if (g_ascii_strcasecmp(name, "Daily Devotional") == 0)
        return GSW_MODULE_TYPE_DAILY_DEVOS;
    if (g_ascii_strcasecmp(name, "Maps") == 0)
        return GSW_MODULE_TYPE_MAPS;

    return GSW_MODULE_TYPE_OTHER;
}

const gchar *
gsw_module_type_to_string(GswModuleType type)
{
    switch (type) {
    case GSW_MODULE_TYPE_BIBLES:       return "Biblical Texts";
    case GSW_MODULE_TYPE_COMMENTARIES: return "Commentaries";
    case GSW_MODULE_TYPE_LEXDICTS:     return "Lexicons / Dictionaries";
    case GSW_MODULE_TYPE_GENBOOKS:     return "Generic Books";
    case GSW_MODULE_TYPE_DAILY_DEVOS:  return "Daily Devotional";
    case GSW_MODULE_TYPE_MAPS:         return "Maps";
    case GSW_MODULE_TYPE_OTHER:        return "Other";
    case GSW_MODULE_TYPE_UNKNOWN:
    default:                           return "";
    }
}

GswModuleType
gsw_module_type_default_for_driver(GswDriver driver)
{
    switch (driver) {
    case GSW_DRIVER_RAW_COM:
    case GSW_DRIVER_RAW_COM4:
    case GSW_DRIVER_Z_COM:
    case GSW_DRIVER_Z_COM4:
        return GSW_MODULE_TYPE_COMMENTARIES;
    case GSW_DRIVER_RAW_LD:
    case GSW_DRIVER_RAW_LD4:
    case GSW_DRIVER_Z_LD:
        return GSW_MODULE_TYPE_LEXDICTS;
    case GSW_DRIVER_RAW_GENBOOK:
        return GSW_MODULE_TYPE_GENBOOKS;
    case GSW_DRIVER_RAW_FILES:
    case GSW_DRIVER_HREF_COM:
        return GSW_MODULE_TYPE_COMMENTARIES;
    case GSW_DRIVER_RAW_TEXT:
    case GSW_DRIVER_RAW_TEXT4:
    case GSW_DRIVER_Z_TEXT:
    case GSW_DRIVER_Z_TEXT4:
        return GSW_MODULE_TYPE_BIBLES;
    default:
        return GSW_MODULE_TYPE_UNKNOWN;
    }
}

GswSourceType
gsw_source_type_from_string(const gchar *name)
{
    if (name == NULL)
        return GSW_SOURCE_FTP;
    if (g_ascii_strcasecmp(name, "HTTP") == 0)
        return GSW_SOURCE_HTTP;
    if (g_ascii_strcasecmp(name, "HTTPS") == 0)
        return GSW_SOURCE_HTTPS;
    if (g_ascii_strcasecmp(name, "SFTP") == 0)
        return GSW_SOURCE_SFTP;
    if (g_ascii_strcasecmp(name, "DIR") == 0)
        return GSW_SOURCE_DIR;
    return GSW_SOURCE_FTP;
}

