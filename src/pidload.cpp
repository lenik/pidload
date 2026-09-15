/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "config.h"
#include "main_frame.hpp"
#include "options.hpp"

#include <bas/locale/i18n.h>
#include <bas/log/deflog.h>

extern "C" {
#include <bas/proc/env.h>
}

#include <wx/app.h>

define_logger();

class PidloadApp : public wxApp {
public:
    bool OnInit() override;
};

static Options g_options;
static int g_parse_rc = 0;

bool PidloadApp::OnInit() {
    /* Skip wxApp::OnInit() cmdline parsing; options were handled in main(). */
    if (g_parse_rc != 0) {
        return false;
    }

    auto *frame = new MainFrame(g_options);
    frame->Show(true);
    SetTopWindow(frame);
    return true;
}

wxIMPLEMENT_APP_NO_MAIN(PidloadApp);

int main(int argc, char **argv) {
    const char *exe = self_exe();
    if (!exe || !*exe) {
        exe = argc > 0 ? argv[0] : "pidload";
    }
    init_i18n(LOCALEDIR);

    g_parse_rc = parse_options(argc, argv, g_options);
    if (g_parse_rc == 2) {
        return 0;
    }
    if (g_parse_rc != 0) {
        return 1;
    }

    loginfo_fmt("%s: interval=%lldms window=%lldms devices=%zu ifaces=%s addrs=%zu names=%zu "
                "cpu=%d mem=%d",
                exe, static_cast<long long>(g_options.interval_ms),
                static_cast<long long>(g_options.window_ms), g_options.devices.size(),
                g_options.iface_all ? "all" : (g_options.monitor_network ? "listed" : "off"),
                g_options.addrs.size(), g_options.names.size(), g_options.show_cpu ? 1 : 0,
                g_options.show_memory ? 1 : 0);

    /* Feed wx only the program name so it does not re-parse our options. */
    char *wx_argv[] = {argv[0], nullptr};
    int wx_argc = 1;
    return wxEntry(wx_argc, wx_argv);
}
