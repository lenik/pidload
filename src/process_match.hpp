/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_PROCESS_MATCH_HPP
#define PIDLOAD_PROCESS_MATCH_HPP

#include <sys/types.h>

#include <string>
#include <vector>

struct MatchedProcess {
    pid_t pid = 0;
    std::string name;  /* the NAME pattern that matched */
    std::string label; /* short display label (comm or basename) */
    std::string exe;   /* resolved executable path if known */
};

bool name_has_glob(const std::string &name);

/* True if s looks like a positive decimal PID. */
bool is_pid_token(const std::string &s);

/* Match one NAME against running processes (pid / exe name / path / window
 * title / globs). Window titles use X11 when DISPLAY is available. */
std::vector<MatchedProcess> resolve_name(const std::string &name);

/* Resolve every NAME; assigns stable style indices in encounter order.
 * duplicate PIDs from overlapping names are kept once (first name wins). */
struct TrackedProcess {
    pid_t pid = 0;
    std::string name;
    std::string label;
    std::string exe;
    size_t style_index = 0;
};

std::vector<TrackedProcess> resolve_names(const std::vector<std::string> &names);

#endif /* PIDLOAD_PROCESS_MATCH_HPP */
