/*
 * gsw-transport.c: libcurl based HTTP/HTTPS/FTP transport
 */

#include <gsword/gsw-transport.h>
#include <gsword/gsw-error.h>
#include "gsw-utils.h"

#include <curl/curl.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* GswDirEntry                                                               */
/* ------------------------------------------------------------------------- */

struct _GswDirEntry {
    gatomicrefcount ref_count;
    gchar *name;
    guint64 size;
    gboolean is_directory;
};

static GswDirEntry *
gsw_dir_entry_alloc(const gchar *name, guint64 size, gboolean is_directory)
{
    GswDirEntry *self = g_new0(GswDirEntry, 1);

    g_atomic_ref_count_init(&self->ref_count);
    self->name = g_strdup(name != NULL ? name : "");
    self->size = size;
    self->is_directory = is_directory;
    return self;
}

static GswDirEntry *
gsw_dir_entry_copy(GswDirEntry *self)
{
    return gsw_dir_entry_alloc(self->name, self->size, self->is_directory);
}

GType
gsw_dir_entry_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswDirEntry",
            (GBoxedCopyFunc)gsw_dir_entry_copy,
            (GBoxedFreeFunc)gsw_dir_entry_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswDirEntry *
gsw_dir_entry_new(const gchar *name, guint64 size, gboolean is_directory)
{
    return gsw_dir_entry_alloc(name, size, is_directory);
}

GswDirEntry *
gsw_dir_entry_ref(GswDirEntry *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_atomic_ref_count_inc(&self->ref_count);
    return self;
}

void
gsw_dir_entry_unref(GswDirEntry *self)
{
    if (self == NULL)
        return;

    if (g_atomic_ref_count_dec(&self->ref_count)) {
        g_free(self->name);
        g_free(self);
    }
}

const gchar *
gsw_dir_entry_get_name(GswDirEntry *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->name;
}

guint64
gsw_dir_entry_get_size(GswDirEntry *self)
{
    g_return_val_if_fail(self != NULL, 0);
    return self->size;
}

gboolean
gsw_dir_entry_is_directory(GswDirEntry *self)
{
    g_return_val_if_fail(self != NULL, FALSE);
    return self->is_directory;
}

/* ------------------------------------------------------------------------- */
/* GswTransport                                                              */
/* ------------------------------------------------------------------------- */

typedef struct {
    GswSourceType type;
    gchar *host;
    gchar *user;
    gchar *password;
    gboolean passive;
    guint timeout_msec;
    gboolean verify_peer;
    GswStatusReporter *reporter;
} GswTransportPrivate;

G_DEFINE_TYPE(GswTransport, gsw_transport, G_TYPE_OBJECT)

static void
gsw_transport_private_free(gpointer data)
{
    GswTransportPrivate *priv = data;

    g_free(priv->host);
    g_free(priv->user);
    g_free(priv->password);
    g_clear_object(&priv->reporter);
    g_free(priv);
}

static GswTransportPrivate *
gsw_transport_priv(GswTransport *self)
{
    GswTransportPrivate *priv = g_object_get_data(G_OBJECT(self),
                                                  "gsw-transport-private");
    if (priv == NULL) {
        priv = g_new0(GswTransportPrivate, 1);
        priv->type = GSW_SOURCE_FTP;
        priv->host = g_strdup("");
        priv->user = g_strdup("ftp");
        priv->password = g_strdup("installmgr@user.com");
        priv->passive = TRUE;
        priv->timeout_msec = 10000;
        priv->verify_peer = TRUE;
        g_object_set_data_full(G_OBJECT(self), "gsw-transport-private",
                               priv, gsw_transport_private_free);
    }
    return priv;
}

static void
gsw_transport_class_init(GswTransportClass *klass)
{
    (void)klass;
}

static void
gsw_transport_init(GswTransport *self)
{
    (void)gsw_transport_priv(self);
}

GswSourceType
gsw_transport_get_source_type(GswTransport *self)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    return priv->type;
}

const gchar *
gsw_transport_get_host(GswTransport *self)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    return priv->host;
}

void
gsw_transport_set_credentials(GswTransport *self, const gchar *user,
                              const gchar *password)
{
    GswTransportPrivate *priv;

    g_return_if_fail(GSW_IS_TRANSPORT(self));
    priv = gsw_transport_priv(self);

    g_free(priv->user);
    priv->user = g_strdup(user != NULL ? user : "");
    g_free(priv->password);
    priv->password = g_strdup(password != NULL ? password : "");
}

void
gsw_transport_set_passive(GswTransport *self, gboolean passive)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    priv->passive = passive;
}

void
gsw_transport_set_timeout(GswTransport *self, guint timeout_msec)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    priv->timeout_msec = timeout_msec > 0 ? timeout_msec : 10000;
}

void
gsw_transport_set_verify_peer(GswTransport *self, gboolean verify)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    priv->verify_peer = verify;
}

void
gsw_transport_set_status_reporter(GswTransport *self, GswStatusReporter *reporter)
{
    GswTransportPrivate *priv;

    g_return_if_fail(GSW_IS_TRANSPORT(self));
    priv = gsw_transport_priv(self);
    g_set_object(&priv->reporter, reporter);
}

/* ------------------------------------------------------------------------- */
/* Curl implementation                                                       */
/* ------------------------------------------------------------------------- */

typedef struct {
    GswTransport *self;
    GOutputStream *out;
    GCancellable *cancellable;
    GswStatusReporter *reporter;
    GError *error;
    gboolean write_failed;
} GswCurlWriteContext;

typedef struct {
    GswTransport parent_instance;
} GswCurlTransport;

typedef struct {
    GswTransportClass parent_class;
} GswCurlTransportClass;

G_DEFINE_TYPE(GswCurlTransport, gsw_curl_transport, GSW_TYPE_TRANSPORT)

#define GSW_TYPE_CURL_TRANSPORT (gsw_curl_transport_get_type())

static gboolean gsw_curl_transport_get(GswTransport *self, const gchar *url,
                                       GOutputStream *out,
                                       GCancellable *cancellable,
                                       GError **error);
static GPtrArray *gsw_curl_transport_list_dir(GswTransport *self,
                                              const gchar *url,
                                              GCancellable *cancellable,
                                              GError **error);

static gboolean gsw_curl_global_initialized = FALSE;

static void
gsw_curl_ensure_global(void)
{
    static gsize once = 0;

    if (g_once_init_enter(&once)) {
        CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
        gsw_curl_global_initialized = (code == CURLE_OK);
        g_once_init_leave(&once, 1);
    }
}

static void
gsw_curl_transport_class_init(GswCurlTransportClass *klass)
{
    GswTransportClass *transport_class = GSW_TRANSPORT_CLASS(klass);

    transport_class->get = gsw_curl_transport_get;
    transport_class->list_dir = gsw_curl_transport_list_dir;
}

static void
gsw_curl_transport_init(GswCurlTransport *self)
{
    (void)self;
}

static size_t
gsw_curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    GswCurlWriteContext *ctx = userdata;
    gsize total = size * nmemb;
    gsize written = 0;
    GError *local_error = NULL;

    if (ctx->write_failed)
        return 0;

    if (!g_output_stream_write_all(ctx->out, ptr, total, &written,
                                   ctx->cancellable, &local_error)) {
        ctx->write_failed = TRUE;
        if (ctx->error == NULL)
            ctx->error = local_error;
        else
            g_error_free(local_error);
        return 0;
    }

    return written;
}

static int
gsw_curl_xferinfo_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t ultotal, curl_off_t ulnow)
{
    GswCurlWriteContext *ctx = clientp;

    (void)ultotal;
    (void)ulnow;

    if (ctx->cancellable != NULL && g_cancellable_is_cancelled(ctx->cancellable))
        return 1;

    if (ctx->reporter != NULL) {
        guint64 total = dltotal > 0 ? (guint64)dltotal : 0;
        guint64 done = dlnow > 0 ? (guint64)dlnow : 0;
        gsw_status_reporter_emit_progress(ctx->reporter, total, done);
    }

    return 0;
}

static gboolean
gsw_curl_setup(GswTransport *self, CURL *curl, const gchar *url,
               const gchar *custom_request, GswCurlWriteContext *ctx,
               GError **error)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    gchar *user_agent;

    if (curl == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NETWORK,
                    "Cannot initialize libcurl");
        return FALSE;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)priv->timeout_msec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)priv->timeout_msec);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, priv->verify_peer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, priv->verify_peer ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gsw_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, gsw_curl_xferinfo_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    user_agent = g_strdup_printf("gsword/%s", GSW_VERSION_S);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    g_free(user_agent);

    if (priv->user != NULL && *priv->user != '\0') {
        curl_easy_setopt(curl, CURLOPT_USERNAME, priv->user);
        curl_easy_setopt(curl, CURLOPT_PASSWORD,
                         priv->password != NULL ? priv->password : "");
    }

    if (!priv->passive && (priv->type == GSW_SOURCE_FTP || priv->type == GSW_SOURCE_SFTP))
        curl_easy_setopt(curl, CURLOPT_FTPPORT, "-");

    if (custom_request != NULL)
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, custom_request);

    return TRUE;
}

static void
gsw_curl_map_error(CURLcode code, long response_code, GCancellable *cancellable,
                   GError **error)
{
    if (code == CURLE_OK)
        return;

    if (code == CURLE_ABORTED_BY_CALLBACK &&
        cancellable != NULL && g_cancellable_is_cancelled(cancellable)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED,
                    "Transfer cancelled");
        return;
    }

    switch (code) {
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_CONNECT:
    case CURLE_OPERATION_TIMEDOUT:
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_PARTIAL_FILE:
    case CURLE_GOT_NOTHING:
        g_set_error(error, GSW_ERROR, GSW_ERROR_NETWORK,
                    "Network error: %s", curl_easy_strerror(code));
        break;
    case CURLE_LOGIN_DENIED:
        g_set_error(error, GSW_ERROR, GSW_ERROR_PERMISSION,
                    "Authentication failed: %s", curl_easy_strerror(code));
        break;
    case CURLE_REMOTE_FILE_NOT_FOUND:
    case CURLE_FTP_COULDNT_RETR_FILE:
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Remote resource not found");
        break;
    case CURLE_HTTP_RETURNED_ERROR:
        if (response_code == 404 || response_code == 410)
            g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                        "HTTP %ld: resource not found", response_code);
        else
            g_set_error(error, GSW_ERROR, GSW_ERROR_PROTOCOL,
                        "HTTP %ld: request failed", response_code);
        break;
    default:
        g_set_error(error, GSW_ERROR, GSW_ERROR_NETWORK,
                    "Transfer failed: %s", curl_easy_strerror(code));
        break;
    }
}

static gboolean
gsw_curl_get_full(GswTransport *self, const gchar *url, GOutputStream *out,
                  const gchar *custom_request, GCancellable *cancellable,
                  GError **error)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    GswCurlWriteContext ctx = { 0 };
    CURL *curl;
    CURLcode code;
    long response_code = 0;

    gsw_curl_ensure_global();
    if (!gsw_curl_global_initialized) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NETWORK,
                    "Cannot initialize libcurl");
        return FALSE;
    }

    curl = curl_easy_init();
    ctx.self = self;
    ctx.out = out;
    ctx.cancellable = cancellable;
    ctx.reporter = priv->reporter;

    if (!gsw_curl_setup(self, curl, url, custom_request, &ctx, error)) {
        if (curl != NULL)
            curl_easy_cleanup(curl);
        return FALSE;
    }

    code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    if (ctx.write_failed) {
        if (ctx.error != NULL)
            g_propagate_error(error, ctx.error);
        else if (cancellable != NULL && g_cancellable_is_cancelled(cancellable))
            g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED, "Transfer cancelled");
        else
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO, "Cannot write transfer data");
        return FALSE;
    }

    gsw_curl_map_error(code, response_code, cancellable, error);
    return code == CURLE_OK;
}

static gboolean
gsw_curl_transport_get(GswTransport *self, const gchar *url, GOutputStream *out,
                       GCancellable *cancellable, GError **error)
{
    return gsw_curl_get_full(self, url, out, NULL, cancellable, error);
}

static GPtrArray *
gsw_parse_html_dir(const gchar *html)
{
    GPtrArray *entries = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsw_dir_entry_unref);
    const gchar *p = html;

    while ((p = strstr(p, "<a href=\"")) != NULL) {
        const gchar *start;
        const gchar *end;
        gchar *href;
        gchar *name;
        gboolean is_dir;

        p += 9;
        start = p;
        end = strchr(start, '"');
        if (end == NULL)
            break;

        href = g_strndup(start, (gsize)(end - start));
        p = end + 1;

        if (href[0] == '\0' || href[0] == '?' || href[0] == '#' ||
            g_str_has_prefix(href, "../") || g_str_has_prefix(href, "/") ||
            g_str_has_prefix(href, "http://") || g_str_has_prefix(href, "https://")) {
            g_free(href);
            continue;
        }

        if (strchr(href, '/') != NULL && !g_str_has_suffix(href, "/")) {
            g_free(href);
            continue;
        }

        name = g_uri_unescape_string(href, NULL);
        g_free(href);
        if (name == NULL || *name == '\0') {
            g_free(name);
            continue;
        }

        is_dir = g_str_has_suffix(name, "/");
        if (is_dir)
            name[strlen(name) - 1] = '\0';

        if (g_strcmp0(name, ".") != 0 && g_strcmp0(name, "..") != 0 &&
            *name != '\0')
            g_ptr_array_add(entries, gsw_dir_entry_new(name, 0, is_dir));

        g_free(name);
    }

    return entries;
}

static GPtrArray *
gsw_parse_mlsd(const gchar *listing)
{
    GPtrArray *entries = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsw_dir_entry_unref);
    gchar **lines = g_strsplit(listing, "\n", -1);
    guint i;

    for (i = 0; lines[i] != NULL; i++) {
        gchar *line = g_strstrip(lines[i]);
        gchar *space;
        gchar **facts;
        const gchar *name;
        gboolean is_dir = FALSE;
        guint64 size = 0;
        guint j;

        if (*line == '\0')
            continue;

        space = strchr(line, ' ');
        if (space == NULL)
            continue;
        *space = '\0';
        name = space + 1;
        if (*name == '\0')
            continue;

        facts = g_strsplit(line, ";", -1);
        for (j = 0; facts[j] != NULL; j++) {
            gchar *fact = g_strstrip(facts[j]);
            if (g_str_has_prefix(fact, "type=")) {
                if (g_ascii_strcasecmp(fact + 5, "dir") == 0)
                    is_dir = TRUE;
            } else if (g_str_has_prefix(fact, "size=")) {
                size = gsw_str_parse_u64(fact + 5, 0);
            }
        }
        g_strfreev(facts);

        if (g_strcmp0(name, ".") != 0 && g_strcmp0(name, "..") != 0)
            g_ptr_array_add(entries, gsw_dir_entry_new(name, size, is_dir));
    }

    g_strfreev(lines);
    return entries;
}

static gchar **
gsw_split_whitespace(const gchar *line)
{
    gchar **raw = g_strsplit_set(line, " \t", -1);
    GPtrArray *tokens = g_ptr_array_new_with_free_func(g_free);
    guint i;

    for (i = 0; raw[i] != NULL; i++) {
        if (raw[i][0] != '\0')
            g_ptr_array_add(tokens, g_strdup(raw[i]));
    }
    g_strfreev(raw);
    g_ptr_array_add(tokens, NULL);
    return (gchar **)g_ptr_array_free(tokens, FALSE);
}

static GPtrArray *
gsw_parse_unix_list(const gchar *listing)
{
    GPtrArray *entries = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsw_dir_entry_unref);
    gchar **lines = g_strsplit(listing, "\n", -1);
    guint i;

    for (i = 0; lines[i] != NULL; i++) {
        gchar *line = g_strstrip(lines[i]);
        gchar **tokens;
        guint n;
        gboolean is_dir = FALSE;
        guint64 size = 0;
        GString *name;
        guint j;

        if (*line == '\0')
            continue;

        tokens = gsw_split_whitespace(line);
        for (n = 0; tokens[n] != NULL; n++)
            ;

        if (n < 9) {
            g_strfreev(tokens);
            continue;
        }

        if (tokens[0][0] == 'd')
            is_dir = TRUE;
        else if (tokens[0][0] == 'l') {
            g_strfreev(tokens);
            continue;
        }

        size = gsw_str_parse_u64(tokens[4], 0);
        name = g_string_new(tokens[8]);
        for (j = 9; j < n; j++) {
            g_string_append_c(name, ' ');
            g_string_append(name, tokens[j]);
        }

        if (g_strcmp0(name->str, ".") != 0 && g_strcmp0(name->str, "..") != 0)
            g_ptr_array_add(entries, gsw_dir_entry_new(name->str, size, is_dir));

        g_string_free(name, TRUE);
        g_strfreev(tokens);
    }

    g_strfreev(lines);
    return entries;
}

static GPtrArray *
gsw_parse_dos_list(const gchar *listing)
{
    GPtrArray *entries = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsw_dir_entry_unref);
    gchar **lines = g_strsplit(listing, "\n", -1);
    guint i;

    for (i = 0; lines[i] != NULL; i++) {
        gchar *line = g_strstrip(lines[i]);
        gchar **tokens;
        guint n;
        gboolean is_dir = FALSE;
        guint64 size = 0;
        GString *name;
        guint j;

        if (*line == '\0')
            continue;

        tokens = gsw_split_whitespace(line);
        for (n = 0; tokens[n] != NULL; n++)
            ;

        if (n < 4) {
            g_strfreev(tokens);
            continue;
        }

        if (g_ascii_strcasecmp(tokens[2], "<DIR>") == 0)
            is_dir = TRUE;
        else
            size = gsw_str_parse_u64(tokens[2], 0);

        name = g_string_new(tokens[3]);
        for (j = 4; j < n; j++) {
            g_string_append_c(name, ' ');
            g_string_append(name, tokens[j]);
        }

        if (g_strcmp0(name->str, ".") != 0 && g_strcmp0(name->str, "..") != 0)
            g_ptr_array_add(entries, gsw_dir_entry_new(name->str, size, is_dir));

        g_string_free(name, TRUE);
        g_strfreev(tokens);
    }

    g_strfreev(lines);
    return entries;
}

static GPtrArray *
gsw_curl_transport_list_dir(GswTransport *self, const gchar *url,
                            GCancellable *cancellable, GError **error)
{
    GswTransportPrivate *priv = gsw_transport_priv(self);
    GError *local_error = NULL;
    GBytes *bytes;
    const gchar *data;
    gsize size;

    if (priv->type == GSW_SOURCE_HTTP || priv->type == GSW_SOURCE_HTTPS) {
        bytes = gsw_transport_get_bytes(self, url, cancellable, error);
        if (bytes == NULL)
            return NULL;
        data = g_bytes_get_data(bytes, &size);
        {
            gchar *text = g_strndup(data, size);
            GPtrArray *entries = gsw_parse_html_dir(text);
            g_free(text);
            g_bytes_unref(bytes);
            return entries;
        }
    }

    /* FTP/SFTP: try MLSD first. */
    {
        GMemoryOutputStream *mem = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
        gboolean ok = gsw_curl_get_full(self, url, G_OUTPUT_STREAM(mem), "MLSD",
                                        cancellable, &local_error);
        if (ok) {
            GBytes *out;
            const gchar *text;
            GPtrArray *entries;

            g_output_stream_close(G_OUTPUT_STREAM(mem), NULL, NULL);
            out = g_memory_output_stream_steal_as_bytes(mem);
            text = g_bytes_get_data(out, &size);
            entries = gsw_parse_mlsd(text);
            g_bytes_unref(out);
            g_object_unref(mem);
            return entries;
        }
        g_clear_error(&local_error);
        g_object_unref(mem);
    }

    /* Fall back to LIST. */
    {
        GMemoryOutputStream *mem = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
        gboolean ok = gsw_curl_get_full(self, url, G_OUTPUT_STREAM(mem), NULL,
                                        cancellable, error);
        if (!ok) {
            g_object_unref(mem);
            return NULL;
        }
        {
            GBytes *out;
            const gchar *text;
            GPtrArray *entries;

            g_output_stream_close(G_OUTPUT_STREAM(mem), NULL, NULL);
            out = g_memory_output_stream_steal_as_bytes(mem);
            text = g_bytes_get_data(out, &size);
            /* Heuristic: DOS listings start with a date, Unix with perms. */
            if (text != NULL && size > 0 &&
                (g_ascii_isdigit(text[0]) || text[0] == ' '))
                entries = gsw_parse_dos_list(text);
            else
                entries = gsw_parse_unix_list(text);
            g_bytes_unref(out);
            g_object_unref(mem);
            return entries;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Public dispatch                                                           */
/* ------------------------------------------------------------------------- */

gboolean
gsw_transport_get(GswTransport *self, const gchar *url, GOutputStream *out,
                  GCancellable *cancellable, GError **error)
{
    GswTransportClass *klass;

    g_return_val_if_fail(GSW_IS_TRANSPORT(self), FALSE);
    g_return_val_if_fail(url != NULL, FALSE);
    g_return_val_if_fail(G_IS_OUTPUT_STREAM(out), FALSE);

    klass = GSW_TRANSPORT_GET_CLASS(self);
    if (klass->get == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "Transport does not implement get()");
        return FALSE;
    }
    return klass->get(self, url, out, cancellable, error);
}

GBytes *
gsw_transport_get_bytes(GswTransport *self, const gchar *url,
                        GCancellable *cancellable, GError **error)
{
    GMemoryOutputStream *mem;
    gboolean ok;

    mem = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
    ok = gsw_transport_get(self, url, G_OUTPUT_STREAM(mem), cancellable, error);
    if (!ok) {
        g_object_unref(mem);
        return NULL;
    }

    if (!g_output_stream_close(G_OUTPUT_STREAM(mem), cancellable, error)) {
        g_object_unref(mem);
        return NULL;
    }
    {
        GBytes *bytes = g_memory_output_stream_steal_as_bytes(mem);
        g_object_unref(mem);
        return bytes;
    }
}

GPtrArray *
gsw_transport_list_dir(GswTransport *self, const gchar *url,
                       GCancellable *cancellable, GError **error)
{
    GswTransportClass *klass;

    g_return_val_if_fail(GSW_IS_TRANSPORT(self), NULL);
    g_return_val_if_fail(url != NULL, NULL);

    klass = GSW_TRANSPORT_GET_CLASS(self);
    if (klass->list_dir == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "Transport does not implement list_dir()");
        return NULL;
    }
    return klass->list_dir(self, url, cancellable, error);
}

GswTransport *
gsw_transport_new_curl(GswSourceType type, const gchar *host, GError **error)
{
    GswTransport *self;
    GswTransportPrivate *priv;

    if (host == NULL || *host == '\0') {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Transport host is empty");
        return NULL;
    }

    self = g_object_new(GSW_TYPE_CURL_TRANSPORT, NULL);
    priv = gsw_transport_priv(self);
    priv->type = type;
    g_free(priv->host);
    priv->host = g_strdup(host);

    return self;
}
