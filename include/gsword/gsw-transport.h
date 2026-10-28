/*
 * gsw-transport.h: remote transfer abstraction
 *
 * This file is part of gsword.
 */

#ifndef __GSW_TRANSPORT_H__
#define __GSW_TRANSPORT_H__ 1

#include <gio/gio.h>
#include <gsword/gsw-enums.h>
#include <gsword/gsw-status-reporter.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

#define GSW_TYPE_DIR_ENTRY (gsw_dir_entry_get_type())
typedef struct _GswDirEntry GswDirEntry;

GType        gsw_dir_entry_get_type(void);
GswDirEntry *gsw_dir_entry_new(const gchar *name, guint64 size, gboolean is_directory);
GswDirEntry *gsw_dir_entry_ref(GswDirEntry *self);
void         gsw_dir_entry_unref(GswDirEntry *self);
const gchar *gsw_dir_entry_get_name(GswDirEntry *self);
guint64      gsw_dir_entry_get_size(GswDirEntry *self);
gboolean     gsw_dir_entry_is_directory(GswDirEntry *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswDirEntry, gsw_dir_entry_unref)

#define GSW_TYPE_TRANSPORT (gsw_transport_get_type())
G_DECLARE_DERIVABLE_TYPE(GswTransport, gsw_transport, GSW, TRANSPORT, GObject)

struct _GswTransportClass {
    GObjectClass parent_class;

    gboolean   (*get)(GswTransport *self, const gchar *url, GOutputStream *out,
                      GCancellable *cancellable, GError **error);
    GPtrArray *(*list_dir)(GswTransport *self, const gchar *url,
                           GCancellable *cancellable, GError **error);
};

GswTransport *gsw_transport_new_curl(GswSourceType type,
                                     const gchar *host,
                                     GError **error);

GswSourceType     gsw_transport_get_source_type(GswTransport *self);
const gchar      *gsw_transport_get_host(GswTransport *self);
void              gsw_transport_set_credentials(GswTransport *self,
                                                const gchar *user,
                                                const gchar *password);
void              gsw_transport_set_passive(GswTransport *self, gboolean passive);
void              gsw_transport_set_timeout(GswTransport *self, guint timeout_msec);
void              gsw_transport_set_verify_peer(GswTransport *self, gboolean verify);
void              gsw_transport_set_status_reporter(GswTransport *self,
                                                    GswStatusReporter *reporter);

gboolean   gsw_transport_get(GswTransport *self, const gchar *url,
                             GOutputStream *out, GCancellable *cancellable,
                             GError **error);
GBytes    *gsw_transport_get_bytes(GswTransport *self, const gchar *url,
                                   GCancellable *cancellable, GError **error);
/**
 * gsw_transport_list_dir:
 *
 * Returns: (transfer container) (element-type GswDirEntry): result
 */
GPtrArray *gsw_transport_list_dir(GswTransport *self, const gchar *url,
                                  GCancellable *cancellable, GError **error);

G_END_DECLS

#endif /* __GSW_TRANSPORT_H__ */
