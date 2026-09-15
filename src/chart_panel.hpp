/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_CHART_PANEL_HPP
#define PIDLOAD_CHART_PANEL_HPP

#include "collector.hpp"
#include "options.hpp"
#include "time_view.hpp"

#include <wx/panel.h>
#include <wx/pen.h>
#include <wx/timer.h>

#include <functional>
#include <unordered_map>
#include <vector>

void apply_process_pen_style(wxPen &pen, size_t style_index);

struct DrawnSeries {
    const Series *series = nullptr;
    wxColour color;
    size_t color_index = 0;
    size_t style_index = 0;
    bool enabled = true;
};

class ChartPanel : public wxPanel {
public:
    ChartPanel(wxWindow *parent, const ChartSpec &spec, Collector *collector, TimeView *view);

    void RefreshData();
    void SetShowLegends(bool on);
    void SetYLog(bool on);
    void SetShowAs(ChartShowAs mode);
    void SetCurveStyle(CurveStyle style);
    void SetNetDisplayUnit(NetDisplayUnit unit);
    void SetShowTimeAxis(bool on);
    void ApplyViewDefaults(const Options &opt);

    bool show_legends() const { return show_legends_; }
    bool y_log() const { return y_log_; }
    ChartShowAs show_as() const { return show_as_; }
    CurveStyle curve_style() const { return curve_style_; }
    NetDisplayUnit net_unit() const { return net_unit_; }
    const ChartSpec &spec() const { return spec_; }
    void UpdateSpec(const ChartSpec &spec);

    void SetRemoveHandler(std::function<void()> handler) { remove_handler_ = std::move(handler); }
    void SetViewChangedHandler(std::function<void()> handler) {
        view_changed_handler_ = std::move(handler);
    }

private:
    enum {
        ID_CtxLegends = wxID_HIGHEST + 200,
        ID_CtxYLog,
        ID_CtxDots,
        ID_CtxCurve,
        ID_CtxBars,
        ID_CtxStacked,
        ID_CtxCurveSeg,
        ID_CtxCurveBezier,
        ID_CtxCurveCubic,
        ID_CtxUnitRaw,
        ID_CtxUnitPayload,
        ID_CtxUnitPackets,
        ID_CtxRemove,
        ID_CtxResetView,
    };

    struct LegendHit {
        enum class Kind { Series, Process };
        Kind kind = Kind::Series;
        SeriesKey series_key{};
        pid_t pid = 0;
        wxRect rect;
    };

    void OnPaint(wxPaintEvent &event);
    void OnSize(wxSizeEvent &event);
    void OnMotion(wxMouseEvent &event);
    void OnLeave(wxMouseEvent &event);
    void OnLeftDown(wxMouseEvent &event);
    void OnLeftUp(wxMouseEvent &event);
    void OnMouseWheel(wxMouseEvent &event);
    void OnRightDown(wxMouseEvent &event);
    void OnFadeTimer(wxTimerEvent &event);
    void OnContextCommand(wxCommandEvent &event);

    bool series_enabled(const SeriesKey &key) const;
    bool process_enabled(pid_t pid) const;
    wxColour SeriesColor(size_t index, bool enabled) const;
    wxColour WithAlpha(const wxColour &c, double alpha) const;
    double PointDisplayValue(const SamplePoint &p) const;
    wxString FormatValue(double v) const;
    static wxString FormatTimeLabel(int64_t t_ms);
    bool is_process_chart() const;
    bool is_net_chart() const;
    bool point_in_plot(const wxPoint &p) const;
    int64_t time_at_x(int x) const;
    void notify_view_changed();
    void DrawSeries(wxDC &dc, const std::vector<DrawnSeries> &drawn, const wxRect &plot,
                    int64_t tmin, int64_t tmax, double ymin, double ymax,
                    const std::function<int(int64_t)> &map_x,
                    const std::function<int(double)> &map_y, double y_floor);

    ChartSpec spec_;
    Collector *collector_;
    TimeView *view_ = nullptr;
    bool show_legends_ = true;
    bool y_log_ = false;
    bool show_time_axis_ = false;
    ChartShowAs show_as_ = ChartShowAs::Curve;
    CurveStyle curve_style_ = CurveStyle::Segment;
    NetDisplayUnit net_unit_ = NetDisplayUnit::RawSize;
    bool hover_ = false;
    double legend_alpha_ = 0.45;
    wxTimer fade_timer_;
    std::vector<LegendHit> legend_hits_;
    std::unordered_map<SeriesKey, bool, SeriesKeyHash> enabled_series_;
    std::unordered_map<pid_t, bool> enabled_procs_;
    std::function<void()> remove_handler_;
    std::function<void()> view_changed_handler_;

    bool panning_ = false;
    wxPoint pan_last_{};
    wxRect plot_rect_;
};

#endif /* PIDLOAD_CHART_PANEL_HPP */
