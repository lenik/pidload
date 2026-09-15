/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_COLLECTOR_HPP
#define PIDLOAD_COLLECTOR_HPP

#include "options.hpp"
#include "process_match.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class SeriesKind {
    DevRead,
    DevWrite,
    IfaceIn,
    IfaceOut,
    IfaceDrop,
    AddrIn,
    AddrOut,
    PidCpu,
    PidRead,
    PidWrite,
    CpuOverall,
    CpuCore,
    MemUsed,
    MemAvail,
    MemSwap,
    ThreadsAlive,
    ThreadsWait,
    ThreadsTotal,
    FdOpen,
    ConnAlive,
    ConnWait,
    ConnTotal,
};

struct SeriesKey {
    SeriesKind kind;
    std::string name; /* device, iface, addr, or pid string */

    bool operator==(const SeriesKey &o) const {
        return kind == o.kind && name == o.name;
    }
};

struct SeriesKeyHash {
    size_t operator()(const SeriesKey &k) const {
        return std::hash<int>()(static_cast<int>(k.kind)) ^ (std::hash<std::string>()(k.name) << 1);
    }
};

struct SamplePoint {
    int64_t t_ms = 0;
    double value = 0;
    double aux = 0; /* e.g. packet count for network series */
};

struct Series {
    SeriesKey key;
    std::string label;
    size_t style_index = 0;
    std::string match_name;
    pid_t pid = 0;
    std::vector<SamplePoint> points;
};

enum class ChartType {
    Device,
    Network,
    Address,
    ProcessCpu,
    ProcessIo,
    SystemCpu,
    SystemMemory,
    Threads,
    NumFd,
    Connections,
};

struct ChartSpec {
    ChartType type;
    std::string title;
    std::string id;
    std::vector<SeriesKey> series;
};

struct CounterSnap {
    uint64_t a = 0;
    uint64_t b = 0;
    uint64_t c = 0;
    uint64_t d = 0;
    uint64_t e = 0;
    bool valid = false;
};

struct Collector {
    explicit Collector(const Options &opt);

    /* Returns true if chart membership changed (caller should rebuild panes). */
    bool tick(int64_t now_ms);

    const std::vector<ChartSpec> &charts() const { return charts_; }
    const std::unordered_map<SeriesKey, Series, SeriesKeyHash> &series() const { return series_; }
    const std::vector<TrackedProcess> &tracked() const { return tracked_; }
    Options &options() { return opt_; }
    const Options &options() const { return opt_; }

    bool set_show_cpu(bool on);
    bool set_show_memory(bool on);
    bool set_show_threads(bool on);
    bool set_show_numfd(bool on);
    bool set_show_connections(bool on);
    bool set_monitor_network(bool on, bool all, const std::vector<std::string> &ifaces);
    bool add_device(const std::string &dev);
    bool add_iface(const std::string &iface);
    bool add_addr(const std::string &addr);
    bool add_name(const std::string &name);
    bool add_pid(pid_t pid);
    bool remove_chart(const std::string &chart_id);
    void clear_history();
    void set_window_ms(int64_t window_ms);
    void set_interval_ms(int64_t interval_ms);

    struct DevDelta {
        std::string name;
        uint64_t read_bytes = 0;
        uint64_t write_bytes = 0;
    };
    struct IfaceDelta {
        std::string name;
        uint64_t inbound = 0;
        uint64_t outbound = 0;
        uint64_t dropped = 0;
    };
    struct AddrDelta {
        std::string name;
        uint64_t inbound = 0;
        uint64_t outbound = 0;
    };
    struct PidDelta {
        pid_t pid = 0;
        std::string name;
        std::string label;
        size_t style_index = 0;
        double cpu_pct = 0;
        uint64_t read_bytes = 0;
        uint64_t write_bytes = 0;
        bool alive = true;
    };
    struct CpuDelta {
        double overall_pct = 0;
        std::vector<double> core_pct;
    };
    struct MemDelta {
        uint64_t used_kb = 0;
        uint64_t avail_kb = 0;
        uint64_t swap_used_kb = 0;
        uint64_t total_kb = 0;
    };

    struct ThreadDelta {
        pid_t pid = 0;
        std::string name;
        std::string label;
        size_t style_index = 0;
        int alive = 0;
        int wait = 0;
        uint64_t total_opened = 0;
    };
    struct FdDelta {
        pid_t pid = 0;
        std::string name;
        std::string label;
        size_t style_index = 0;
        int open_fds = 0;
    };
    struct ConnDelta {
        pid_t pid = 0;
        std::string name;
        std::string label;
        size_t style_index = 0;
        int alive = 0;
        int wait = 0;
        uint64_t total = 0;
    };

    const std::vector<DevDelta> &last_devs() const { return last_devs_; }
    const std::vector<IfaceDelta> &last_ifaces() const { return last_ifaces_; }
    const std::vector<AddrDelta> &last_addrs() const { return last_addrs_; }
    const std::vector<PidDelta> &last_pids() const { return last_pids_; }
    const CpuDelta &last_cpu() const { return last_cpu_; }
    const MemDelta &last_mem() const { return last_mem_; }
    const std::vector<ThreadDelta> &last_threads() const { return last_threads_; }
    const std::vector<FdDelta> &last_fds() const { return last_fds_; }
    const std::vector<ConnDelta> &last_conns() const { return last_conns_; }
    bool have_deltas() const { return have_deltas_; }

    size_t max_points() const { return max_points_; }
    size_t session_max_points() const { return session_max_points_; }
    int core_count() const { return core_count_; }
    int64_t data_t_min() const;
    int64_t data_t_max() const;

private:
    void rebuild_charts();
    bool sync_tracked();
    void push_point(const SeriesKey &key, int64_t t_ms, double value, double aux = 0);
    void ensure_series(const SeriesKey &key, const std::string &label, size_t style_index = 0,
                       const std::string &match_name = {}, pid_t pid = 0);
    void resolve_ifaces();
    void refresh_addr_map();
    int detect_cores();
    void sample_devices(int64_t now_ms, bool emit);
    void sample_ifaces(int64_t now_ms, bool emit);
    void sample_addrs(int64_t now_ms, bool emit);
    void sample_pids(int64_t now_ms, bool emit);
    void sample_cpu(int64_t now_ms, bool emit);
    void sample_memory(int64_t now_ms, bool emit);
    void sample_threads(int64_t now_ms, bool emit);
    void sample_numfd(int64_t now_ms, bool emit);
    void sample_connections(int64_t now_ms, bool emit);

    Options opt_;
    size_t max_points_ = 90;
    size_t session_max_points_ = 50000;
    bool have_prev_ = false;
    bool have_deltas_ = false;
    int core_count_ = 0;

    std::vector<TrackedProcess> tracked_;
    std::vector<std::string> ifaces_;
    std::unordered_map<std::string, std::string> addr_to_iface_;

    std::unordered_map<std::string, CounterSnap> prev_dev_;
    std::unordered_map<std::string, CounterSnap> prev_iface_;
    std::unordered_map<std::string, CounterSnap> prev_addr_;
    std::unordered_map<pid_t, CounterSnap> prev_pid_io_;
    std::unordered_map<pid_t, uint64_t> prev_pid_jiffies_;
    uint64_t prev_total_jiffies_ = 0;
    std::vector<CounterSnap> prev_cpu_;

    std::unordered_map<pid_t, std::unordered_map<int, bool>> seen_tids_;
    std::unordered_map<pid_t, std::unordered_map<unsigned long, bool>> seen_conn_inodes_;

    std::vector<ChartSpec> charts_;
    std::unordered_map<SeriesKey, Series, SeriesKeyHash> series_;

    std::vector<DevDelta> last_devs_;
    std::vector<IfaceDelta> last_ifaces_;
    std::vector<AddrDelta> last_addrs_;
    std::vector<PidDelta> last_pids_;
    CpuDelta last_cpu_;
    MemDelta last_mem_;
    std::vector<ThreadDelta> last_threads_;
    std::vector<FdDelta> last_fds_;
    std::vector<ConnDelta> last_conns_;
};

const char *series_kind_name(SeriesKind k);
const char *chart_type_title(ChartType t);

#endif /* PIDLOAD_COLLECTOR_HPP */
