/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-str-key.h: string key for dictionaries
 *
 * This file is part of gsword.
 */

#ifndef __GSW_STR_KEY_H__
#define __GSW_STR_KEY_H__ 1

#include <gsword/gsw-key.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswStrKey:
 *
 * A string key for lexicon and dictionary modules. It is a specialised
 * #GswKey; use gsw_key_ref() and gsw_key_unref() for ownership.
 */
typedef GswKey GswStrKey;

#define GSW_TYPE_STR_KEY (gsw_str_key_get_type())

/**
 * gsw_str_key_get_type:
 *
 * Gets the #GType for #GswStrKey.
 *
 * Returns: the #GType for #GswStrKey
 */
GType       gsw_str_key_get_type(void);
/**
 * gsw_str_key_new:
 * @text: key text
 *
 * Creates a string key for dictionary modules.
 *
 * Returns: (transfer full): a new #GswStrKey
 */
GswStrKey  *gsw_str_key_new(const gchar *text);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswStrKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_STR_KEY_H__ */
