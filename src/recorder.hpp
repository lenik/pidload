/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_RECORDER_HPP
#define PIDLOAD_RECORDER_HPP

#include "collector.hpp"
#include "options.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>

class Recorder {
public:
    explicit Recorder(const Options &opt);
    ~Recorder();

    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    bool ok() const { return ok_; }
    void write_sample(const Collector &collector);

private:
    FILE *open_log(const std::string &basename, const char *header);

    std::string dir_;
    bool ok_ = false;
    std::unordered_map<std::string, FILE *> files_;
};

#endif /* PIDLOAD_RECORDER_HPP */
