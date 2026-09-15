/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_TIME_VIEW_HPP
#define PIDLOAD_TIME_VIEW_HPP

#include <algorithm>
#include <cstdint>

/* Shared session time axis for synchronized pan/zoom across charts.
 * History lives in Collector series; this only controls the visible window. */
struct TimeView {
    int64_t session_t_max = 0;     /* latest sample time (ms since start) */
    int64_t view_end = 0;          /* right edge of visible window */
    int64_t view_span = 180000;    /* visible duration (= capture window default) */
    int64_t min_span_ms = 1000;
    int64_t max_span_ms = 24LL * 3600 * 1000;
    bool follow_live = true;

    int64_t view_start() const {
        return view_end > view_span ? view_end - view_span : 0;
    }

    void set_capture_window(int64_t window_ms) {
        if (window_ms < min_span_ms) {
            window_ms = min_span_ms;
        }
        view_span = window_ms;
        if (follow_live) {
            view_end = session_t_max;
        }
        clamp();
    }

    void on_sample(int64_t t_ms) {
        if (t_ms > session_t_max) {
            session_t_max = t_ms;
        }
        if (follow_live) {
            view_end = session_t_max;
        }
        clamp();
    }

    void pan_ms(int64_t delta) {
        follow_live = false;
        view_end += delta;
        clamp();
        /* Re-engage live follow if user pans back to the live edge. */
        if (view_end >= session_t_max) {
            view_end = session_t_max;
            follow_live = true;
        }
    }

    /* Zoom keeping anchor_t fixed in the view. factor > 1 zooms out. */
    void zoom_at(int64_t anchor_t, double factor) {
        if (factor <= 0) {
            return;
        }
        follow_live = false;
        double rel = 0.5;
        if (view_span > 0) {
            rel = static_cast<double>(anchor_t - view_start()) / static_cast<double>(view_span);
            rel = std::clamp(rel, 0.0, 1.0);
        }
        int64_t new_span = static_cast<int64_t>(static_cast<double>(view_span) * factor + 0.5);
        new_span = std::clamp(new_span, min_span_ms, max_span_ms);
        view_span = new_span;
        view_end = anchor_t + static_cast<int64_t>((1.0 - rel) * static_cast<double>(view_span));
        clamp();
        if (view_end >= session_t_max) {
            view_end = session_t_max;
            follow_live = true;
        }
    }

    void reset_live() {
        follow_live = true;
        view_end = session_t_max;
        clamp();
    }

    void clamp() {
        if (view_span < min_span_ms) {
            view_span = min_span_ms;
        }
        if (view_span > max_span_ms) {
            view_span = max_span_ms;
        }
        if (view_end < view_span) {
            view_end = view_span;
        }
        if (session_t_max > 0 && view_end > session_t_max) {
            view_end = session_t_max;
        }
    }
};

#endif /* PIDLOAD_TIME_VIEW_HPP */
