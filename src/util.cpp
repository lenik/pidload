/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "util.hpp"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

int64_t parse_duration_ms(const char *text) {
    if (!text || !*text) {
        return -1;
    }

    char *end = nullptr;
    errno = 0;
    double value = strtod(text, &end);
    if (errno != 0 || end == text || !std::isfinite(value) || value < 0) {
        return -1;
    }

    while (*end && std::isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }

    double factor = 1000.0; /* default: seconds */
    if (*end == '\0') {
        /* seconds */
    } else if (strcmp(end, "ms") == 0 || strcmp(end, "msec") == 0) {
        factor = 1.0;
    } else if (strcmp(end, "s") == 0 || strcmp(end, "sec") == 0 || strcmp(end, "secs") == 0) {
        factor = 1000.0;
    } else if (strcmp(end, "m") == 0 || strcmp(end, "min") == 0 || strcmp(end, "mins") == 0) {
        factor = 60.0 * 1000.0;
    } else if (strcmp(end, "h") == 0 || strcmp(end, "hr") == 0 || strcmp(end, "hour") == 0 ||
               strcmp(end, "hours") == 0) {
        factor = 3600.0 * 1000.0;
    } else {
        return -1;
    }

    double ms = value * factor;
    if (ms > static_cast<double>(INT64_MAX)) {
        return -1;
    }
    return static_cast<int64_t>(ms + 0.5);
}

std::string device_basename(const std::string &path) {
    if (path.compare(0, 5, "/dev/") == 0) {
        return path.substr(5);
    }
    return path;
}

std::string safe_filename(const std::string &name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c) || c == '.' || c == '_' || c == '-') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        out = "unknown";
    }
    return out;
}

bool ensure_directory(const std::string &path) {
    if (path.empty()) {
        return false;
    }
    struct stat st {};
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    if (errno == EEXIST) {
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    return false;
}

std::vector<std::string> list_net_ifaces() {
    std::vector<std::string> names;
    DIR *dir = opendir("/sys/class/net");
    if (!dir) {
        return names;
    }
    while (dirent *ent = readdir(dir)) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        names.emplace_back(ent->d_name);
    }
    closedir(dir);
    return names;
}
