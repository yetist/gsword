/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-lock.c: small cross-process file lock helper
 */

#include "gsw-lock.h"

#include <gsword/gsw-error.h>

#include <errno.h>

#ifdef G_OS_UNIX
#include <fcntl.h>
#include <glib/gstdio.h>
#include <sys/file.h>
#include <unistd.h>
#endif

gint
gsw_file_lock(const gchar *path, GError **error)
{
#ifdef G_OS_UNIX
    gint fd = g_open(path, O_CREAT | O_RDWR, 0600);

    if (fd < 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot open lock file %s: %s", path, g_strerror(errno));
        return -1;
    }
    if (flock(fd, LOCK_EX) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot lock %s: %s", path, g_strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
#else
    (void)path;
    g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                "File locking is not supported on this platform");
    return -1;
#endif
}

void
gsw_file_unlock(gint fd)
{
#ifdef G_OS_UNIX
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
#else
    (void)fd;
#endif
}
