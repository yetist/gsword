/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-error.c: error domain for gsword
 */

#include <gsword/gsw-error.h>

GQuark
gsw_error_quark(void)
{
    return g_quark_from_static_string("gsword-error-quark");
}
