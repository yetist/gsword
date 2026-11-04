/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-tar.c: safe tar.gz extraction for mods.d.tar.gz
 *
 * Only regular files and directories are accepted. Absolute paths,
 * "..", symbolic links, hard links, devices, FIFOs and oversize members
 * are rejected.
 */

#include "gsw-tar.h"
#include "gsw-utils.h"
#include <gsword/gsw-error.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#define GSW_TAR_BLOCK_SIZE 512
#define GSW_TAR_MAX_FILE_SIZE (64ULL * 1024 * 1024)
#define GSW_TAR_MAX_TOTAL_SIZE (512ULL * 1024 * 1024)
#define GSW_TAR_MAX_ENTRIES 100000

typedef struct {
    gchar name[100];
    gchar mode[8];
    gchar uid[8];
    gchar gid[8];
    gchar size[12];
    gchar mtime[12];
    gchar chksum[8];
    gchar typeflag;
    gchar linkname[100];
    gchar magic[6];
    gchar version[2];
    gchar uname[32];
    gchar gname[32];
    gchar devmajor[8];
    gchar devminor[8];
    gchar prefix[155];
    gchar pad[12];
} GswTarHeader;

typedef struct {
    guint64 total_size;
    guint entries;
} GswTarLimits;

static gboolean
gsw_tar_parse_octal(const gchar *field, gsize len, guint64 *value)
{
    guint64 result = 0;
    gsize i = 0;
    gboolean saw_digit = FALSE;

    while (i < len && (field[i] == ' ' || field[i] == '\0'))
        i++;

    if (i < len && ((guchar)field[i] & 0x80) != 0)
        return FALSE; /* GNU base-256 encoding is not supported. */

    for (; i < len; i++) {
        gchar c = field[i];

        if (c == '\0' || c == ' ')
            break;
        if (c < '0' || c > '7')
            return FALSE;
        saw_digit = TRUE;
        if (result > (G_MAXUINT64 >> 3))
            return FALSE;
        result = (result << 3) + (guint64)(c - '0');
    }

    *value = saw_digit ? result : 0;
    return TRUE;
}

static gboolean
gsw_tar_read_exact(gzFile gz, gchar *buffer, gsize len, GError **error)
{
    gsize done = 0;

    while (done < len) {
        int n = gzread(gz, buffer + done, (unsigned int)(len - done));
        if (n <= 0) {
            int errnum = 0;
            const gchar *msg = gzerror(gz, &errnum);
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Truncated gzip/tar archive: %s",
                        msg != NULL ? msg : "unexpected end of file");
            return FALSE;
        }
        done += (gsize)n;
    }
    return TRUE;
}

static gboolean
gsw_tar_skip(gzFile gz, guint64 len, GError **error)
{
    gchar buffer[GSW_TAR_BLOCK_SIZE];

    while (len > 0) {
        gsize chunk = len > sizeof(buffer) ? sizeof(buffer) : (gsize)len;
        if (!gsw_tar_read_exact(gz, buffer, chunk, error))
            return FALSE;
        len -= chunk;
    }
    return TRUE;
}

static gboolean
gsw_tar_is_zero_block(const gchar *block)
{
    gsize i;

    for (i = 0; i < GSW_TAR_BLOCK_SIZE; i++) {
        if (block[i] != '\0')
            return FALSE;
    }
    return TRUE;
}

static gchar *
gsw_tar_member_name(const GswTarHeader *header)
{
    gchar *name;
    gchar *prefix;

    name = g_strndup(header->name, strnlen(header->name, sizeof(header->name)));
    prefix = g_strndup(header->prefix,
                       strnlen(header->prefix, sizeof(header->prefix)));

    if (*prefix != '\0') {
        gchar *full = g_build_filename(prefix, name, NULL);
        g_free(name);
        g_free(prefix);
        return full;
    }

    g_free(prefix);
    return name;
}

gboolean
gsw_tar_extract_gz(const gchar *archive_path, const gchar *dest_dir,
                   GError **error)
{
    gzFile gz;
    gchar block[GSW_TAR_BLOCK_SIZE];
    GswTarLimits limits = { 0, 0 };

    g_return_val_if_fail(archive_path != NULL, FALSE);
    g_return_val_if_fail(dest_dir != NULL, FALSE);

    if (!g_file_test(archive_path, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Archive not found: %s", archive_path);
        return FALSE;
    }

    if (!gsw_mkdir_with_parents(dest_dir, error))
        return FALSE;

    gz = gzopen(archive_path, "rb");
    if (gz == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot open gzip archive %s", archive_path);
        return FALSE;
    }

    for (;;) {
        GswTarHeader *header;
        gchar *member;
        gchar *target;
        guint64 size;
        gchar type;

        if (!gsw_tar_read_exact(gz, block, GSW_TAR_BLOCK_SIZE, error))
            goto fail;

        if (gsw_tar_is_zero_block(block))
            break;

        limits.entries++;
        if (limits.entries > GSW_TAR_MAX_ENTRIES) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Archive has too many members");
            goto fail;
        }

        header = (GswTarHeader *)block;
        type = header->typeflag;

        if (!gsw_tar_parse_octal(header->size, sizeof(header->size), &size)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Invalid tar member size");
            goto fail;
        }

        if (size > GSW_TAR_MAX_FILE_SIZE) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Tar member is too large (%" G_GUINT64_FORMAT " bytes)",
                        size);
            goto fail;
        }

        if (limits.total_size > GSW_TAR_MAX_TOTAL_SIZE - size) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Archive expands beyond the configured limit");
            goto fail;
        }
        limits.total_size += size;

        member = gsw_tar_member_name(header);
        if (!gsw_path_is_safe_relative(member)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Unsafe tar member path: %s", member);
            g_free(member);
            goto fail;
        }

        {
            gchar *clean = gsw_str_remove_trailing_slashes(member);
            g_free(member);
            member = clean;
        }

        target = gsw_path_join_under(dest_dir, member, error);
        if (target == NULL) {
            g_free(member);
            goto fail;
        }

        if (type == '5' || (member[0] != '\0' && member[strlen(member) - 1] == '/')) {
            if (!gsw_mkdir_with_parents(target, error)) {
                g_free(target);
                g_free(member);
                goto fail;
            }
            if (!gsw_tar_skip(gz, size, error)) {
                g_free(target);
                g_free(member);
                goto fail;
            }
        } else if (type == '0' || type == '\0' || type == '7') {
            gchar *parent;
            int fd;
            guint64 remaining = size;

            parent = g_path_get_dirname(target);
            if (!gsw_mkdir_with_parents(parent, error)) {
                g_free(parent);
                g_free(target);
                g_free(member);
                goto fail;
            }
            g_free(parent);

            fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                            "Cannot create %s: %s", target, g_strerror(errno));
                g_free(target);
                g_free(member);
                goto fail;
            }

            while (remaining > 0) {
                gsize chunk = remaining > GSW_TAR_BLOCK_SIZE
                            ? GSW_TAR_BLOCK_SIZE : (gsize)remaining;
                ssize_t off = 0;

                if (!gsw_tar_read_exact(gz, block, chunk, error)) {
                    close(fd);
                    g_free(target);
                    g_free(member);
                    goto fail;
                }
                while (off < (ssize_t)chunk) {
                    ssize_t written = write(fd, block + off, chunk - (gsize)off);
                    if (written < 0) {
                        if (errno == EINTR)
                            continue;
                        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                                    "Cannot write %s: %s", target,
                                    g_strerror(errno));
                        close(fd);
                        g_free(target);
                        g_free(member);
                        goto fail;
                    }
                    off += written;
                }
                remaining -= chunk;
            }
            close(fd);

            {
                gsize padding = (gsize)((GSW_TAR_BLOCK_SIZE - (size % GSW_TAR_BLOCK_SIZE)) % GSW_TAR_BLOCK_SIZE);
                if (padding > 0 && !gsw_tar_skip(gz, padding, error)) {
                    g_free(target);
                    g_free(member);
                    goto fail;
                }
            }
        } else {
            /* Symlinks, hardlinks, devices and FIFOs are rejected. */
            g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                        "Unsupported tar member type '%c' for %s", type, member);
            g_free(target);
            g_free(member);
            goto fail;
        }

        g_free(target);
        g_free(member);
    }

    gzclose(gz);
    return TRUE;

fail:
    gzclose(gz);
    return FALSE;
}
