/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "process_match.hpp"

#include <bas/log/uselog.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fnmatch.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <unistd.h>

bool name_has_glob(const std::string &name) {
    return name.find_first_of("*?") != std::string::npos;
}

bool is_pid_token(const std::string &s) {
    if (s.empty()) {
        return false;
    }
    for (unsigned char c : s) {
        if (!std::isdigit(c)) {
            return false;
        }
    }
    return true;
}

static std::string read_comm(pid_t pid) {
    std::ifstream in("/proc/" + std::to_string(pid) + "/comm");
    std::string s;
    std::getline(in, s);
    return s;
}

static std::string read_exe(pid_t pid) {
    char buf[4096];
    std::string link = "/proc/" + std::to_string(pid) + "/exe";
    ssize_t n = readlink(link.c_str(), buf, sizeof buf - 1);
    if (n < 0) {
        return {};
    }
    buf[n] = '\0';
    return buf;
}

static std::string base_name(const std::string &path) {
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

static std::string to_lower(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

/* Case-insensitive whole-word / exact title match for plain names. */
static bool title_matches_plain(const std::string &title, const std::string &name) {
    if (title.empty() || name.empty()) {
        return false;
    }
    if (title == name) {
        return true;
    }
    std::string t = to_lower(title);
    std::string n = to_lower(name);
    if (t == n) {
        return true;
    }
    size_t pos = 0;
    while ((pos = t.find(n, pos)) != std::string::npos) {
        bool left = (pos == 0) || !std::isalnum(static_cast<unsigned char>(t[pos - 1]));
        size_t end = pos + n.size();
        bool right = (end >= t.size()) || !std::isalnum(static_cast<unsigned char>(t[end]));
        if (left && right) {
            return true;
        }
        pos += 1;
    }
    return false;
}

static std::map<pid_t, std::string> load_window_titles() {
    std::map<pid_t, std::string> out;
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        return out;
    }

    Atom net_client = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom net_pid = XInternAtom(dpy, "_NET_WM_PID", False);
    Atom net_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    Window root = DefaultRootWindow(dpy);

    Atom type = None;
    int format = 0;
    unsigned long nitems = 0, bytes = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(dpy, root, net_client, 0, 1024, False, XA_WINDOW, &type, &format,
                           &nitems, &bytes, &data) != Success ||
        !data) {
        XCloseDisplay(dpy);
        return out;
    }

    auto *wins = reinterpret_cast<Window *>(data);
    for (unsigned long i = 0; i < nitems; ++i) {
        Window w = wins[i];

        Atom ptype = None;
        int pformat = 0;
        unsigned long pn = 0, pb = 0;
        unsigned char *pdata = nullptr;
        pid_t pid = 0;
        if (XGetWindowProperty(dpy, w, net_pid, 0, 1, False, XA_CARDINAL, &ptype, &pformat, &pn,
                               &pb, &pdata) == Success &&
            pdata && pn == 1) {
            pid = static_cast<pid_t>(*reinterpret_cast<unsigned long *>(pdata));
        }
        if (pdata) {
            XFree(pdata);
        }
        if (pid <= 0) {
            continue;
        }

        std::string title;
        unsigned char *tdata = nullptr;
        if (XGetWindowProperty(dpy, w, net_name, 0, 1024, False, utf8, &ptype, &pformat, &pn, &pb,
                               &tdata) == Success &&
            tdata && pn > 0) {
            title.assign(reinterpret_cast<char *>(tdata), pn);
        } else if (tdata) {
            XFree(tdata);
            tdata = nullptr;
        }
        if (!tdata) {
            XTextProperty prop;
            if (XGetWMName(dpy, w, &prop) && prop.value) {
                title.assign(reinterpret_cast<char *>(prop.value));
                if (prop.value) {
                    XFree(prop.value);
                }
            }
        } else {
            XFree(tdata);
        }

        if (!title.empty()) {
            /* Prefer first title; keep if empty previously. */
            if (out.find(pid) == out.end() || out[pid].empty()) {
                out[pid] = title;
            }
        }
    }
    XFree(data);
    XCloseDisplay(dpy);
    return out;
}

struct ProcInfo {
    pid_t pid = 0;
    std::string comm;
    std::string exe;
    std::string exe_base;
    std::string title;
};

static std::vector<ProcInfo> scan_processes(const std::map<pid_t, std::string> &titles) {
    std::vector<ProcInfo> list;
    DIR *dir = opendir("/proc");
    if (!dir) {
        return list;
    }
    while (dirent *ent = readdir(dir)) {
        if (!is_pid_token(ent->d_name)) {
            continue;
        }
        pid_t pid = static_cast<pid_t>(strtol(ent->d_name, nullptr, 10));
        ProcInfo p;
        p.pid = pid;
        p.comm = read_comm(pid);
        p.exe = read_exe(pid);
        p.exe_base = p.exe.empty() ? p.comm : base_name(p.exe);
        auto it = titles.find(pid);
        if (it != titles.end()) {
            p.title = it->second;
        }
        list.push_back(std::move(p));
    }
    closedir(dir);
    return list;
}

static MatchedProcess make_match(const ProcInfo &p, const std::string &name) {
    MatchedProcess m;
    m.pid = p.pid;
    m.name = name;
    m.exe = p.exe;
    m.label = !p.exe_base.empty() ? p.exe_base : (!p.comm.empty() ? p.comm : std::to_string(p.pid));
    return m;
}

std::vector<MatchedProcess> resolve_name(const std::string &name) {
    std::vector<MatchedProcess> matches;
    if (name.empty()) {
        return matches;
    }

    /* Fast path: numeric PID — skip X11. */
    if (is_pid_token(name)) {
        pid_t want = static_cast<pid_t>(strtol(name.c_str(), nullptr, 10));
        std::string path = "/proc/" + name;
        if (access(path.c_str(), F_OK) == 0) {
            ProcInfo p;
            p.pid = want;
            p.comm = read_comm(want);
            p.exe = read_exe(want);
            p.exe_base = p.exe.empty() ? p.comm : base_name(p.exe);
            matches.push_back(make_match(p, name));
        }
        return matches;
    }

    auto titles = load_window_titles();
    auto procs = scan_processes(titles);
    std::set<pid_t> seen;
    const bool glob = name_has_glob(name);

    auto add = [&](const ProcInfo &p) {
        if (seen.insert(p.pid).second) {
            matches.push_back(make_match(p, name));
        }
    };

    /* 2. executable name (comm or basename) */
    for (const auto &p : procs) {
        if ((!p.comm.empty() && p.comm == name) || (!p.exe_base.empty() && p.exe_base == name)) {
            add(p);
        }
    }

    /* 3. executable path */
    for (const auto &p : procs) {
        if (!p.exe.empty() && p.exe == name) {
            add(p);
        }
    }

    /* 4. window title (exact) */
    for (const auto &p : procs) {
        if (!p.title.empty() && p.title == name) {
            add(p);
        }
    }

    /* 5–7. globs, or plain-name word match on window titles */
    if (glob) {
        for (const auto &p : procs) {
            if ((!p.exe_base.empty() && fnmatch(name.c_str(), p.exe_base.c_str(), 0) == 0) ||
                (!p.comm.empty() && fnmatch(name.c_str(), p.comm.c_str(), 0) == 0)) {
                add(p);
            }
        }
        for (const auto &p : procs) {
            if (!p.exe.empty() && fnmatch(name.c_str(), p.exe.c_str(), FNM_PATHNAME) == 0) {
                add(p);
            }
        }
        for (const auto &p : procs) {
            if (!p.exe.empty() && fnmatch(name.c_str(), p.exe.c_str(), 0) == 0) {
                add(p);
            }
        }
        for (const auto &p : procs) {
            if (!p.title.empty() && fnmatch(name.c_str(), p.title.c_str(), 0) == 0) {
                add(p);
            }
        }
    } else {
        for (const auto &p : procs) {
            if (title_matches_plain(p.title, name)) {
                add(p);
            }
        }
    }

    return matches;
}

std::vector<TrackedProcess> resolve_names(const std::vector<std::string> &names) {
    std::vector<TrackedProcess> out;
    std::set<pid_t> seen;
    size_t style = 0;
    for (const auto &name : names) {
        auto matches = resolve_name(name);
        if (matches.empty()) {
            logwarn_fmt("no process matched NAME '%s'", name.c_str());
        }
        for (const auto &m : matches) {
            if (!seen.insert(m.pid).second) {
                continue;
            }
            TrackedProcess t;
            t.pid = m.pid;
            t.name = m.name;
            t.label = m.label;
            t.exe = m.exe;
            t.style_index = style++;
            out.push_back(std::move(t));
        }
    }
    return out;
}
