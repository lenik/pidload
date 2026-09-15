/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "recorder.hpp"
#include "util.hpp"

#include <bas/log/uselog.h>

#include <string>

Recorder::Recorder(const Options &opt) : dir_(opt.output_dir) {
    if (dir_.empty()) {
        ok_ = true; /* recording disabled */
        return;
    }
    if (!ensure_directory(dir_)) {
        logerror_fmt("cannot create output directory %s", dir_.c_str());
        ok_ = false;
        return;
    }
    ok_ = true;
}

Recorder::~Recorder() {
    for (auto &kv : files_) {
        if (kv.second) {
            fclose(kv.second);
        }
    }
}

FILE *Recorder::open_log(const std::string &basename, const char *header) {
    auto it = files_.find(basename);
    if (it != files_.end()) {
        return it->second;
    }
    std::string path = dir_ + "/" + safe_filename(basename) + ".log";
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
        logerror_fmt("cannot open %s for writing", path.c_str());
        files_[basename] = nullptr;
        return nullptr;
    }
    fprintf(f, "%s\n", header);
    fflush(f);
    files_[basename] = f;
    loginfo_fmt("recording to %s", path.c_str());
    return f;
}

void Recorder::write_sample(const Collector &collector) {
    if (dir_.empty() || !ok_ || !collector.have_deltas()) {
        return;
    }

    for (const auto &d : collector.last_devs()) {
        FILE *f = open_log(d.name, "; read write");
        if (f) {
            fprintf(f, "%llu %llu\n", static_cast<unsigned long long>(d.read_bytes),
                    static_cast<unsigned long long>(d.write_bytes));
            fflush(f);
        }
    }

    for (const auto &d : collector.last_ifaces()) {
        FILE *f = open_log(d.name, "; inbound outbound dropped");
        if (f) {
            fprintf(f, "%llu %llu %llu\n", static_cast<unsigned long long>(d.inbound),
                    static_cast<unsigned long long>(d.outbound),
                    static_cast<unsigned long long>(d.dropped));
            fflush(f);
        }
    }

    for (const auto &d : collector.last_addrs()) {
        FILE *f = open_log(d.name, "; inbound outbound");
        if (f) {
            fprintf(f, "%llu %llu\n", static_cast<unsigned long long>(d.inbound),
                    static_cast<unsigned long long>(d.outbound));
            fflush(f);
        }
    }

    for (const auto &d : collector.last_pids()) {
        std::string base = safe_filename(d.name) + "." + std::to_string(d.pid);
        FILE *f = open_log(base, "; cpu_pct read write");
        if (f) {
            fprintf(f, "%.2f %llu %llu\n", d.cpu_pct,
                    static_cast<unsigned long long>(d.read_bytes),
                    static_cast<unsigned long long>(d.write_bytes));
            fflush(f);
        }
    }

    if (collector.options().show_cpu) {
        const auto &cpu = collector.last_cpu();
        std::string header = "; overall";
        for (size_t i = 0; i < cpu.core_pct.size(); ++i) {
            header += " core" + std::to_string(i);
        }
        FILE *f = open_log("cpu", header.c_str());
        if (f) {
            fprintf(f, "%.2f", cpu.overall_pct);
            for (double c : cpu.core_pct) {
                fprintf(f, " %.2f", c);
            }
            fputc('\n', f);
            fflush(f);
        }
    }

    if (collector.options().show_memory) {
        const auto &m = collector.last_mem();
        FILE *f = open_log("memory", "; used_kb avail_kb swap_used_kb");
        if (f) {
            fprintf(f, "%llu %llu %llu\n", static_cast<unsigned long long>(m.used_kb),
                    static_cast<unsigned long long>(m.avail_kb),
                    static_cast<unsigned long long>(m.swap_used_kb));
            fflush(f);
        }
    }

    for (const auto &d : collector.last_threads()) {
        std::string base =
            d.pid > 0 ? (safe_filename(d.name) + "." + std::to_string(d.pid) + ".threads")
                      : "threads";
        FILE *f = open_log(base, "; alive wait total_opened");
        if (f) {
            fprintf(f, "%d %d %llu\n", d.alive, d.wait,
                    static_cast<unsigned long long>(d.total_opened));
            fflush(f);
        }
    }

    for (const auto &d : collector.last_fds()) {
        std::string base = safe_filename(d.name) + "." + std::to_string(d.pid) + ".fds";
        FILE *f = open_log(base, "; open_fds");
        if (f) {
            fprintf(f, "%d\n", d.open_fds);
            fflush(f);
        }
    }

    for (const auto &d : collector.last_conns()) {
        std::string base =
            d.pid > 0 ? (safe_filename(d.name) + "." + std::to_string(d.pid) + ".conn")
                      : "connections";
        FILE *f = open_log(base, "; alive wait total");
        if (f) {
            fprintf(f, "%d %d %llu\n", d.alive, d.wait, static_cast<unsigned long long>(d.total));
            fflush(f);
        }
    }
}
