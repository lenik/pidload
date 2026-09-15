/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_UTIL_HPP
#define PIDLOAD_UTIL_HPP

#include <cstdint>
#include <string>
#include <vector>

/* Parse durations like "2", "2s", ".5s", "5min", "3m", "100ms", "1h".
 * Default unit is seconds. Returns milliseconds, or -1 on error. */
int64_t parse_duration_ms(const char *text);

/* Strip "/dev/" prefix and return the kernel diskstats name. */
std::string device_basename(const std::string &path);

/* Sanitize a name for use as a log file basename. */
std::string safe_filename(const std::string &name);

bool ensure_directory(const std::string &path);

std::vector<std::string> list_net_ifaces();

#endif /* PIDLOAD_UTIL_HPP */
