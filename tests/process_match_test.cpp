/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "process_match.hpp"
#include "util.hpp"

#include <bas/log/deflog.h>

#include <cstdio>
#include <string>
#include <unistd.h>

define_logger();

static int failures;

static void expect_true(const char *name, bool v) {
    if (!v) {
        std::fprintf(stderr, "FAIL %s: expected true\n", name);
        failures++;
    }
}

static void expect_false(const char *name, bool v) {
    if (v) {
        std::fprintf(stderr, "FAIL %s: expected false\n", name);
        failures++;
    }
}

int main() {
    expect_true("pid token", is_pid_token("1234"));
    expect_false("not pid", is_pid_token("12a"));
    expect_false("empty", is_pid_token(""));
    expect_true("glob", name_has_glob("ed*tor"));
    expect_true("glob?", name_has_glob("edit?r"));
    expect_false("no glob", name_has_glob("editor"));

    pid_t self = getpid();
    auto by_pid = resolve_name(std::to_string(self));
    expect_true("resolve self pid", !by_pid.empty() && by_pid[0].pid == self);

    auto tracked = resolve_names({std::to_string(self)});
    expect_true("tracked style0", !tracked.empty() && tracked[0].style_index == 0);

    expect_true("safe dots", safe_filename("ed.itor") == "ed.itor");

    return failures == 0 ? 0 : 1;
}
