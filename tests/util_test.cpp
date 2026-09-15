/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "util.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static int failures;

static void expect_eq_i64(const char *name, long long got, long long want) {
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got %lld want %lld\n", name, got, want);
        failures++;
    }
}

static void expect_eq_str(const char *name, const std::string &got, const char *want) {
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got %s want %s\n", name, got.c_str(), want);
        failures++;
    }
}

int main() {
    expect_eq_i64("2s", parse_duration_ms("2s"), 2000);
    expect_eq_i64("2", parse_duration_ms("2"), 2000);
    expect_eq_i64(".5s", parse_duration_ms(".5s"), 500);
    expect_eq_i64("5min", parse_duration_ms("5min"), 5 * 60 * 1000);
    expect_eq_i64("3min", parse_duration_ms("3min"), 3 * 60 * 1000);
    expect_eq_i64("100ms", parse_duration_ms("100ms"), 100);
    expect_eq_i64("bad", parse_duration_ms("xyz"), -1);

    expect_eq_str("dev", device_basename("/dev/sda"), "sda");
    expect_eq_str("dev2", device_basename("nvme0n1"), "nvme0n1");
    expect_eq_str("safe", safe_filename("127.0.0.1"), "127.0.0.1");
    expect_eq_str("safe2", safe_filename("a/b"), "a_b");

    return failures == 0 ? 0 : 1;
}
