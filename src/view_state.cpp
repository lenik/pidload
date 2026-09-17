/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "view_state.hpp"
#include "process_match.hpp"
#include "util.hpp"

#include <bas/log/uselog.h>

#include <openssl/sha.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static std::string path_from_env_search(const std::string &name) {
    if (name.empty()) {
        return name;
    }
    if (name.find('/') != std::string::npos) {
        char *rp = realpath(name.c_str(), nullptr);
        if (rp) {
            std::string out(rp);
            free(rp);
            return out;
        }
        return name;
    }

    const char *path_env = getenv("PATH");
    if (!path_env || !*path_env) {
        return name;
    }
    std::string path = path_env;
    size_t start = 0;
    while (start <= path.size()) {
        size_t colon = path.find(':', start);
        std::string dir =
            colon == std::string::npos ? path.substr(start) : path.substr(start, colon - start);
        if (dir.empty()) {
            dir = ".";
        }
        std::string candidate = dir + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) {
            struct stat st {};
            if (stat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                return candidate;
            }
        }
        if (colon == std::string::npos) {
            break;
        }
        start = colon + 1;
    }
    return name;
}

std::string classify_name_token(const std::string &name) {
    if (is_pid_token(name)) {
        return "pid " + name;
    }
    if (name_has_glob(name)) {
        return "glob " + name;
    }
    return "path " + path_from_env_search(name);
}

std::string view_state_key_material(const std::vector<std::string> &names) {
    std::ostringstream ss;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            ss << ',';
        }
        ss << classify_name_token(names[i]);
    }
    return ss.str();
}

std::string utf8sha1_hex(const std::string &utf8) {
    unsigned char md[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(utf8.data()), utf8.size(), md);
    static const char *hex = "0123456789abcdef";
    std::string out;
    out.resize(SHA_DIGEST_LENGTH * 2);
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        out[static_cast<size_t>(i) * 2] = hex[(md[i] >> 4) & 0xf];
        out[static_cast<size_t>(i) * 2 + 1] = hex[md[i] & 0xf];
    }
    return out;
}

std::string view_state_config_dir() {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        return std::string(xdg) + "/pidload";
    }
    const char *home = getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.config/pidload";
    }
    return {};
}

std::string view_state_path_for_names(const std::vector<std::string> &names) {
    std::string dir = view_state_config_dir();
    if (dir.empty()) {
        return {};
    }
    std::string key = utf8sha1_hex(view_state_key_material(names));
    return dir + "/" + key + ".state";
}

static bool parse_bool(const std::string &v, bool *out) {
    if (v == "1" || v == "true" || v == "yes") {
        *out = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "no") {
        *out = false;
        return true;
    }
    return false;
}

static bool parse_i64(const std::string &v, int64_t *out) {
    char *end = nullptr;
    errno = 0;
    long long n = strtoll(v.c_str(), &end, 10);
    if (errno != 0 || end == v.c_str() || (end && *end)) {
        return false;
    }
    *out = static_cast<int64_t>(n);
    return true;
}

static bool parse_int(const std::string &v, int *out) {
    int64_t n = 0;
    if (!parse_i64(v, &n) || n < INT_MIN || n > INT_MAX) {
        return false;
    }
    *out = static_cast<int>(n);
    return true;
}

bool load_view_state(const std::string &path, ViewState &out) {
    if (path.empty()) {
        return false;
    }
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    ViewState st;
    std::string line;
    bool any = false;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        any = true;

        if (key == "show_cpu") {
            parse_bool(val, &st.show_cpu);
        } else if (key == "show_memory") {
            parse_bool(val, &st.show_memory);
        } else if (key == "monitor_network") {
            parse_bool(val, &st.monitor_network);
        } else if (key == "iface_all") {
            parse_bool(val, &st.iface_all);
        } else if (key == "ifaces") {
            st.ifaces.clear();
            std::stringstream ss(val);
            std::string part;
            while (std::getline(ss, part, ',')) {
                if (!part.empty()) {
                    st.ifaces.push_back(part);
                }
            }
        } else if (key == "show_threads") {
            parse_bool(val, &st.show_threads);
        } else if (key == "show_numfd") {
            parse_bool(val, &st.show_numfd);
        } else if (key == "show_connections") {
            parse_bool(val, &st.show_connections);
        } else if (key == "show_legends") {
            parse_bool(val, &st.show_legends);
        } else if (key == "y_log") {
            parse_bool(val, &st.y_log);
        } else if (key == "show_as") {
            int n = 0;
            if (parse_int(val, &n) && n >= 0 && n <= 3) {
                st.show_as = static_cast<ChartShowAs>(n);
            }
        } else if (key == "curve_style") {
            int n = 0;
            if (parse_int(val, &n) && n >= 0 && n <= 2) {
                st.curve_style = static_cast<CurveStyle>(n);
            }
        } else if (key == "net_unit") {
            int n = 0;
            if (parse_int(val, &n) && n >= 0 && n <= 2) {
                st.net_unit = static_cast<NetDisplayUnit>(n);
            }
        } else if (key == "interval_ms") {
            int64_t n = 0;
            if (parse_i64(val, &n) && n > 0) {
                st.interval_ms = n;
            }
        } else if (key == "frame_w") {
            parse_int(val, &st.frame_w);
        } else if (key == "frame_h") {
            parse_int(val, &st.frame_h);
        } else if (key == "frame_x") {
            if (parse_int(val, &st.frame_x)) {
                st.have_frame_pos = true;
            }
        } else if (key == "frame_y") {
            if (parse_int(val, &st.frame_y)) {
                st.have_frame_pos = true;
            }
        } else if (key == "aui_perspective") {
            st.aui_perspective = val;
            /* Unescape: \n -> newline, \\ -> \ */
            std::string decoded;
            for (size_t i = 0; i < st.aui_perspective.size(); ++i) {
                if (st.aui_perspective[i] == '\\' && i + 1 < st.aui_perspective.size()) {
                    char n = st.aui_perspective[++i];
                    if (n == 'n') {
                        decoded.push_back('\n');
                    } else if (n == '\\') {
                        decoded.push_back('\\');
                    } else {
                        decoded.push_back(n);
                    }
                } else {
                    decoded.push_back(st.aui_perspective[i]);
                }
            }
            st.aui_perspective = std::move(decoded);
        }
    }

    if (!any) {
        return false;
    }
    out = std::move(st);
    return true;
}

static std::string escape_perspective(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '\n') {
            out += "\\n";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

bool save_view_state(const std::string &path, const ViewState &st) {
    if (path.empty()) {
        return false;
    }
    auto slash = path.find_last_of('/');
    if (slash != std::string::npos) {
        if (!ensure_directory(path.substr(0, slash))) {
            logwarn_fmt("cannot create config dir for %s", path.c_str());
            return false;
        }
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        logwarn_fmt("cannot write view state %s", path.c_str());
        return false;
    }

    out << "# pidload view state\n";
    out << "version=1\n";
    out << "show_cpu=" << (st.show_cpu ? 1 : 0) << "\n";
    out << "show_memory=" << (st.show_memory ? 1 : 0) << "\n";
    out << "monitor_network=" << (st.monitor_network ? 1 : 0) << "\n";
    out << "iface_all=" << (st.iface_all ? 1 : 0) << "\n";
    out << "ifaces=";
    for (size_t i = 0; i < st.ifaces.size(); ++i) {
        if (i) {
            out << ',';
        }
        out << st.ifaces[i];
    }
    out << "\n";
    out << "show_threads=" << (st.show_threads ? 1 : 0) << "\n";
    out << "show_numfd=" << (st.show_numfd ? 1 : 0) << "\n";
    out << "show_connections=" << (st.show_connections ? 1 : 0) << "\n";
    out << "show_legends=" << (st.show_legends ? 1 : 0) << "\n";
    out << "y_log=" << (st.y_log ? 1 : 0) << "\n";
    out << "show_as=" << static_cast<int>(st.show_as) << "\n";
    out << "curve_style=" << static_cast<int>(st.curve_style) << "\n";
    out << "net_unit=" << static_cast<int>(st.net_unit) << "\n";
    out << "interval_ms=" << st.interval_ms << "\n";
    if (st.frame_w > 0 && st.frame_h > 0) {
        out << "frame_w=" << st.frame_w << "\n";
        out << "frame_h=" << st.frame_h << "\n";
    }
    if (st.have_frame_pos) {
        out << "frame_x=" << st.frame_x << "\n";
        out << "frame_y=" << st.frame_y << "\n";
    }
    out << "aui_perspective=" << escape_perspective(st.aui_perspective) << "\n";
    return static_cast<bool>(out);
}

void apply_view_state(Options &opt, const ViewState &st) {
    if (!opt.cli_cpu) {
        opt.show_cpu = st.show_cpu;
    }
    if (!opt.cli_memory) {
        opt.show_memory = st.show_memory;
    }
    if (!opt.cli_threads) {
        opt.show_threads = st.show_threads;
    }
    if (!opt.cli_numfd) {
        opt.show_numfd = st.show_numfd;
    }
    if (!opt.cli_connections) {
        opt.show_connections = st.show_connections;
    }
    if (!opt.cli_iface) {
        opt.monitor_network = st.monitor_network;
        opt.iface_all = st.iface_all;
        opt.ifaces = st.ifaces;
        opt.iface_explicit = st.monitor_network && !st.iface_all && !st.ifaces.empty();
    }
    if (!opt.cli_interval && st.interval_ms > 0 && st.interval_ms <= opt.window_ms) {
        opt.interval_ms = st.interval_ms;
    }

    opt.show_legends = st.show_legends;
    opt.y_log = st.y_log;
    opt.show_as = st.show_as;
    opt.curve_style = st.curve_style;
    opt.net_unit = st.net_unit;
}

void capture_view_state_from_options(const Options &opt, ViewState &st) {
    st.show_cpu = opt.show_cpu;
    st.show_memory = opt.show_memory;
    st.monitor_network = opt.monitor_network;
    st.iface_all = opt.iface_all;
    st.ifaces = opt.ifaces;
    st.show_threads = opt.show_threads;
    st.show_numfd = opt.show_numfd;
    st.show_connections = opt.show_connections;
    st.show_legends = opt.show_legends;
    st.y_log = opt.y_log;
    st.show_as = opt.show_as;
    st.curve_style = opt.curve_style;
    st.net_unit = opt.net_unit;
    st.interval_ms = opt.interval_ms;
}
