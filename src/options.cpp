/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "options.hpp"
#include "config.h"
#include "util.hpp"

#include <bas/locale/i18n.h>
#include <bas/log/uselog.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>

enum { OPT_VERSION = 256 };

void print_usage(FILE *out) {
    fputs(_("Usage: pidload [OPTION]... [NAME]...\n"
            "Monitor process load and selected device/network traffic in a wxWidgets window.\n"),
          out);
    fputs("\n", out);
    fputs(_("Each NAME matches processes, tried as:\n"
            "  pid, executable name, executable path, window title,\n"
            "  executable glob name, executable glob path, window glob title.\n"
            "Glob characters: * ?   Example: pidload editor   pidload '*term*'\n"),
          out);
    fputs("\n", out);
    fputs("  -d, --dev DEVICE     ", out);
    fputs(_("block device I/O to monitor (repeatable)\n"), out);
    fputs("  -i, --iface NETDEV   ", out);
    fputs(_("netdev to monitor, or \"all\" (repeatable)\n"), out);
    fputs("  -a, --addr NETADDR   ", out);
    fputs(_("IPv4/IPv6 address traffic to monitor (repeatable)\n"), out);
    fputs("  -c, --cpu            ", out);
    fputs(_("chart system CPU (overall and each core)\n"), out);
    fputs("  -m, --memory         ", out);
    fputs(_("chart system memory and swap\n"), out);
    fputs("  -T, --threads        ", out);
    fputs(_("chart thread counts (alive / wait / total opened)\n"), out);
    fputs("  -F, --numfd          ", out);
    fputs(_("chart open file-descriptor counts\n"), out);
    fputs("  -C, --connections    ", out);
    fputs(_("chart net connections (alive / wait / total)\n"), out);
    fputs("  -t, --interval D     ", out);
    fputs(_("sample interval (default: 2s)\n"), out);
    fputs("  -w, --window D       ", out);
    fputs(_("default visible window; session history stays in memory (default: 3min)\n"), out);
    fputs("  -o, --output PATH    ", out);
    fputs(_("directory prefix for interval logs (name.pid.log)\n"), out);
    fputs("  -v, --verbose        ", out);
    fputs(_("repeat for more verbose logging\n"), out);
    fputs("  -q, --quiet          ", out);
    fputs(_("show fewer logging messages\n"), out);
    fputs("  -h, --help           ", out);
    fputs(_("display this help and exit\n"), out);
    fputs("      --version        ", out);
    fputs(_("output version information and exit\n"), out);
    fputs("\n", out);
    fputs(_("Duration units: ms, s/sec (default), m/min, h. Examples: .5s, 5min.\n"), out);
    fputs("\n", out);
    fprintf(out, _("Report bugs to: <%s>\n"), PROJECT_EMAIL);
}

int parse_options(int argc, char **argv, Options &opt) {
    static const struct option long_opts[] = {
        {"dev", required_argument, nullptr, 'd'},
        {"iface", required_argument, nullptr, 'i'},
        {"addr", required_argument, nullptr, 'a'},
        {"cpu", no_argument, nullptr, 'c'},
        {"memory", no_argument, nullptr, 'm'},
        {"threads", no_argument, nullptr, 'T'},
        {"numfd", no_argument, nullptr, 'F'},
        {"connections", no_argument, nullptr, 'C'},
        {"interval", required_argument, nullptr, 't'},
        {"window", required_argument, nullptr, 'w'},
        {"output", required_argument, nullptr, 'o'},
        {"verbose", no_argument, nullptr, 'v'},
        {"quiet", no_argument, nullptr, 'q'},
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, OPT_VERSION},
        {nullptr, 0, nullptr, 0},
    };

    bool iface_seen = false;

    for (;;) {
        int c = getopt_long(argc, argv, "d:i:a:cmTFCt:w:o:vqh", long_opts, nullptr);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'd':
            opt.devices.push_back(device_basename(optarg));
            break;
        case 'i':
            if (!iface_seen) {
                opt.ifaces.clear();
                opt.iface_all = false;
                iface_seen = true;
            }
            opt.cli_iface = true;
            opt.iface_explicit = true;
            opt.monitor_network = true;
            if (strcmp(optarg, "all") == 0) {
                opt.iface_all = true;
                opt.ifaces.clear();
            } else {
                opt.iface_all = false;
                opt.ifaces.emplace_back(optarg);
            }
            break;
        case 'a':
            opt.addrs.emplace_back(optarg);
            break;
        case 'c':
            opt.cli_cpu = true;
            opt.show_cpu = true;
            break;
        case 'm':
            opt.cli_memory = true;
            opt.show_memory = true;
            break;
        case 'T':
            opt.cli_threads = true;
            opt.show_threads = true;
            break;
        case 'F':
            opt.cli_numfd = true;
            opt.show_numfd = true;
            break;
        case 'C':
            opt.cli_connections = true;
            opt.show_connections = true;
            break;
        case 't': {
            int64_t ms = parse_duration_ms(optarg);
            if (ms <= 0) {
                fprintf(stderr, _("pidload: invalid interval: %s\n"), optarg);
                return 1;
            }
            opt.cli_interval = true;
            opt.interval_ms = ms;
            break;
        }
        case 'w': {
            int64_t ms = parse_duration_ms(optarg);
            if (ms <= 0) {
                fprintf(stderr, _("pidload: invalid window: %s\n"), optarg);
                return 1;
            }
            opt.window_ms = ms;
            break;
        }
        case 'o':
            opt.output_dir = optarg;
            break;
        case 'v':
            log_more();
            break;
        case 'q':
            log_less();
            break;
        case 'h':
            print_usage(stdout);
            return 2;
        case OPT_VERSION:
            printf("pidload %s\n", PROJECT_VERSION);
            printf(_("Copyright (C) %d %s\n"), PROJECT_YEAR, PROJECT_AUTHOR);
            fputs(_("License AGPL-3.0-or-later: <https://www.gnu.org/licenses/agpl-3.0.html>\n"),
                  stdout);
            fputs(_("This is free software: you are free to change and redistribute it.\n"),
                  stdout);
            fputs(_("This project opposes AI exploitation and AI hegemony.\n"), stdout);
            fputs(_("This project rejects mindless MIT-style licensing and politically naive "
                    "BSD-style licensing.\n"),
                  stdout);
            fputs(_("There is NO WARRANTY, to the extent permitted by law.\n"), stdout);
            return 2;
        default:
            print_usage(stderr);
            return 1;
        }
    }

    if (!iface_seen) {
        if (opt.names.empty() && opt.devices.empty() && opt.addrs.empty() && !opt.show_cpu &&
            !opt.show_memory && !opt.show_threads && !opt.show_numfd && !opt.show_connections) {
            /* filled after names parsed — handle below */
        }
    }

    for (int i = optind; i < argc; ++i) {
        opt.names.emplace_back(argv[i]);
    }

    if (!iface_seen) {
        if (opt.names.empty() && opt.devices.empty() && opt.addrs.empty() && !opt.show_cpu &&
            !opt.show_memory && !opt.show_threads && !opt.show_numfd && !opt.show_connections) {
            opt.monitor_network = true;
            opt.iface_all = true;
        }
    }

    if (opt.window_ms < opt.interval_ms) {
        fprintf(stderr, _("pidload: window must be >= interval\n"));
        return 1;
    }

    if (opt.names.empty() && opt.devices.empty() && opt.addrs.empty() && !opt.monitor_network &&
        !opt.show_cpu && !opt.show_memory && !opt.show_threads && !opt.show_numfd &&
        !opt.show_connections) {
        fprintf(stderr,
                _("pidload: nothing to monitor; give NAME(s) and/or -d/-i/-a/-c/-m/-T/-F/-C\n"));
        return 1;
    }

    return 0;
}
