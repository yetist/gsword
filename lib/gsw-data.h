/*
 * gsw-data.h: small endian/file helpers
 */

#ifndef __GSW_DATA_H__
#define __GSW_DATA_H__ 1

#include <glib.h>
#include <gsword/gsw-error.h>
#include <stdio.h>
#include <string.h>

static inline guint16
gsw_le16(const guint8 *p)
{
    return (guint16)(p[0] | ((guint16)p[1] << 8));
}

static inline guint32
gsw_le32(const guint8 *p)
{
    return (guint32)(p[0] | ((guint32)p[1] << 8) |
                     ((guint32)p[2] << 16) | ((guint32)p[3] << 24));
}

static inline gboolean
gsw_file_read_at(FILE *fp, gint64 offset, gpointer buffer, gsize size,
                 GError **error)
{
    if (fseeko(fp, (off_t)offset, SEEK_SET) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Cannot seek in module data file");
        return FALSE;
    }
    if (size > 0 && fread(buffer, 1, size, fp) != size) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated module data file");
        return FALSE;
    }
    return TRUE;
}

static inline gint64
gsw_file_size(FILE *fp)
{
    off_t current = ftello(fp);
    off_t size;

    if (fseeko(fp, 0, SEEK_END) != 0)
        return -1;
    size = ftello(fp);
    if (current >= 0)
        fseeko(fp, current, SEEK_SET);
    return (gint64)size;
}

/*
 * Finds the last record in an index file whose size field is non-zero.
 * Returns -1 in @index when the index is empty or contains only empty entries.
 */
static inline gboolean
gsw_find_last_nonempty(FILE *idx, gsize rec_size, gsize size_offset,
                       gboolean size4, glong *index, GError **error)
{
    gint64 count = gsw_file_size(idx) / (gint64)rec_size;
    guint8 record[12];

    *index = -1;
    while (count > 0) {
        guint32 size;

        count--;
        if (!gsw_file_read_at(idx, count * (gint64)rec_size, record,
                              rec_size, error))
            return FALSE;
        size = size4 ? gsw_le32(record + size_offset)
                     : gsw_le16(record + size_offset);
        if (size > 0) {
            *index = (glong)count;
            return TRUE;
        }
    }
    return TRUE;
}

#endif /* __GSW_DATA_H__ */
