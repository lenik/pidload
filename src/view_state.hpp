/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_VIEW_STATE_HPP
#define PIDLOAD_VIEW_STATE_HPP

#include "options.hpp"

#include <string>
#include <vector>

/* Classify one NAME token for the session key: "pid N", "glob PAT", or "path P". */
std::string classify_name_token(const std::string &name);

/* Comma-joined classifications, e.g. "path /bin/foo,pid 18612,glob bar*bar". */
std::string view_state_key_material(const std::vector<std::string> &names);

/* Lowercase hex SHA-1 of the UTF-8 key material. */
std::string utf8sha1_hex(const std::string &utf8);

/* ~/.config/pidload (or $XDG_CONFIG_HOME/pidload). Empty if HOME unset. */
std::string view_state_config_dir();

/* Full path: <config_dir>/<hex>.state */
std::string view_state_path_for_names(const std::vector<std::string> &names);

struct ViewState {
    bool show_cpu = false;
    bool show_memory = false;
    bool monitor_network = false;
    bool iface_all = false;
    std::vector<std::string> ifaces;
    bool show_threads = false;
    bool show_numfd = false;
    bool show_connections = false;
    bool show_legends = true;
    bool y_log = false;
    ChartShowAs show_as = ChartShowAs::Curve;
    CurveStyle curve_style = CurveStyle::Segment;
    NetDisplayUnit net_unit = NetDisplayUnit::RawSize;
    int64_t interval_ms = 2000;
    int frame_w = 0;
    int frame_h = 0;
    int frame_x = 0;
    int frame_y = 0;
    bool have_frame_pos = false;
    std::string aui_perspective;
};

bool load_view_state(const std::string &path, ViewState &out);
bool save_view_state(const std::string &path, const ViewState &st);

/* Overlay saved view prefs onto Options, honoring cli_* overrides. */
void apply_view_state(Options &opt, const ViewState &st);

/* Snapshot view prefs from live Options (toggles / soft prefs). */
void capture_view_state_from_options(const Options &opt, ViewState &st);

#endif /* PIDLOAD_VIEW_STATE_HPP */
