/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include "commons.h"
#include "config.h"

#include <bas/locale/i18n.h>
#include <bas/log/deflog.h>
#include <bas/proc/env.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

define_logger();

enum { OPT_VERSION = 256 };

static void usage(FILE *out) {
    fputs(_("Usage: pidload [OPTION]... [FILE]...\n"
            "Concatenate FILE(s) to standard output. With no FILE, or when FILE is -,\n"
            "read standard input.\n"),
          out);
    fputs("\n", out);
    fputs("  -v, --verbose      ", out);
    fputs(_("repeat for more verbose loggings\n"), out);
    fputs("  -q, --quiet        ", out);
    fputs(_("show less logging messages\n"), out);
    fputs("  -h, --help         ", out);
    fputs(_("display this help and exit\n"), out);
    fputs("      --version      ", out);
    fputs(_("output version information and exit\n"), out);
    fputs("\n", out);
    fprintf(out, _("Report bugs to: <%s>\n"), PROJECT_EMAIL);
}

int main(int argc, char **argv) {
    const char *exe = self_exe();
    init_i18n(LOCALEDIR);

    static const struct option long_opts[] = {
        {"verbose", no_argument, NULL, 'v'},
        {"quiet", no_argument, NULL, 'q'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, OPT_VERSION},
        {NULL, 0, NULL, 0},
    };

    for (;;) {
        int c = getopt_long(argc, argv, "vqh", long_opts, NULL);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'v':
            log_more();
            break;
        case 'q':
            log_less();
            break;
        case 'h':
            usage(stdout);
            return 0;
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
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }

    argc -= optind;
    argv += optind;

    loginfo_fmt("%s: verbose mode enabled", exe);

    if (argc == 0) {
        loginfo_fmt("%s: reading from standard input", exe);
        if (copy_stream(stdin, stdout) != 0) {
            fprintf(stderr, "%s: ", exe);
            perror("stdin");
            return 1;
        }
        loginfo_fmt("%s: done", exe);
        return 0;
    }

    for (int i = 0; i < argc; i++) {
        const char *path = argv[i];
        if (strcmp(path, "-") == 0) {
            loginfo_fmt("%s: copying from standard input", exe);
            if (copy_stream(stdin, stdout) != 0) {
                fprintf(stderr, "%s: ", exe);
                perror("stdin");
                return 1;
            }
        } else if (copy_file(exe, path) != 0) {
            return 1;
        } else {
            loginfo_fmt("%s: copied %s", exe, path);
        }
    }
    loginfo_fmt("%s: done", exe);

    return 0;
}
