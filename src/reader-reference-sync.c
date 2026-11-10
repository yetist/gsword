/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-reference-sync.c: reader reference chooser synchronization
 */

#include "reader-page-private.h"
#include "window-private.h"

#include <glib/gi18n.h>

void
gsword_reader_page_sync_context(GswordReaderPage *page)
{
    GswModule *module;

    if (page == NULL || page->reference_chooser == NULL)
        return;
    module = gsword_window_get_module(READER_WINDOW(page));
    if (module == NULL)
        return;
    gsword_reference_context_sync(&page->reference_context,
                                  page->reference_chooser, READER_WINDOW(page),
                                  module);
    if (page->compact_reference_chooser != NULL)
        gsword_reference_context_sync(&page->compact_reference_context,
                                      page->compact_reference_chooser,
                                      READER_WINDOW(page), module);
}

void
gsword_reader_page_sync_reference_chooser(GswordReaderPage *page)
{
    GswModule *module;
    gint range_start = 0;
    gint range_end = 0;

    if (page == NULL || page->reference_chooser == NULL)
        return;
    module = gsword_window_get_module(READER_WINDOW(page));
    if (page->range_active && page->range_start > 0 &&
        page->range_end > 0) {
        range_start = page->range_start;
        range_end = page->range_end;
    }
    gsword_reference_chooser_sync_from_module(
        page->reference_chooser, &page->reference_context, READER_WINDOW(page),
        module, range_start, range_end);
    if (page->compact_reference_chooser != NULL)
        gsword_reference_chooser_sync_from_module(
            page->compact_reference_chooser, &page->compact_reference_context,
            READER_WINDOW(page), module, range_start, range_end);
}


void
gsword_reader_page_update_reference_status(GswordReaderPage *page)
{
    GswModule *module = gsword_window_get_module(READER_WINDOW(page));

    if (module == NULL)
        return;
    gsword_reader_page_sync_reference_chooser(page);
    if (page->toolbar != NULL)
        gsword_reader_toolbar_update_favorite(page->toolbar);
    gsword_window_set_status(READER_WINDOW(page), gsw_module_get_name(module));
}

