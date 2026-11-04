/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#ifndef GSWORD_APPLICATION_H
#define GSWORD_APPLICATION_H 1

#include <adwaita.h>

G_BEGIN_DECLS

#define GSWORD_TYPE_APPLICATION (gsword_application_get_type())
G_DECLARE_FINAL_TYPE(GswordApplication, gsword_application,
                     GSWORD, APPLICATION, AdwApplication)

GswordApplication *gsword_application_new(void);

G_END_DECLS

#endif /* GSWORD_APPLICATION_H */
