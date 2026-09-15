/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include "commons.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect_eq_int(const char *name, int got, int want) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
        failures++;
    }
}

static void expect_eq_str(const char *name, const char *got, const char *want) {
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL %s: got %s want %s\n", name, got, want);
        failures++;
    }
}

int main(void) {
    FILE *in = tmpfile();
    FILE *out = tmpfile();
    char buf[64] = {0};
    const char *text = "alpha\nbeta\n";

    if (!in || !out) {
        fputs("FAIL tmpfile\n", stderr);
        return 1;
    }

    fwrite(text, 1, strlen(text), in);
    rewind(in);
    expect_eq_int("copy_stream", copy_stream(in, out), 0);
    rewind(out);
    size_t n = fread(buf, 1, sizeof(buf) - 1, out);
    buf[n] = '\0';
    expect_eq_str("copy_stream data", buf, text);
    expect_eq_int("copy_file missing", copy_file("test", "/definitely/not/found"), -1);

    fclose(in);
    fclose(out);
    return failures == 0 ? 0 : 1;
}
