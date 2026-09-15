/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_DIALOGS_HPP
#define PIDLOAD_DIALOGS_HPP

#include <wx/dialog.h>
#include <wx/listctrl.h>

#include <sys/types.h>
#include <vector>

/* Returns selected PIDs (empty if cancelled). */
std::vector<pid_t> ShowProcessPicker(wxWindow *parent);

/* Ask for a single PID. Returns -1 if cancelled. */
pid_t ShowOpenPidDialog(wxWindow *parent);

enum class CaptureKind { Device, Iface, Addr, Pid };

struct CaptureRequest {
    CaptureKind kind;
    wxString value;
};

/* Returns true if user confirmed. */
bool ShowAddCaptureDialog(wxWindow *parent, CaptureRequest *out);

#endif /* PIDLOAD_DIALOGS_HPP */
