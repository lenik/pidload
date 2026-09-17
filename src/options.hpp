/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_OPTIONS_HPP
#define PIDLOAD_OPTIONS_HPP

#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/types.h>
#include <vector>

enum class ChartShowAs {
    Dots = 0,
    Curve,
    Bars,
    StackedBars,
};

/* Interpolation used when Show-as is Curve. */
enum class CurveStyle {
    Segment = 0, /* polyline */
    Bezier,      /* quadratic Bezier through midpoints */
    Bicubic,     /* Catmull-Rom cubic spline */
};

enum class NetDisplayUnit {
    RawSize = 0,   /* wire bytes from /proc/net/dev */
    PayloadSize,   /* approx. bytes minus Ethernet header per packet */
    Packets,
};

struct Options {
    std::vector<std::string> devices;
    std::vector<std::string> ifaces; /* empty + iface_all => all */
    bool iface_all = false;
    bool iface_explicit = false;
    bool monitor_network = false; /* true if -i given (incl. default all) */
    std::vector<std::string> addrs;
    /* NAME args: pid / exe name / path / window title / globs */
    std::vector<std::string> names;
    bool show_cpu = false;    /* system CPU overall + cores */
    bool show_memory = false; /* system memory */
    bool show_threads = false;
    bool show_numfd = false;
    bool show_connections = false;
    int64_t interval_ms = 2000;
    int64_t window_ms = 3 * 60 * 1000; /* capture / default visible window */
    std::string output_dir;
    bool show_legends = true;
    bool y_log = false;
    ChartShowAs show_as = ChartShowAs::Curve;
    CurveStyle curve_style = CurveStyle::Segment;
    NetDisplayUnit net_unit = NetDisplayUnit::RawSize;

    /* True when the corresponding CLI flag was given (overrides saved view state). */
    bool cli_cpu = false;
    bool cli_memory = false;
    bool cli_threads = false;
    bool cli_numfd = false;
    bool cli_connections = false;
    bool cli_iface = false;
    bool cli_interval = false;
};

/* Parse argv. Returns 0 on success, 1 on error, 2 if help/version already handled. */
int parse_options(int argc, char **argv, Options &out);

void print_usage(FILE *out);

#endif /* PIDLOAD_OPTIONS_HPP */
