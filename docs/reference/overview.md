# Overview

gsword is a C/GObject implementation of the SWORD module library. It is
organized around a module library manager, immutable module metadata, and
lazily opened module readers.

## Basic usage

Open a module library, look up a module, set a key, and read text. The
library owns the returned module and metadata objects; only values that are
explicitly annotated as `(transfer full)` must be freed by the caller.

```c
GError *error = NULL;
gchar *prefix = g_build_filename(g_get_home_dir(), ".sword", NULL);
GswManager *manager = gsw_manager_new_for_path(prefix, &error);
GswModule *module;
gchar *text;

g_free(prefix);
if (manager == NULL) {
    g_printerr("Cannot open module library: %s\n", error->message);
    g_clear_error(&error);
    return;
}

module = gsw_manager_get_module(manager, "KJV");
if (module == NULL ||
    !gsw_module_set_key_text(module, "John 3:16", &error)) {
    g_printerr("Cannot read John 3:16: %s\n",
               error != NULL ? error->message : "module not found");
    g_clear_error(&error);
    g_object_unref(manager);
    return;
}

text = gsw_module_get_text(module, &error);
g_print("%s\n", text);
g_free(text);
g_object_unref(manager);
```

Module storage is opened lazily. Looking up metadata does not read the module
data files; the backend is opened the first time that a key or entry is
accessed.

## Installing modules

[class@GSword.InstallManager] reads `InstallMgr.conf`, refreshes remote sources,
reports progress through [class@GSword.StatusReporter], and installs modules
atomically. Synchronous and GTask-based asynchronous APIs are available.

Connect to the [signal@GSword.StatusReporter::pre-status] signal for status
messages and to [signal@GSword.StatusReporter::progress] for byte-level progress:

```c
static void
on_progress(GswStatusReporter *reporter,
            guint64 total_bytes,
            guint64 completed_bytes,
            gpointer user_data)
{
    (void)reporter;
    (void)user_data;
    if (total_bytes > 0)
        g_print("\r%" G_GUINT64_FORMAT " / %" G_GUINT64_FORMAT,
                completed_bytes, total_bytes);
}

GswStatusReporter *reporter = gsw_status_reporter_new();
GswInstallSource *source;
gchar *private_path =
    g_build_filename(g_get_home_dir(), ".sword", "InstallMgr", NULL);
GswInstallManager *installer =
    gsw_install_manager_new(private_path, &error);

g_free(private_path);
g_signal_connect(reporter, "progress", G_CALLBACK(on_progress), NULL);
gsw_install_manager_set_status_reporter(installer, reporter);

source = gsw_install_manager_lookup_source(installer, "CrossWire");
if (source != NULL &&
    gsw_install_manager_refresh_remote_source(installer, source, &error) &&
    gsw_install_manager_install_module(installer, manager, source, "KJV", &error)) {
    g_print("\nKJV installed\n");
}

g_object_unref(reporter);
g_object_unref(installer);
```

The asynchronous variants use the standard GIO pattern: call
[method@GSword.InstallManager.install_module_async] and then call
[method@GSword.InstallManager.install_module_finish] from the
[type@Gio.AsyncReadyCallback].

## Keys and versification

[class@GSword.Module] exposes a module-compatible key through
[method@GSword.Module.create_key]. Bible modules use [alias@GSword.VerseKey],
dictionaries use [alias@GSword.StrKey], and general books use
[alias@GSword.TreeKey]. Verse keys can also be detached from a module and used
with a versification system directly.

[class@GSword.VersificationMgr] provides the built-in KJV, Catholic, and other
versification systems. Book names are localized through
[class@GSword.LocaleMgr].

## Memory and ownership

GObject-based types are reference counted and released with
[method@GObject.Object.unref]. Boxed key, versification, and directory-entry types
use their own `gsw_*_ref()` and `gsw_*_unref()` functions. Functions returning
arrays document their transfer mode; in particular, caller-owned arrays
returned with `(transfer full)` must be freed with `g_strfreev()` or
`g_ptr_array_unref()` as appropriate.

## Supported drivers

Readers are provided for RawText, zText, RawCom, zCom, RawLD, zLD, RawGenBook,
RawFiles, and HREFCom. Compression support includes LZSS, zlib, bzip2, and xz.
Encrypted modules use SWORD-compatible Sapphire decryption.

## Searching

[method@GSword.Module.search] provides phrase, multiword, and regular-expression
searches over a module and returns matching key texts.

## GObject Introspection

Configure with `-Dintrospection=true` to generate `GSword-0.1.gir` and
`GSword-0.1.typelib` for Python, JavaScript, and other GObject binding consumers.
The documentation build enables GIR generation automatically.

## Building this reference

The API reference is generated with [gi-docgen](https://gitlab.gnome.org/GNOME/gi-docgen)
from the GIR data and the public headers:

```sh
meson setup build-doc -Ddocs=true
meson compile -C build-doc docs
```

The generated HTML is written to
`build-doc/docs/reference/gsword/index.html` and can be installed with
`meson install -C build-doc`.
