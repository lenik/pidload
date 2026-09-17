/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "view_state.hpp"
#include "util.hpp"

#include <bas/log/deflog.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

define_logger();

static int failures;

static void expect_eq_str(const char *name, const std::string &got, const char *want) {
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got [%s] want [%s]\n", name, got.c_str(), want);
        failures++;
    }
}

static void expect_true(const char *name, bool v) {
    if (!v) {
        std::fprintf(stderr, "FAIL %s: expected true\n", name);
        failures++;
    }
}

int main() {
    expect_eq_str("pid class", classify_name_token("18612"), "pid 18612");
    expect_eq_str("glob class", classify_name_token("bar*bar"), "glob bar*bar");

    /* Resolve a known PATH binary to an absolute path classification. */
    std::string ls_class = classify_name_token("ls");
    expect_true("ls is path", ls_class.compare(0, 5, "path ") == 0);
    expect_true("ls path absolute", ls_class.size() > 5 && ls_class[5] == '/');

    std::string material =
        view_state_key_material({"/bin/foo", "18612", "bar*bar"});
    /* /bin/foo may realpath to /usr/bin/foo on merged /usr; pin expected for literal path token
     * that already contains a slash — use classify of a path that may canonicalize. */
    std::string path_part = classify_name_token("/bin/foo");
    std::string want_material = path_part + ",pid 18612,glob bar*bar";
    expect_eq_str("material", material, want_material.c_str());

    /* Documented example when foo resolves to /bin/foo (PATH or literal). */
    expect_eq_str("example hash",
                  utf8sha1_hex("path /bin/foo,pid 18612,glob bar*bar"),
                  "877c7c108d7b30633cd80520c2fa9b135d63354e");

    char tmpl[] = "/tmp/pidload-state-XXXXXX";
    int fd = mkstemp(tmpl);
    expect_true("mkstemp", fd >= 0);
    if (fd >= 0) {
        close(fd);
        unlink(tmpl);
        ViewState st;
        st.show_cpu = true;
        st.show_legends = false;
        st.y_log = true;
        st.show_as = ChartShowAs::Bars;
        st.curve_style = CurveStyle::Bezier;
        st.net_unit = NetDisplayUnit::Packets;
        st.interval_ms = 5000;
        st.aui_perspective = "layout\\with\nnewline";
        expect_true("save", save_view_state(tmpl, st));

        ViewState loaded;
        expect_true("load", load_view_state(tmpl, loaded));
        expect_true("cpu", loaded.show_cpu);
        expect_true("legends off", !loaded.show_legends);
        expect_true("ylog", loaded.y_log);
        expect_true("show_as", loaded.show_as == ChartShowAs::Bars);
        expect_true("curve", loaded.curve_style == CurveStyle::Bezier);
        expect_true("net", loaded.net_unit == NetDisplayUnit::Packets);
        expect_true("interval", loaded.interval_ms == 5000);
        expect_eq_str("persp", loaded.aui_perspective, "layout\\with\nnewline");
        unlink(tmpl);
    }

    Options opt;
    opt.cli_cpu = true;
    opt.show_cpu = true;
    ViewState overlay;
    overlay.show_cpu = false;
    overlay.show_memory = true;
    overlay.y_log = true;
    apply_view_state(opt, overlay);
    expect_true("cli cpu kept", opt.show_cpu);
    expect_true("memory from state", opt.show_memory);
    expect_true("ylog from state", opt.y_log);

    return failures == 0 ? 0 : 1;
}
