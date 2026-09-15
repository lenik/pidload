/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "collector.hpp"
#include "process_match.hpp"
#include "util.hpp"

#include <bas/locale/i18n.h>
#include <bas/log/uselog.h>

#include <arpa/inet.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <ifaddrs.h>
#include <iterator>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

static uint64_t delta_u64(uint64_t now, uint64_t prev) {
    return now >= prev ? now - prev : 0;
}

const char *series_kind_name(SeriesKind k) {
    switch (k) {
    case SeriesKind::DevRead:
        return _("read");
    case SeriesKind::DevWrite:
        return _("write");
    case SeriesKind::IfaceIn:
        return _("in");
    case SeriesKind::IfaceOut:
        return _("out");
    case SeriesKind::IfaceDrop:
        return _("drop");
    case SeriesKind::AddrIn:
        return _("in");
    case SeriesKind::AddrOut:
        return _("out");
    case SeriesKind::PidCpu:
        return _("cpu");
    case SeriesKind::PidRead:
        return _("read");
    case SeriesKind::PidWrite:
        return _("write");
    case SeriesKind::CpuOverall:
        return _("overall");
    case SeriesKind::CpuCore:
        return _("core");
    case SeriesKind::MemUsed:
        return _("used");
    case SeriesKind::MemAvail:
        return _("avail");
    case SeriesKind::MemSwap:
        return _("swap");
    case SeriesKind::ThreadsAlive:
        return _("alive");
    case SeriesKind::ThreadsWait:
        return _("wait");
    case SeriesKind::ThreadsTotal:
        return _("total");
    case SeriesKind::FdOpen:
        return _("open");
    case SeriesKind::ConnAlive:
        return _("alive");
    case SeriesKind::ConnWait:
        return _("wait");
    case SeriesKind::ConnTotal:
        return _("total");
    }
    return "?";
}

const char *chart_type_title(ChartType t) {
    switch (t) {
    case ChartType::Device:
        return _("Device I/O");
    case ChartType::Network:
        return _("Network");
    case ChartType::Address:
        return _("Address");
    case ChartType::ProcessCpu:
        return _("Process CPU");
    case ChartType::ProcessIo:
        return _("Process I/O");
    case ChartType::SystemCpu:
        return _("CPU");
    case ChartType::SystemMemory:
        return _("Memory");
    case ChartType::Threads:
        return _("Threads");
    case ChartType::NumFd:
        return _("File Descriptors");
    case ChartType::Connections:
        return _("Connections");
    }
    return _("Chart");
}

Collector::Collector(const Options &opt) : opt_(opt) {
    max_points_ = static_cast<size_t>(opt_.window_ms / opt_.interval_ms);
    if (max_points_ < 2) {
        max_points_ = 2;
    }
    /* Keep a long session history in memory; capture window is the default view. */
    session_max_points_ = std::max<size_t>(max_points_ * 200, 10000);
    if (session_max_points_ > 200000) {
        session_max_points_ = 200000;
    }
    core_count_ = detect_cores();
    refresh_addr_map();
    resolve_ifaces();
    sync_tracked();
    rebuild_charts();
}

int Collector::detect_cores() {
    std::ifstream in("/proc/stat");
    std::string line;
    int cores = 0;
    while (std::getline(in, line)) {
        if (line.compare(0, 3, "cpu") == 0 && line.size() > 3 && std::isdigit(line[3])) {
            ++cores;
        }
    }
    return cores > 0 ? cores : 1;
}

void Collector::refresh_addr_map() {
    addr_to_iface_.clear();
    ifaddrs *ifa_list = nullptr;
    if (getifaddrs(&ifa_list) != 0) {
        return;
    }
    for (ifaddrs *ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || !ifa->ifa_name) {
            continue;
        }
        char buf[INET6_ADDRSTRLEN];
        if (ifa->ifa_addr->sa_family == AF_INET) {
            auto *sin = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
            if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof buf)) {
                addr_to_iface_[buf] = ifa->ifa_name;
            }
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            auto *sin6 = reinterpret_cast<sockaddr_in6 *>(ifa->ifa_addr);
            if (inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof buf)) {
                addr_to_iface_[buf] = ifa->ifa_name;
            }
        }
    }
    freeifaddrs(ifa_list);
}

void Collector::resolve_ifaces() {
    ifaces_.clear();
    if (!opt_.monitor_network) {
        return;
    }
    if (opt_.iface_all) {
        ifaces_ = list_net_ifaces();
    } else {
        ifaces_ = opt_.ifaces;
    }
}

void Collector::rebuild_charts() {
    charts_.clear();
    resolve_ifaces();

    auto add_chart = [&](ChartType t, const std::string &id) -> ChartSpec & {
        charts_.push_back(ChartSpec{t, chart_type_title(t), id, {}});
        return charts_.back();
    };

    if (opt_.show_cpu) {
        auto &ch = add_chart(ChartType::SystemCpu, "sys-cpu");
        SeriesKey ok{SeriesKind::CpuOverall, "all"};
        ensure_series(ok, "overall");
        ch.series.push_back(ok);
        for (int i = 0; i < core_count_; ++i) {
            std::string name = std::to_string(i);
            SeriesKey ck{SeriesKind::CpuCore, name};
            ensure_series(ck, "cpu" + name);
            ch.series.push_back(ck);
        }
    }

    if (opt_.show_memory) {
        auto &ch = add_chart(ChartType::SystemMemory, "sys-mem");
        SeriesKey u{SeriesKind::MemUsed, "used"};
        SeriesKey a{SeriesKind::MemAvail, "avail"};
        SeriesKey s{SeriesKind::MemSwap, "swap"};
        ensure_series(u, "used");
        ensure_series(a, "available");
        ensure_series(s, "swap");
        ch.series.push_back(u);
        ch.series.push_back(a);
        ch.series.push_back(s);
    }

    if (!opt_.devices.empty()) {
        auto &ch = add_chart(ChartType::Device, "device");
        for (const auto &d : opt_.devices) {
            SeriesKey rk{SeriesKind::DevRead, d};
            SeriesKey wk{SeriesKind::DevWrite, d};
            ensure_series(rk, d + " read");
            ensure_series(wk, d + " write");
            ch.series.push_back(rk);
            ch.series.push_back(wk);
        }
    }

    if (opt_.monitor_network && (opt_.iface_all || !opt_.ifaces.empty())) {
        auto &ch = add_chart(ChartType::Network, "network");
        for (const auto &iface : ifaces_) {
            SeriesKey ik{SeriesKind::IfaceIn, iface};
            SeriesKey ok{SeriesKind::IfaceOut, iface};
            ensure_series(ik, iface + " in");
            ensure_series(ok, iface + " out");
            ch.series.push_back(ik);
            ch.series.push_back(ok);
        }
    }

    if (!opt_.addrs.empty()) {
        auto &ch = add_chart(ChartType::Address, "address");
        for (const auto &addr : opt_.addrs) {
            SeriesKey ik{SeriesKind::AddrIn, addr};
            SeriesKey ok{SeriesKind::AddrOut, addr};
            ensure_series(ik, addr + " in");
            ensure_series(ok, addr + " out");
            ch.series.push_back(ik);
            ch.series.push_back(ok);
        }
    }

    if (!tracked_.empty()) {
        charts_.push_back(ChartSpec{ChartType::ProcessCpu, chart_type_title(ChartType::ProcessCpu),
                                    "proc-cpu", {}});
        charts_.push_back(
            ChartSpec{ChartType::ProcessIo, chart_type_title(ChartType::ProcessIo), "proc-io", {}});
        ChartSpec &cpu = charts_[charts_.size() - 2];
        ChartSpec &pio = charts_[charts_.size() - 1];
        for (const auto &tp : tracked_) {
            std::string id = std::to_string(tp.pid);
            std::string lab = tp.label + " (" + id + ")";
            SeriesKey ck{SeriesKind::PidCpu, id};
            SeriesKey rk{SeriesKind::PidRead, id};
            SeriesKey wk{SeriesKind::PidWrite, id};
            ensure_series(ck, lab, tp.style_index, tp.name, tp.pid);
            ensure_series(rk, lab + " read", tp.style_index, tp.name, tp.pid);
            ensure_series(wk, lab + " write", tp.style_index, tp.name, tp.pid);
            cpu.series.push_back(ck);
            pio.series.push_back(rk);
            pio.series.push_back(wk);
        }
    }

    auto add_proc_metric_chart = [&](ChartType type, const char *id, SeriesKind a, SeriesKind b,
                                     SeriesKind c, const char *la, const char *lb, const char *lc) {
        if (tracked_.empty()) {
            return;
        }
        auto &ch = add_chart(type, id);
        for (const auto &tp : tracked_) {
            std::string pid = std::to_string(tp.pid);
            std::string base = tp.label + " (" + pid + ")";
            SeriesKey ka{a, pid};
            SeriesKey kb{b, pid};
            SeriesKey kc{c, pid};
            ensure_series(ka, base + " " + la, tp.style_index, tp.name, tp.pid);
            ensure_series(kb, base + " " + lb, tp.style_index, tp.name, tp.pid);
            ensure_series(kc, base + " " + lc, tp.style_index, tp.name, tp.pid);
            ch.series.push_back(ka);
            ch.series.push_back(kb);
            ch.series.push_back(kc);
        }
    };

    if (opt_.show_threads) {
        if (!tracked_.empty()) {
            add_proc_metric_chart(ChartType::Threads, "threads", SeriesKind::ThreadsAlive,
                                  SeriesKind::ThreadsWait, SeriesKind::ThreadsTotal, "alive", "wait",
                                  "total");
        } else {
            auto &ch = add_chart(ChartType::Threads, "threads");
            SeriesKey ka{SeriesKind::ThreadsAlive, "system"};
            SeriesKey kb{SeriesKind::ThreadsWait, "system"};
            SeriesKey kc{SeriesKind::ThreadsTotal, "system"};
            ensure_series(ka, "system alive");
            ensure_series(kb, "system wait");
            ensure_series(kc, "system total");
            ch.series.push_back(ka);
            ch.series.push_back(kb);
            ch.series.push_back(kc);
        }
    }
    if (opt_.show_numfd) {
        if (!tracked_.empty()) {
            auto &ch = add_chart(ChartType::NumFd, "numfd");
            for (const auto &tp : tracked_) {
                std::string pid = std::to_string(tp.pid);
                SeriesKey k{SeriesKind::FdOpen, pid};
                ensure_series(k, tp.label + " (" + pid + ")", tp.style_index, tp.name, tp.pid);
                ch.series.push_back(k);
            }
        }
    }
    if (opt_.show_connections) {
        if (!tracked_.empty()) {
            add_proc_metric_chart(ChartType::Connections, "connections", SeriesKind::ConnAlive,
                                  SeriesKind::ConnWait, SeriesKind::ConnTotal, "alive", "wait",
                                  "total");
        } else {
            /* System-wide when no process NAMEs. */
            auto &ch = add_chart(ChartType::Connections, "connections");
            SeriesKey ka{SeriesKind::ConnAlive, "system"};
            SeriesKey kb{SeriesKind::ConnWait, "system"};
            SeriesKey kc{SeriesKind::ConnTotal, "system"};
            ensure_series(ka, "system alive");
            ensure_series(kb, "system wait");
            ensure_series(kc, "system total");
            ch.series.push_back(ka);
            ch.series.push_back(kb);
            ch.series.push_back(kc);
        }
    }
}

bool Collector::sync_tracked() {
    auto next = resolve_names(opt_.names);
    bool same = next.size() == tracked_.size();
    if (same) {
        for (size_t i = 0; i < next.size(); ++i) {
            if (next[i].pid != tracked_[i].pid || next[i].name != tracked_[i].name ||
                next[i].style_index != tracked_[i].style_index) {
                same = false;
                break;
            }
        }
    }
    if (same) {
        return false;
    }
    tracked_ = std::move(next);
    return true;
}

void Collector::ensure_series(const SeriesKey &key, const std::string &label, size_t style_index,
                              const std::string &match_name, pid_t pid) {
    auto it = series_.find(key);
    if (it != series_.end()) {
        it->second.label = label;
        it->second.style_index = style_index;
        it->second.match_name = match_name;
        it->second.pid = pid;
        return;
    }
    Series s;
    s.key = key;
    s.label = label;
    s.style_index = style_index;
    s.match_name = match_name;
    s.pid = pid;
    series_.emplace(key, std::move(s));
}

void Collector::push_point(const SeriesKey &key, int64_t t_ms, double value, double aux) {
    auto it = series_.find(key);
    if (it == series_.end()) {
        return;
    }
    auto &pts = it->second.points;
    pts.push_back(SamplePoint{t_ms, value, aux});
    while (pts.size() > session_max_points_) {
        pts.erase(pts.begin());
    }
}

int64_t Collector::data_t_min() const {
    int64_t t = -1;
    for (const auto &kv : series_) {
        if (kv.second.points.empty()) {
            continue;
        }
        int64_t v = kv.second.points.front().t_ms;
        t = (t < 0) ? v : std::min(t, v);
    }
    return t < 0 ? 0 : t;
}

int64_t Collector::data_t_max() const {
    int64_t t = 0;
    for (const auto &kv : series_) {
        if (kv.second.points.empty()) {
            continue;
        }
        t = std::max(t, kv.second.points.back().t_ms);
    }
    return t;
}

void Collector::clear_history() {
    for (auto &kv : series_) {
        kv.second.points.clear();
    }
    seen_tids_.clear();
    seen_conn_inodes_.clear();
    have_prev_ = false;
    have_deltas_ = false;
}

void Collector::set_window_ms(int64_t window_ms) {
    opt_.window_ms = window_ms;
    max_points_ = static_cast<size_t>(opt_.window_ms / opt_.interval_ms);
    if (max_points_ < 2) {
        max_points_ = 2;
    }
}

void Collector::set_interval_ms(int64_t interval_ms) {
    opt_.interval_ms = interval_ms;
    max_points_ = static_cast<size_t>(opt_.window_ms / opt_.interval_ms);
    if (max_points_ < 2) {
        max_points_ = 2;
    }
}

bool Collector::set_show_cpu(bool on) {
    if (opt_.show_cpu == on) {
        return false;
    }
    opt_.show_cpu = on;
    rebuild_charts();
    return true;
}

bool Collector::set_show_memory(bool on) {
    if (opt_.show_memory == on) {
        return false;
    }
    opt_.show_memory = on;
    rebuild_charts();
    return true;
}

bool Collector::set_show_threads(bool on) {
    if (opt_.show_threads == on) {
        return false;
    }
    opt_.show_threads = on;
    rebuild_charts();
    return true;
}

bool Collector::set_show_numfd(bool on) {
    if (opt_.show_numfd == on) {
        return false;
    }
    opt_.show_numfd = on;
    rebuild_charts();
    return true;
}

bool Collector::set_show_connections(bool on) {
    if (opt_.show_connections == on) {
        return false;
    }
    opt_.show_connections = on;
    rebuild_charts();
    return true;
}

bool Collector::set_monitor_network(bool on, bool all, const std::vector<std::string> &ifaces) {
    opt_.monitor_network = on;
    opt_.iface_all = all;
    opt_.ifaces = ifaces;
    rebuild_charts();
    return true;
}

bool Collector::add_device(const std::string &dev) {
    std::string name = device_basename(dev);
    for (const auto &d : opt_.devices) {
        if (d == name) {
            return false;
        }
    }
    opt_.devices.push_back(name);
    rebuild_charts();
    return true;
}

bool Collector::add_iface(const std::string &iface) {
    opt_.monitor_network = true;
    if (iface == "all") {
        opt_.iface_all = true;
        opt_.ifaces.clear();
    } else {
        opt_.iface_all = false;
        for (const auto &i : opt_.ifaces) {
            if (i == iface) {
                rebuild_charts();
                return false;
            }
        }
        opt_.ifaces.push_back(iface);
    }
    rebuild_charts();
    return true;
}

bool Collector::add_addr(const std::string &addr) {
    for (const auto &a : opt_.addrs) {
        if (a == addr) {
            return false;
        }
    }
    opt_.addrs.push_back(addr);
    refresh_addr_map();
    rebuild_charts();
    return true;
}

bool Collector::add_name(const std::string &name) {
    if (name.empty()) {
        return false;
    }
    for (const auto &n : opt_.names) {
        if (n == name) {
            sync_tracked();
            rebuild_charts();
            return false;
        }
    }
    opt_.names.push_back(name);
    sync_tracked();
    rebuild_charts();
    return true;
}

bool Collector::add_pid(pid_t pid) {
    return add_name(std::to_string(pid));
}

bool Collector::remove_chart(const std::string &chart_id) {
    bool changed = false;
    if (chart_id == "sys-cpu") {
        changed = set_show_cpu(false);
    } else if (chart_id == "sys-mem") {
        changed = set_show_memory(false);
    } else if (chart_id == "network") {
        opt_.monitor_network = false;
        opt_.ifaces.clear();
        opt_.iface_all = false;
        rebuild_charts();
        changed = true;
    } else if (chart_id == "device") {
        opt_.devices.clear();
        rebuild_charts();
        changed = true;
    } else if (chart_id == "address") {
        opt_.addrs.clear();
        rebuild_charts();
        changed = true;
    } else if (chart_id == "proc-cpu" || chart_id == "proc-io") {
        opt_.names.clear();
        tracked_.clear();
        rebuild_charts();
        changed = true;
    } else if (chart_id == "threads") {
        changed = set_show_threads(false);
    } else if (chart_id == "numfd") {
        changed = set_show_numfd(false);
    } else if (chart_id == "connections") {
        changed = set_show_connections(false);
    }
    return changed;
}

bool Collector::tick(int64_t now_ms) {
    bool layout_changed = false;
    /* Re-scan NAMEs periodically so newly started processes appear, but not
     * every sample (X11 window enumeration is relatively expensive). */
    static thread_local int64_t last_resolve_ms = -100000;
    if (!opt_.names.empty() && (now_ms - last_resolve_ms >= 2000 || last_resolve_ms < 0)) {
        last_resolve_ms = now_ms;
        if (sync_tracked()) {
            rebuild_charts();
            layout_changed = true;
        }
    }

    last_devs_.clear();
    last_ifaces_.clear();
    last_addrs_.clear();
    last_pids_.clear();
    last_cpu_ = CpuDelta{};
    last_mem_ = MemDelta{};
    last_threads_.clear();
    last_fds_.clear();
    last_conns_.clear();

    bool emit = have_prev_;
    sample_cpu(now_ms, emit);
    sample_memory(now_ms, emit);
    sample_devices(now_ms, emit);
    sample_ifaces(now_ms, emit);
    sample_addrs(now_ms, emit);
    sample_pids(now_ms, emit);
    sample_threads(now_ms, emit);
    sample_numfd(now_ms, emit);
    sample_connections(now_ms, emit);

    if (emit) {
        have_deltas_ = true;
    }
    have_prev_ = true;
    return layout_changed;
}

static bool read_diskstats(std::unordered_map<std::string, CounterSnap> &out) {
    std::ifstream in("/proc/diskstats");
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        unsigned major = 0, minor = 0;
        std::string name;
        uint64_t rd_completed = 0, rd_merged = 0, rd_sectors = 0, rd_ms = 0;
        uint64_t wr_completed = 0, wr_merged = 0, wr_sectors = 0, wr_ms = 0;
        if (!(ss >> major >> minor >> name >> rd_completed >> rd_merged >> rd_sectors >> rd_ms >>
              wr_completed >> wr_merged >> wr_sectors >> wr_ms)) {
            continue;
        }
        CounterSnap snap;
        snap.a = rd_sectors * 512ULL;
        snap.b = wr_sectors * 512ULL;
        snap.valid = true;
        out[name] = snap;
    }
    return true;
}

void Collector::sample_devices(int64_t now_ms, bool emit) {
    if (opt_.devices.empty()) {
        return;
    }
    std::unordered_map<std::string, CounterSnap> cur;
    read_diskstats(cur);

    for (const auto &name : opt_.devices) {
        auto it = cur.find(name);
        if (it == cur.end()) {
            continue;
        }
        const CounterSnap &now = it->second;
        DevDelta d;
        d.name = name;
        if (emit) {
            auto pit = prev_dev_.find(name);
            if (pit != prev_dev_.end() && pit->second.valid) {
                d.read_bytes = delta_u64(now.a, pit->second.a);
                d.write_bytes = delta_u64(now.b, pit->second.b);
                push_point({SeriesKind::DevRead, name}, now_ms, static_cast<double>(d.read_bytes));
                push_point({SeriesKind::DevWrite, name}, now_ms, static_cast<double>(d.write_bytes));
            }
        }
        last_devs_.push_back(d);
        prev_dev_[name] = now;
    }
}

static bool read_netdev(std::unordered_map<std::string, CounterSnap> &out) {
    std::ifstream in("/proc/net/dev");
    if (!in) {
        return false;
    }
    std::string line;
    std::getline(in, line);
    std::getline(in, line);
    while (std::getline(in, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string name = line.substr(0, colon);
        size_t start = name.find_first_not_of(" \t");
        size_t end = name.find_last_not_of(" \t");
        if (start == std::string::npos) {
            continue;
        }
        name = name.substr(start, end - start + 1);

        std::istringstream ss(line.substr(colon + 1));
        uint64_t rbytes = 0, rpackets = 0, rerrs = 0, rdrop = 0;
        uint64_t rfifo = 0, rframe = 0, rcomp = 0, rmulti = 0;
        uint64_t tbytes = 0, tpackets = 0, terrs = 0, tdrop = 0;
        if (!(ss >> rbytes >> rpackets >> rerrs >> rdrop >> rfifo >> rframe >> rcomp >> rmulti >>
              tbytes >> tpackets >> terrs >> tdrop)) {
            continue;
        }
        CounterSnap snap;
        snap.a = rbytes;
        snap.b = tbytes;
        snap.c = rdrop + tdrop;
        snap.d = rpackets;
        snap.e = tpackets;
        snap.valid = true;
        out[name] = snap;
    }
    return true;
}

void Collector::sample_ifaces(int64_t now_ms, bool emit) {
    if (!opt_.monitor_network || (!opt_.iface_all && opt_.ifaces.empty())) {
        return;
    }
    if (opt_.iface_all && ifaces_.empty()) {
        resolve_ifaces();
    }

    std::unordered_map<std::string, CounterSnap> cur;
    read_netdev(cur);

    for (const auto &name : ifaces_) {
        auto it = cur.find(name);
        if (it == cur.end()) {
            continue;
        }
        const CounterSnap &now = it->second;
        IfaceDelta d;
        d.name = name;
        if (emit) {
            auto pit = prev_iface_.find(name);
            if (pit != prev_iface_.end() && pit->second.valid) {
                d.inbound = delta_u64(now.a, pit->second.a);
                d.outbound = delta_u64(now.b, pit->second.b);
                d.dropped = delta_u64(now.c, pit->second.c);
                uint64_t in_pkts = delta_u64(now.d, pit->second.d);
                uint64_t out_pkts = delta_u64(now.e, pit->second.e);
                push_point({SeriesKind::IfaceIn, name}, now_ms, static_cast<double>(d.inbound),
                           static_cast<double>(in_pkts));
                push_point({SeriesKind::IfaceOut, name}, now_ms, static_cast<double>(d.outbound),
                           static_cast<double>(out_pkts));
            }
        }
        last_ifaces_.push_back(d);
        prev_iface_[name] = now;
    }
}

static bool read_conntrack_for_addr(const std::string &addr, uint64_t *in_bytes, uint64_t *out_bytes) {
    std::ifstream in("/proc/net/nf_conntrack");
    if (!in) {
        in.open("/proc/net/ip_conntrack");
    }
    if (!in) {
        return false;
    }

    *in_bytes = 0;
    *out_bytes = 0;
    std::string line;
    while (std::getline(in, line)) {
        std::string src_m = "src=" + addr;
        std::string dst_m = "dst=" + addr;
        bool as_src = line.find(src_m) != std::string::npos;
        bool as_dst = line.find(dst_m) != std::string::npos;
        if (!as_src && !as_dst) {
            continue;
        }

        size_t first_src = line.find("src=");
        size_t second_src =
            first_src == std::string::npos ? std::string::npos : line.find("src=", first_src + 4);
        uint64_t orig_bytes = 0;
        uint64_t reply_bytes = 0;
        if (first_src != std::string::npos) {
            size_t b = line.find("bytes=", first_src);
            if (b != std::string::npos && (second_src == std::string::npos || b < second_src)) {
                orig_bytes = strtoull(line.c_str() + b + 6, nullptr, 10);
            }
        }
        if (second_src != std::string::npos) {
            size_t b = line.find("bytes=", second_src);
            if (b != std::string::npos) {
                reply_bytes = strtoull(line.c_str() + b + 6, nullptr, 10);
            }
        }

        size_t addr_as_orig_src = line.find(src_m);
        if (addr_as_orig_src != std::string::npos &&
            (second_src == std::string::npos || addr_as_orig_src < second_src)) {
            *out_bytes += orig_bytes;
            *in_bytes += reply_bytes;
        } else {
            *in_bytes += orig_bytes;
            *out_bytes += reply_bytes;
        }
    }
    return true;
}

void Collector::sample_addrs(int64_t now_ms, bool emit) {
    if (opt_.addrs.empty()) {
        return;
    }

    std::unordered_map<std::string, CounterSnap> netdev;
    read_netdev(netdev);

    for (const auto &addr : opt_.addrs) {
        CounterSnap now;
        auto local = addr_to_iface_.find(addr);
        if (local != addr_to_iface_.end()) {
            auto nit = netdev.find(local->second);
            if (nit != netdev.end()) {
                now = nit->second;
                now.valid = true;
            }
        } else {
            uint64_t in_b = 0, out_b = 0;
            if (read_conntrack_for_addr(addr, &in_b, &out_b)) {
                now.a = in_b;
                now.b = out_b;
                now.valid = true;
            }
        }

        AddrDelta d;
        d.name = addr;
        if (emit && now.valid) {
            auto pit = prev_addr_.find(addr);
            if (pit != prev_addr_.end() && pit->second.valid) {
                d.inbound = delta_u64(now.a, pit->second.a);
                d.outbound = delta_u64(now.b, pit->second.b);
                push_point({SeriesKind::AddrIn, addr}, now_ms, static_cast<double>(d.inbound));
                push_point({SeriesKind::AddrOut, addr}, now_ms, static_cast<double>(d.outbound));
            }
        }
        if (now.valid) {
            prev_addr_[addr] = now;
        }
        last_addrs_.push_back(d);
    }
}

static bool read_proc_stat_total(uint64_t *total) {
    std::ifstream in("/proc/stat");
    std::string cpu;
    uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    if (!(in >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
        return false;
    }
    *total = user + nice + system + idle + iowait + irq + softirq + steal;
    return true;
}

static bool read_cpu_snaps(std::vector<CounterSnap> &out) {
    std::ifstream in("/proc/stat");
    if (!in) {
        return false;
    }
    out.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.compare(0, 3, "cpu") != 0) {
            if (!out.empty()) {
                break;
            }
            continue;
        }
        std::istringstream ss(line);
        std::string label;
        uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0,
                 steal = 0;
        ss >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        CounterSnap snap;
        snap.a = idle + iowait;
        snap.b = user + nice + system + idle + iowait + irq + softirq + steal;
        snap.valid = true;
        out.push_back(snap);
    }
    return !out.empty();
}

void Collector::sample_cpu(int64_t now_ms, bool emit) {
    if (!opt_.show_cpu) {
        return;
    }
    std::vector<CounterSnap> cur;
    if (!read_cpu_snaps(cur) || cur.empty()) {
        return;
    }

    last_cpu_.core_pct.assign(cur.size() > 1 ? cur.size() - 1 : 0, 0.0);

    if (emit && prev_cpu_.size() == cur.size()) {
        for (size_t i = 0; i < cur.size(); ++i) {
            uint64_t didle = delta_u64(cur[i].a, prev_cpu_[i].a);
            uint64_t dtotal = delta_u64(cur[i].b, prev_cpu_[i].b);
            double pct = 0;
            if (dtotal > 0) {
                pct = 100.0 * (1.0 - static_cast<double>(didle) / static_cast<double>(dtotal));
                if (pct < 0) {
                    pct = 0;
                }
                if (pct > 100) {
                    pct = 100;
                }
            }
            if (i == 0) {
                last_cpu_.overall_pct = pct;
                push_point({SeriesKind::CpuOverall, "all"}, now_ms, pct);
            } else {
                last_cpu_.core_pct[i - 1] = pct;
                push_point({SeriesKind::CpuCore, std::to_string(i - 1)}, now_ms, pct);
            }
        }
    }
    prev_cpu_ = cur;
}

void Collector::sample_memory(int64_t now_ms, bool emit) {
    if (!opt_.show_memory) {
        return;
    }
    std::ifstream in("/proc/meminfo");
    if (!in) {
        return;
    }
    uint64_t total = 0, avail = 0, free_k = 0, buffers = 0, cached = 0, swap_total = 0, swap_free = 0;
    std::string key;
    uint64_t val = 0;
    std::string unit;
    while (in >> key >> val >> unit) {
        if (!key.empty() && key.back() == ':') {
            key.pop_back();
        }
        if (key == "MemTotal") {
            total = val;
        } else if (key == "MemAvailable") {
            avail = val;
        } else if (key == "MemFree") {
            free_k = val;
        } else if (key == "Buffers") {
            buffers = val;
        } else if (key == "Cached") {
            cached = val;
        } else if (key == "SwapTotal") {
            swap_total = val;
        } else if (key == "SwapFree") {
            swap_free = val;
        }
    }
    if (avail == 0) {
        avail = free_k + buffers + cached;
    }
    uint64_t used = total > avail ? total - avail : 0;
    uint64_t swap_used = swap_total > swap_free ? swap_total - swap_free : 0;

    last_mem_.total_kb = total;
    last_mem_.used_kb = used;
    last_mem_.avail_kb = avail;
    last_mem_.swap_used_kb = swap_used;

    if (emit) {
        /* Store as bytes for consistent FormatValue scaling. */
        push_point({SeriesKind::MemUsed, "used"}, now_ms, static_cast<double>(used) * 1024.0);
        push_point({SeriesKind::MemAvail, "avail"}, now_ms, static_cast<double>(avail) * 1024.0);
        push_point({SeriesKind::MemSwap, "swap"}, now_ms, static_cast<double>(swap_used) * 1024.0);
    }
}

static bool read_pid_jiffies(pid_t pid, uint64_t *jiffies) {
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto rparen = content.rfind(')');
    if (rparen == std::string::npos) {
        return false;
    }
    std::istringstream ss(content.substr(rparen + 1));
    std::string state;
    long ppid, pgrp, session, tty, tpgid;
    unsigned long flags, minflt, cminflt, majflt, cmajflt, utime, stime;
    if (!(ss >> state >> ppid >> pgrp >> session >> tty >> tpgid >> flags >> minflt >> cminflt >>
          majflt >> cmajflt >> utime >> stime)) {
        return false;
    }
    *jiffies = utime + stime;
    return true;
}

static bool read_pid_io(pid_t pid, uint64_t *read_bytes, uint64_t *write_bytes) {
    std::string path = "/proc/" + std::to_string(pid) + "/io";
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    *read_bytes = 0;
    *write_bytes = 0;
    std::string key;
    uint64_t val = 0;
    while (in >> key >> val) {
        if (!key.empty() && key.back() == ':') {
            key.pop_back();
        }
        if (key == "read_bytes") {
            *read_bytes = val;
        } else if (key == "write_bytes") {
            *write_bytes = val;
        }
    }
    return true;
}

void Collector::sample_pids(int64_t now_ms, bool emit) {
    if (tracked_.empty()) {
        return;
    }

    uint64_t total_j = 0;
    read_proc_stat_total(&total_j);

    for (const auto &tp : tracked_) {
        pid_t pid = tp.pid;
        std::string id = std::to_string(pid);
        PidDelta d;
        d.pid = pid;
        d.name = tp.name;
        d.label = tp.label;
        d.style_index = tp.style_index;

        uint64_t pj = 0;
        uint64_t rb = 0, wb = 0;
        bool alive = read_pid_jiffies(pid, &pj);
        bool io_ok = read_pid_io(pid, &rb, &wb);
        d.alive = alive;

        if (emit && alive) {
            auto pj_it = prev_pid_jiffies_.find(pid);
            if (pj_it != prev_pid_jiffies_.end() && prev_total_jiffies_ > 0 &&
                total_j > prev_total_jiffies_) {
                uint64_t dj = delta_u64(pj, pj_it->second);
                uint64_t dt = total_j - prev_total_jiffies_;
                d.cpu_pct = 100.0 * static_cast<double>(dj) / static_cast<double>(dt);
                push_point({SeriesKind::PidCpu, id}, now_ms, d.cpu_pct);
            }
        }
        if (emit && io_ok) {
            auto io_it = prev_pid_io_.find(pid);
            if (io_it != prev_pid_io_.end() && io_it->second.valid) {
                d.read_bytes = delta_u64(rb, io_it->second.a);
                d.write_bytes = delta_u64(wb, io_it->second.b);
                push_point({SeriesKind::PidRead, id}, now_ms, static_cast<double>(d.read_bytes));
                push_point({SeriesKind::PidWrite, id}, now_ms, static_cast<double>(d.write_bytes));
            }
        }

        if (alive) {
            prev_pid_jiffies_[pid] = pj;
        }
        if (io_ok) {
            CounterSnap snap;
            snap.a = rb;
            snap.b = wb;
            snap.valid = true;
            prev_pid_io_[pid] = snap;
        }
        last_pids_.push_back(d);
    }
    prev_total_jiffies_ = total_j;
}

static char read_tid_state(pid_t pid, int tid) {
    std::string path = "/proc/" + std::to_string(pid) + "/task/" + std::to_string(tid) + "/stat";
    std::ifstream in(path);
    if (!in) {
        return '?';
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto rparen = content.rfind(')');
    if (rparen == std::string::npos || rparen + 2 >= content.size()) {
        return '?';
    }
    return content[rparen + 2];
}

static void count_process_threads(pid_t pid, int *alive, int *wait,
                                  std::unordered_map<int, bool> *seen) {
    *alive = 0;
    *wait = 0;
    std::string path = "/proc/" + std::to_string(pid) + "/task";
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return;
    }
    while (dirent *ent = readdir(dir)) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        char *end = nullptr;
        long tid = strtol(ent->d_name, &end, 10);
        if (!end || *end != '\0' || tid <= 0) {
            continue;
        }
        ++(*alive);
        if (seen) {
            (*seen)[static_cast<int>(tid)] = true;
        }
        char st = read_tid_state(pid, static_cast<int>(tid));
        /* Waiting: interruptible/uninterruptible sleep, stopped, etc. */
        if (st != 'R') {
            ++(*wait);
        }
    }
    closedir(dir);
}

static int count_open_fds(pid_t pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/fd";
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return 0;
    }
    int n = 0;
    while (dirent *ent = readdir(dir)) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        ++n;
    }
    closedir(dir);
    return n;
}

static void collect_socket_inodes(pid_t pid, std::unordered_map<unsigned long, bool> &out) {
    std::string path = "/proc/" + std::to_string(pid) + "/fd";
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return;
    }
    char linkbuf[256];
    while (dirent *ent = readdir(dir)) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        std::string fpath = path + "/" + ent->d_name;
        ssize_t n = readlink(fpath.c_str(), linkbuf, sizeof(linkbuf) - 1);
        if (n <= 0) {
            continue;
        }
        linkbuf[n] = '\0';
        unsigned long ino = 0;
        if (sscanf(linkbuf, "socket:[%lu]", &ino) == 1 && ino > 0) {
            out[ino] = true;
        }
    }
    closedir(dir);
}

/* TCP states: 01 ESTABLISHED; FIN/wait-ish: 04-06, 08-09, 0B. */
static bool tcp_state_alive(unsigned st) {
    return st == 0x01;
}
static bool tcp_state_wait(unsigned st) {
    return st == 0x04 || st == 0x05 || st == 0x06 || st == 0x08 || st == 0x09 || st == 0x0B;
}

struct SockRow {
    unsigned long inode = 0;
    unsigned state = 0;
    bool is_tcp = false;
};

static void read_proc_net_socks(const char *path, bool is_tcp, std::vector<SockRow> &out) {
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    std::getline(in, line); /* header */
    while (std::getline(in, line)) {
        /* sl local_address rem_address st ... uid timeout inode */
        unsigned st = 0;
        unsigned long inode = 0;
        if (sscanf(line.c_str(), " %*d: %*s %*s %X %*s %*s %*s %*d %*d %lu", &st, &inode) < 2) {
            continue;
        }
        if (inode == 0) {
            continue;
        }
        out.push_back(SockRow{inode, st, is_tcp});
    }
}

static void load_all_socks(std::vector<SockRow> &rows) {
    rows.clear();
    read_proc_net_socks("/proc/net/tcp", true, rows);
    read_proc_net_socks("/proc/net/tcp6", true, rows);
    read_proc_net_socks("/proc/net/udp", false, rows);
    read_proc_net_socks("/proc/net/udp6", false, rows);
}

void Collector::sample_threads(int64_t now_ms, bool emit) {
    if (!opt_.show_threads) {
        return;
    }

    if (tracked_.empty()) {
        /* System-wide: /proc/loadavg field4 = runnable/total; procs_blocked ≈ wait. */
        ThreadDelta d;
        d.pid = 0;
        d.name = "system";
        d.label = "system";
        std::ifstream lav("/proc/loadavg");
        std::string a, b, c, field4;
        if (lav >> a >> b >> c >> field4) {
            auto slash = field4.find('/');
            if (slash != std::string::npos) {
                d.alive = atoi(field4.c_str() + slash + 1);
            }
        }
        std::ifstream st("/proc/stat");
        std::string key;
        while (st >> key) {
            if (key == "procs_blocked") {
                st >> d.wait;
                break;
            }
            std::string rest;
            std::getline(st, rest);
        }
        auto &seen = seen_tids_[0];
        /* Approximate total-opened growth by high-water of alive. */
        if (static_cast<uint64_t>(d.alive) > seen.size()) {
            while (seen.size() < static_cast<size_t>(d.alive)) {
                seen[static_cast<int>(seen.size() + 1)] = true;
            }
        }
        d.total_opened = seen.size();
        if (emit) {
            push_point({SeriesKind::ThreadsAlive, "system"}, now_ms, d.alive);
            push_point({SeriesKind::ThreadsWait, "system"}, now_ms, d.wait);
            push_point({SeriesKind::ThreadsTotal, "system"}, now_ms,
                       static_cast<double>(d.total_opened));
        }
        last_threads_.push_back(d);
        return;
    }

    for (const auto &tp : tracked_) {
        ThreadDelta d;
        d.pid = tp.pid;
        d.name = tp.name;
        d.label = tp.label;
        d.style_index = tp.style_index;
        auto &seen = seen_tids_[tp.pid];
        count_process_threads(tp.pid, &d.alive, &d.wait, &seen);
        d.total_opened = seen.size();
        if (emit) {
            std::string id = std::to_string(tp.pid);
            push_point({SeriesKind::ThreadsAlive, id}, now_ms, d.alive);
            push_point({SeriesKind::ThreadsWait, id}, now_ms, d.wait);
            push_point({SeriesKind::ThreadsTotal, id}, now_ms, static_cast<double>(d.total_opened));
        }
        last_threads_.push_back(d);
    }
}

void Collector::sample_numfd(int64_t now_ms, bool emit) {
    if (!opt_.show_numfd || tracked_.empty()) {
        return;
    }
    for (const auto &tp : tracked_) {
        FdDelta d;
        d.pid = tp.pid;
        d.name = tp.name;
        d.label = tp.label;
        d.style_index = tp.style_index;
        d.open_fds = count_open_fds(tp.pid);
        if (emit) {
            push_point({SeriesKind::FdOpen, std::to_string(tp.pid)}, now_ms, d.open_fds);
        }
        last_fds_.push_back(d);
    }
}

void Collector::sample_connections(int64_t now_ms, bool emit) {
    if (!opt_.show_connections) {
        return;
    }

    std::vector<SockRow> socks;
    load_all_socks(socks);
    std::unordered_map<unsigned long, SockRow> by_ino;
    for (const auto &r : socks) {
        by_ino[r.inode] = r;
    }

    auto tally = [&](pid_t key_pid, const std::string &id, const std::string &name,
                     const std::string &label, size_t style,
                     const std::unordered_map<unsigned long, bool> &inodes) {
        ConnDelta d;
        d.pid = key_pid;
        d.name = name;
        d.label = label;
        d.style_index = style;
        auto &seen = seen_conn_inodes_[key_pid];
        for (const auto &kv : inodes) {
            seen[kv.first] = true;
            auto it = by_ino.find(kv.first);
            if (it == by_ino.end()) {
                continue;
            }
            const SockRow &row = it->second;
            if (!row.is_tcp) {
                /* UDP: count as alive while present. */
                ++d.alive;
                continue;
            }
            if (tcp_state_alive(row.state)) {
                ++d.alive;
            } else if (tcp_state_wait(row.state)) {
                ++d.wait;
            }
        }
        d.total = seen.size();
        if (emit) {
            push_point({SeriesKind::ConnAlive, id}, now_ms, d.alive);
            push_point({SeriesKind::ConnWait, id}, now_ms, d.wait);
            push_point({SeriesKind::ConnTotal, id}, now_ms, static_cast<double>(d.total));
        }
        last_conns_.push_back(d);
    };

    if (tracked_.empty()) {
        std::unordered_map<unsigned long, bool> all;
        for (const auto &r : socks) {
            all[r.inode] = true;
        }
        tally(0, "system", "system", "system", 0, all);
        return;
    }

    for (const auto &tp : tracked_) {
        std::unordered_map<unsigned long, bool> inodes;
        collect_socket_inodes(tp.pid, inodes);
        tally(tp.pid, std::to_string(tp.pid), tp.name, tp.label, tp.style_index, inodes);
    }
}
