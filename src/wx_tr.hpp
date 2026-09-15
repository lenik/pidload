/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_WX_TR_HPP
#define PIDLOAD_WX_TR_HPP

#include <bas/locale/i18n.h>
#include <wx/string.h>

/* gettext → wxString (UTF-8). Mark the English with _() for xgettext. */
inline wxString tr(const char *msgid) {
    return wxString::FromUTF8(_(msgid));
}

#endif /* PIDLOAD_WX_TR_HPP */
