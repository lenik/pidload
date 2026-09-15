/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "chart_panel.hpp"
#include "wx_tr.hpp"

#include <wx/dcbuffer.h>
#include <wx/menu.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {
constexpr double kLegendIdle = 0.42;
constexpr double kLegendHover = 1.0;

/* Built-in styles cycle; extras use stable static dash tables (wx requires
 * the dash array to outlive the pen). */
wxDash kDash1[] = {4, 3};
wxDash kDash2[] = {1, 3};
wxDash kDash3[] = {4, 2, 1, 2};
wxDash kDash4[] = {8, 3};
wxDash kDash5[] = {6, 2, 1, 2};
wxDash kDash6[] = {1, 2, 1, 2, 4, 2};
wxDash kDash7[] = {2, 2};
wxDash kDash8[] = {10, 2, 2, 2};
wxDash kDash9[] = {3, 2, 1, 2, 1, 2};

struct DashStyle {
    int n;
    const wxDash *dashes;
};

const DashStyle kStyles[10] = {
    {0, nullptr},
    {2, kDash1},
    {2, kDash2},
    {4, kDash3},
    {2, kDash4},
    {4, kDash5},
    {6, kDash6},
    {2, kDash7},
    {4, kDash8},
    {6, kDash9},
};
} // namespace

void apply_process_pen_style(wxPen &pen, size_t style_index) {
    const DashStyle &st = kStyles[style_index % 10];
    if (st.n <= 0 || !st.dashes) {
        pen.SetStyle(wxPENSTYLE_SOLID);
        return;
    }
    pen.SetStyle(wxPENSTYLE_USER_DASH);
    pen.SetDashes(st.n, st.dashes);
}

ChartPanel::ChartPanel(wxWindow *parent, const ChartSpec &spec, Collector *collector, TimeView *view)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL),
      spec_(spec),
      collector_(collector),
      view_(view) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    fade_timer_.SetOwner(this);
    Bind(wxEVT_PAINT, &ChartPanel::OnPaint, this);
    Bind(wxEVT_SIZE, &ChartPanel::OnSize, this);
    Bind(wxEVT_MOTION, &ChartPanel::OnMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, &ChartPanel::OnLeave, this);
    Bind(wxEVT_LEFT_DOWN, &ChartPanel::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &ChartPanel::OnLeftUp, this);
    Bind(wxEVT_MOUSEWHEEL, &ChartPanel::OnMouseWheel, this);
    Bind(wxEVT_RIGHT_DOWN, &ChartPanel::OnRightDown, this);
    Bind(wxEVT_TIMER, &ChartPanel::OnFadeTimer, this);
    Bind(wxEVT_MENU, &ChartPanel::OnContextCommand, this);
}

void ChartPanel::UpdateSpec(const ChartSpec &spec) {
    spec_ = spec;
    Refresh(false);
}

void ChartPanel::RefreshData() {
    Refresh(false);
}

void ChartPanel::SetShowLegends(bool on) {
    show_legends_ = on;
    Refresh(false);
}

void ChartPanel::SetYLog(bool on) {
    y_log_ = on;
    Refresh(false);
}

void ChartPanel::SetShowAs(ChartShowAs mode) {
    show_as_ = mode;
    Refresh(false);
}

void ChartPanel::SetCurveStyle(CurveStyle style) {
    curve_style_ = style;
    Refresh(false);
}

void ChartPanel::SetNetDisplayUnit(NetDisplayUnit unit) {
    net_unit_ = unit;
    Refresh(false);
}

void ChartPanel::SetShowTimeAxis(bool on) {
    show_time_axis_ = on;
    Refresh(false);
}

void ChartPanel::ApplyViewDefaults(const Options &opt) {
    show_legends_ = opt.show_legends;
    y_log_ = opt.y_log;
    show_as_ = opt.show_as;
    curve_style_ = opt.curve_style;
    net_unit_ = opt.net_unit;
    Refresh(false);
}

void ChartPanel::notify_view_changed() {
    if (view_changed_handler_) {
        view_changed_handler_();
    } else {
        Refresh(false);
    }
}

bool ChartPanel::point_in_plot(const wxPoint &p) const {
    return plot_rect_.width > 0 && plot_rect_.Contains(p);
}

int64_t ChartPanel::time_at_x(int x) const {
    if (!view_ || plot_rect_.width <= 1) {
        return 0;
    }
    double f = static_cast<double>(x - plot_rect_.x) / static_cast<double>(plot_rect_.width - 1);
    f = std::clamp(f, 0.0, 1.0);
    return view_->view_start() +
           static_cast<int64_t>(f * static_cast<double>(view_->view_span) + 0.5);
}

void ChartPanel::OnRightDown(wxMouseEvent &event) {
    wxMenu menu;
    menu.AppendCheckItem(ID_CtxLegends, tr("Legends\tCtrl+Shift+L"));
    menu.Check(ID_CtxLegends, show_legends_);
    menu.AppendCheckItem(ID_CtxYLog, tr("Y-Log\tCtrl+Shift+Y"));
    menu.Check(ID_CtxYLog, y_log_);
    menu.AppendSeparator();
    auto *show = new wxMenu;
    show->AppendRadioItem(ID_CtxDots, tr("Dots"));
    show->AppendRadioItem(ID_CtxCurve, tr("Curve"));
    show->AppendRadioItem(ID_CtxBars, tr("Bars (siblings)"));
    show->AppendRadioItem(ID_CtxStacked, tr("Stacked Bars"));
    switch (show_as_) {
    case ChartShowAs::Dots:
        show->Check(ID_CtxDots, true);
        break;
    case ChartShowAs::Curve:
        show->Check(ID_CtxCurve, true);
        break;
    case ChartShowAs::Bars:
        show->Check(ID_CtxBars, true);
        break;
    case ChartShowAs::StackedBars:
        show->Check(ID_CtxStacked, true);
        break;
    }
    menu.AppendSubMenu(show, tr("Show as"));

    auto *curve = new wxMenu;
    curve->AppendRadioItem(ID_CtxCurveSeg, tr("Segment"));
    curve->AppendRadioItem(ID_CtxCurveBezier, tr("Bezier"));
    curve->AppendRadioItem(ID_CtxCurveCubic, tr("Bicubic"));
    switch (curve_style_) {
    case CurveStyle::Segment:
        curve->Check(ID_CtxCurveSeg, true);
        break;
    case CurveStyle::Bezier:
        curve->Check(ID_CtxCurveBezier, true);
        break;
    case CurveStyle::Bicubic:
        curve->Check(ID_CtxCurveCubic, true);
        break;
    }
    menu.AppendSubMenu(curve, tr("Display Curve"));

    if (is_net_chart()) {
        auto *unit = new wxMenu;
        unit->AppendRadioItem(ID_CtxUnitRaw, tr("Raw size"));
        unit->AppendRadioItem(ID_CtxUnitPayload, tr("Payload size"));
        unit->AppendRadioItem(ID_CtxUnitPackets, tr("Num of packets"));
        switch (net_unit_) {
        case NetDisplayUnit::RawSize:
            unit->Check(ID_CtxUnitRaw, true);
            break;
        case NetDisplayUnit::PayloadSize:
            unit->Check(ID_CtxUnitPayload, true);
            break;
        case NetDisplayUnit::Packets:
            unit->Check(ID_CtxUnitPackets, true);
            break;
        }
        menu.AppendSubMenu(unit, tr("Display Unit"));
    }

    menu.AppendSeparator();
    menu.Append(ID_CtxResetView, tr("Reset Live View\tHome"));
    menu.Append(ID_CtxRemove, tr("Remove Chart\tDelete"));
    PopupMenu(&menu, event.GetPosition());
}

void ChartPanel::OnContextCommand(wxCommandEvent &event) {
    switch (event.GetId()) {
    case ID_CtxLegends:
        SetShowLegends(!show_legends_);
        break;
    case ID_CtxYLog:
        SetYLog(!y_log_);
        break;
    case ID_CtxDots:
        SetShowAs(ChartShowAs::Dots);
        break;
    case ID_CtxCurve:
        SetShowAs(ChartShowAs::Curve);
        break;
    case ID_CtxBars:
        SetShowAs(ChartShowAs::Bars);
        break;
    case ID_CtxStacked:
        SetShowAs(ChartShowAs::StackedBars);
        break;
    case ID_CtxCurveSeg:
        SetCurveStyle(CurveStyle::Segment);
        break;
    case ID_CtxCurveBezier:
        SetCurveStyle(CurveStyle::Bezier);
        break;
    case ID_CtxCurveCubic:
        SetCurveStyle(CurveStyle::Bicubic);
        break;
    case ID_CtxUnitRaw:
        SetNetDisplayUnit(NetDisplayUnit::RawSize);
        break;
    case ID_CtxUnitPayload:
        SetNetDisplayUnit(NetDisplayUnit::PayloadSize);
        break;
    case ID_CtxUnitPackets:
        SetNetDisplayUnit(NetDisplayUnit::Packets);
        break;
    case ID_CtxResetView:
        if (view_) {
            view_->reset_live();
            notify_view_changed();
        }
        break;
    case ID_CtxRemove:
        if (remove_handler_) {
            remove_handler_();
        }
        break;
    default:
        event.Skip();
        break;
    }
}

bool ChartPanel::is_process_chart() const {
    return spec_.type == ChartType::ProcessCpu || spec_.type == ChartType::ProcessIo ||
           spec_.type == ChartType::Threads || spec_.type == ChartType::NumFd ||
           spec_.type == ChartType::Connections;
}

bool ChartPanel::is_net_chart() const {
    return spec_.type == ChartType::Network || spec_.type == ChartType::Address;
}

bool ChartPanel::series_enabled(const SeriesKey &key) const {
    auto it = enabled_series_.find(key);
    return it == enabled_series_.end() || it->second;
}

bool ChartPanel::process_enabled(pid_t pid) const {
    if (pid <= 0) {
        return true;
    }
    auto it = enabled_procs_.find(pid);
    return it == enabled_procs_.end() || it->second;
}

void ChartPanel::OnSize(wxSizeEvent &event) {
    Refresh(false);
    event.Skip();
}

void ChartPanel::OnMotion(wxMouseEvent &event) {
    if (panning_ && view_ && event.Dragging() && event.LeftIsDown()) {
        wxPoint p = event.GetPosition();
        int dx = p.x - pan_last_.x;
        if (dx != 0 && plot_rect_.width > 1) {
            double ms_per_px =
                static_cast<double>(view_->view_span) / static_cast<double>(plot_rect_.width);
            /* Drag right → look earlier (negative pan of view_end). */
            view_->pan_ms(static_cast<int64_t>(-dx * ms_per_px));
            pan_last_ = p;
            notify_view_changed();
        }
        return;
    }
    if (!hover_) {
        hover_ = true;
        if (!fade_timer_.IsRunning()) {
            fade_timer_.Start(30);
        }
    }
}

void ChartPanel::OnLeave(wxMouseEvent &) {
    if (panning_) {
        panning_ = false;
        if (HasCapture()) {
            ReleaseMouse();
        }
    }
    hover_ = false;
    if (!fade_timer_.IsRunning()) {
        fade_timer_.Start(30);
    }
}

void ChartPanel::OnFadeTimer(wxTimerEvent &) {
    double target = hover_ ? kLegendHover : kLegendIdle;
    double step = 0.12;
    if (std::fabs(legend_alpha_ - target) < step) {
        legend_alpha_ = target;
        fade_timer_.Stop();
    } else if (legend_alpha_ < target) {
        legend_alpha_ += step;
    } else {
        legend_alpha_ -= step;
    }
    Refresh(false);
}

void ChartPanel::OnLeftDown(wxMouseEvent &event) {
    wxPoint p = event.GetPosition();
    for (auto &hit : legend_hits_) {
        if (!hit.rect.Contains(p)) {
            continue;
        }
        if (hit.kind == LegendHit::Kind::Process) {
            bool cur = process_enabled(hit.pid);
            enabled_procs_[hit.pid] = !cur;
        } else if (spec_.type == ChartType::ProcessIo || spec_.type == ChartType::Threads ||
                   spec_.type == ChartType::Connections) {
            /* Toggle this metric kind across all processes. */
            SeriesKind kind = hit.series_key.kind;
            bool any_on = false;
            for (const auto &key : spec_.series) {
                if (key.kind != kind) {
                    continue;
                }
                if (series_enabled(key)) {
                    any_on = true;
                    break;
                }
            }
            for (const auto &key : spec_.series) {
                if (key.kind == kind) {
                    enabled_series_[key] = !any_on;
                }
            }
        } else {
            bool cur = series_enabled(hit.series_key);
            enabled_series_[hit.series_key] = !cur;
        }
        Refresh(false);
        return;
    }
    if (view_ && point_in_plot(p)) {
        panning_ = true;
        pan_last_ = p;
        CaptureMouse();
        return;
    }
    event.Skip();
}

void ChartPanel::OnLeftUp(wxMouseEvent &event) {
    if (panning_) {
        panning_ = false;
        if (HasCapture()) {
            ReleaseMouse();
        }
    }
    event.Skip();
}

void ChartPanel::OnMouseWheel(wxMouseEvent &event) {
    if (!view_ || !point_in_plot(event.GetPosition())) {
        event.Skip();
        return;
    }
    int64_t anchor = time_at_x(event.GetPosition().x);
    double factor = event.GetWheelRotation() > 0 ? (1.0 / 1.2) : 1.2;
    view_->zoom_at(anchor, factor);
    notify_view_changed();
}

wxColour ChartPanel::SeriesColor(size_t index, bool enabled) const {
    static const wxColour palette[] = {
        wxColour(31, 119, 180),  wxColour(255, 127, 14),  wxColour(44, 160, 44),
        wxColour(214, 39, 40),   wxColour(148, 103, 189), wxColour(140, 86, 75),
        wxColour(227, 119, 194), wxColour(127, 127, 127), wxColour(188, 189, 34),
        wxColour(23, 190, 207),
    };
    wxColour c = palette[index % (sizeof palette / sizeof palette[0])];
    if (!enabled) {
        return wxColour(180, 184, 190);
    }
    return c;
}

wxColour ChartPanel::WithAlpha(const wxColour &c, double alpha) const {
    alpha = std::clamp(alpha, 0.0, 1.0);
    int r = static_cast<int>(c.Red() * alpha + 248 * (1.0 - alpha));
    int g = static_cast<int>(c.Green() * alpha + 249 * (1.0 - alpha));
    int b = static_cast<int>(c.Blue() * alpha + 251 * (1.0 - alpha));
    return wxColour(r, g, b);
}

wxString ChartPanel::FormatValue(double v) const {
    if (spec_.type == ChartType::ProcessCpu || spec_.type == ChartType::SystemCpu) {
        return wxString::Format("%.1f%%", v);
    }
    if (spec_.type == ChartType::Threads || spec_.type == ChartType::NumFd ||
        spec_.type == ChartType::Connections ||
        (is_net_chart() && net_unit_ == NetDisplayUnit::Packets)) {
        return wxString::Format("%.0f", v);
    }
    const char *unit = "B";
    double abs = std::fabs(v);
    if (abs >= 1e12) {
        v /= 1e12;
        unit = "TB";
    } else if (abs >= 1e9) {
        v /= 1e9;
        unit = "GB";
    } else if (abs >= 1e6) {
        v /= 1e6;
        unit = "MB";
    } else if (abs >= 1e3) {
        v /= 1e3;
        unit = "KB";
    }
    return wxString::Format("%.1f %s", v, unit);
}

double ChartPanel::PointDisplayValue(const SamplePoint &p) const {
    if (!is_net_chart()) {
        return p.value;
    }
    switch (net_unit_) {
    case NetDisplayUnit::Packets:
        return p.aux;
    case NetDisplayUnit::PayloadSize: {
        /* Approximate L2 payload: wire bytes minus Ethernet header per packet. */
        constexpr double kEthHdr = 14.0;
        return std::max(0.0, p.value - p.aux * kEthHdr);
    }
    case NetDisplayUnit::RawSize:
    default:
        return p.value;
    }
}

wxString ChartPanel::FormatTimeLabel(int64_t t_ms) {
    if (t_ms < 0) {
        t_ms = 0;
    }
    int64_t total_s = t_ms / 1000;
    int64_t h = total_s / 3600;
    int64_t m = (total_s % 3600) / 60;
    int64_t s = total_s % 60;
    if (h > 0) {
        return wxString::Format("%lld:%02lld:%02lld", static_cast<long long>(h),
                                static_cast<long long>(m), static_cast<long long>(s));
    }
    return wxString::Format("%lld:%02lld", static_cast<long long>(m), static_cast<long long>(s));
}

void ChartPanel::OnPaint(wxPaintEvent &) {
    wxAutoBufferedPaintDC dc(this);
    wxSize size = GetClientSize();
    dc.SetBackground(wxBrush(wxColour(248, 249, 251)));
    dc.Clear();
    legend_hits_.clear();

    const int pad_l = 56;
    const int pad_r = 10;
    const int pad_t = 10;
    const int title_h = 22;
    const int time_h = show_time_axis_ ? 16 : 0;
    const int pad_b = 8 + time_h;

    wxRect plot(pad_l, pad_t, size.x - pad_l - pad_r, size.y - pad_t - pad_b - title_h);
    plot_rect_ = plot;
    if (plot.width < 40 || plot.height < 40) {
        return;
    }

    dc.SetPen(wxPen(wxColour(220, 224, 230)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(plot);

    double ymin = 0;
    double ymax = 1;
    int64_t tmin = 0, tmax = 1;
    if (view_) {
        tmin = view_->view_start();
        tmax = view_->view_end;
        if (tmax <= tmin) {
            tmax = tmin + std::max<int64_t>(view_->view_span, 1);
        }
    }
    bool any = false;

    std::vector<DrawnSeries> drawn;
    drawn.reserve(spec_.series.size());

    /* For process I/O / counters, color by metric kind; else by series order. */
    auto color_index_for = [&](const Series &s, size_t idx) -> size_t {
        if (spec_.type == ChartType::ProcessIo) {
            return s.key.kind == SeriesKind::PidRead ? 0 : 1;
        }
        if (spec_.type == ChartType::Threads) {
            if (s.key.kind == SeriesKind::ThreadsAlive) {
                return 0;
            }
            if (s.key.kind == SeriesKind::ThreadsWait) {
                return 1;
            }
            return 2;
        }
        if (spec_.type == ChartType::Connections) {
            if (s.key.kind == SeriesKind::ConnAlive) {
                return 0;
            }
            if (s.key.kind == SeriesKind::ConnWait) {
                return 1;
            }
            return 2;
        }
        if (spec_.type == ChartType::ProcessCpu || spec_.type == ChartType::NumFd) {
            return s.style_index;
        }
        return idx;
    };

    auto in_view = [&](int64_t t) -> bool { return t >= tmin && t <= tmax; };

    size_t idx = 0;
    for (const auto &key : spec_.series) {
        auto it = collector_->series().find(key);
        if (it == collector_->series().end()) {
            ++idx;
            continue;
        }
        const Series &s = it->second;
        bool en = series_enabled(key) && process_enabled(s.pid);
        size_t ci = color_index_for(s, idx);
        drawn.push_back(DrawnSeries{&s, SeriesColor(ci, en), ci, s.style_index, en});
        if (en && show_as_ != ChartShowAs::StackedBars) {
            for (const auto &p : s.points) {
                if (!in_view(p.t_ms)) {
                    continue;
                }
                double v = PointDisplayValue(p);
                if (y_log_ && v <= 0) {
                    continue;
                }
                if (!any) {
                    ymin = ymax = v;
                    any = true;
                } else {
                    ymin = std::min(ymin, v);
                    ymax = std::max(ymax, v);
                }
            }
        } else if (en) {
            for (const auto &p : s.points) {
                if (in_view(p.t_ms)) {
                    any = true;
                    break;
                }
            }
        }
        ++idx;
    }

    /* Stacked bar Y max = sum of enabled series at each visible sample time. */
    if (show_as_ == ChartShowAs::StackedBars) {
        ymin = 0;
        ymax = 1;
        std::vector<int64_t> times;
        for (const auto &d : drawn) {
            if (!d.enabled) {
                continue;
            }
            for (const auto &p : d.series->points) {
                if (in_view(p.t_ms)) {
                    times.push_back(p.t_ms);
                }
            }
        }
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());
        for (int64_t t : times) {
            double sum = 0;
            for (const auto &d : drawn) {
                if (!d.enabled) {
                    continue;
                }
                for (const auto &p : d.series->points) {
                    if (p.t_ms == t) {
                        sum += std::max(0.0, PointDisplayValue(p));
                        break;
                    }
                }
            }
            ymax = std::max(ymax, sum);
            any = any || sum > 0 || !times.empty();
        }
    }

    if (!y_log_ && ymin > 0) {
        ymin = 0;
    }
    if (y_log_) {
        if (ymin <= 0) {
            ymin = 1;
        }
        if (ymax <= ymin) {
            ymax = ymin * 10;
        }
    } else if (ymax <= ymin) {
        ymax = ymin + 1;
    } else {
        ymax *= 1.08;
    }
    if (tmax <= tmin) {
        tmax = tmin + 1;
    }

    auto y_to_frac = [&](double v) -> double {
        if (y_log_) {
            double a = std::log10(std::max(v, ymin));
            double lo = std::log10(ymin);
            double hi = std::log10(ymax);
            return (a - lo) / (hi - lo);
        }
        return (v - ymin) / (ymax - ymin);
    };

    dc.SetFont(wxFontInfo(8).Family(wxFONTFAMILY_TELETYPE));
    dc.SetTextForeground(wxColour(110, 118, 130));
    for (int i = 0; i <= 4; ++i) {
        double frac = i / 4.0;
        int y = plot.y + plot.height - static_cast<int>(frac * plot.height);
        dc.SetPen(wxPen(wxColour(232, 236, 241), 1, wxPENSTYLE_DOT));
        dc.DrawLine(plot.x, y, plot.x + plot.width, y);
        double val;
        if (y_log_) {
            double lo = std::log10(ymin);
            double hi = std::log10(ymax);
            val = std::pow(10.0, lo + (hi - lo) * frac);
        } else {
            val = ymin + (ymax - ymin) * frac;
        }
        wxString label = FormatValue(val);
        wxSize ts = dc.GetTextExtent(label);
        dc.DrawText(label, plot.x - ts.x - 6, y - ts.y / 2);
    }

    auto map_x = [&](int64_t t) -> int {
        double f = static_cast<double>(t - tmin) / static_cast<double>(tmax - tmin);
        return plot.x + static_cast<int>(f * (plot.width - 1));
    };
    auto map_y = [&](double v) -> int {
        double f = std::clamp(y_to_frac(v), 0.0, 1.0);
        return plot.y + plot.height - 1 - static_cast<int>(f * (plot.height - 1));
    };

    DrawSeries(dc, drawn, plot, tmin, tmax, ymin, ymax, map_x, map_y, y_log_ ? ymin : 0.0);

    if (show_time_axis_) {
        dc.SetFont(wxFontInfo(8).Family(wxFONTFAMILY_TELETYPE));
        dc.SetTextForeground(wxColour(110, 118, 130));
        const int ticks = 5;
        for (int i = 0; i < ticks; ++i) {
            double frac = static_cast<double>(i) / static_cast<double>(ticks - 1);
            int64_t t = tmin + static_cast<int64_t>(frac * (tmax - tmin));
            int x = map_x(t);
            dc.SetPen(wxPen(wxColour(210, 214, 220)));
            dc.DrawLine(x, plot.y + plot.height, x, plot.y + plot.height + 3);
            wxString lab = FormatTimeLabel(t);
            wxSize ts = dc.GetTextExtent(lab);
            int tx = x - ts.x / 2;
            if (i == 0) {
                tx = x;
            } else if (i == ticks - 1) {
                tx = x - ts.x;
            }
            dc.DrawText(lab, tx, plot.y + plot.height + 4);
        }
    }

    /* Title centered below the plot (under time labels if any) */
    dc.SetFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS).Bold());
    dc.SetTextForeground(wxColour(40, 44, 52));
    wxString title = wxString::FromUTF8(spec_.title.c_str());
    if (y_log_) {
        title += "  [log]";
    }
    if (is_net_chart()) {
        switch (net_unit_) {
        case NetDisplayUnit::PayloadSize:
            title += "  [payload]";
            break;
        case NetDisplayUnit::Packets:
            title += "  [packets]";
            break;
        default:
            break;
        }
    }
    wxSize ts = dc.GetTextExtent(title);
    int title_y = plot.y + plot.height + 4 + time_h;
    dc.DrawText(title, plot.x + (plot.width - ts.x) / 2, title_y);

    if (!show_legends_) {
        if (!any) {
            dc.SetTextForeground(wxColour(140, 146, 156));
            dc.SetFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS).Italic());
            wxString msg = tr("waiting for samples…");
            wxSize mts = dc.GetTextExtent(msg);
            dc.DrawText(msg, plot.x + (plot.width - mts.x) / 2, plot.y + (plot.height - mts.y) / 2);
        }
        return;
    }

    dc.SetFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS));

    /* --- Metric / series legend (top-right) --- */
    if (!is_process_chart() || spec_.type == ChartType::ProcessIo ||
        spec_.type == ChartType::Threads || spec_.type == ChartType::Connections) {
        std::vector<DrawnSeries> metric_items;
        if (spec_.type == ChartType::ProcessIo || spec_.type == ChartType::Threads ||
            spec_.type == ChartType::Connections) {
            /* Unique metric kinds only */
            std::unordered_map<int, bool> seen_kind;
            for (const auto &d : drawn) {
                int k = static_cast<int>(d.series->key.kind);
                if (seen_kind[k]) {
                    continue;
                }
                seen_kind[k] = true;
                metric_items.push_back(d);
            }
        } else {
            metric_items = drawn;
        }

        if (!metric_items.empty()) {
            int row_h = 16;
            int box_pad = 6;
            int max_w = 0;
            for (const auto &d : metric_items) {
                wxString lab = (spec_.type == ChartType::ProcessIo ||
                                spec_.type == ChartType::Threads ||
                                spec_.type == ChartType::Connections)
                                   ? wxString(series_kind_name(d.series->key.kind))
                                   : wxString::FromUTF8(d.series->label.c_str());
                max_w = std::max(max_w, dc.GetTextExtent(lab).x);
            }
            int box_w = max_w + 28 + box_pad * 2;
            int box_h = static_cast<int>(metric_items.size()) * row_h + box_pad * 2;
            int box_x = plot.x + plot.width - box_w - 4;
            int box_y = plot.y + 4;

            dc.SetPen(wxPen(WithAlpha(wxColour(180, 186, 196), legend_alpha_)));
            dc.SetBrush(wxBrush(WithAlpha(wxColour(255, 255, 255), legend_alpha_ * 0.92)));
            dc.DrawRoundedRectangle(box_x, box_y, box_w, box_h, 3);

            int ly = box_y + box_pad;
            for (const auto &d : metric_items) {
                wxColour col = WithAlpha(d.color, legend_alpha_);
                wxPen pen(col, 2);
                if (!is_process_chart()) {
                    apply_process_pen_style(pen, 0);
                }
                dc.SetPen(pen);
                dc.DrawLine(box_x + box_pad, ly + 7, box_x + box_pad + 12, ly + 7);

                bool en = series_enabled(d.series->key);
                wxString lab = (spec_.type == ChartType::ProcessIo ||
                                spec_.type == ChartType::Threads ||
                                spec_.type == ChartType::Connections)
                                   ? wxString(series_kind_name(d.series->key.kind))
                                   : wxString::FromUTF8(d.series->label.c_str());
                if (!en) {
                    lab = wxString("(") + lab + ")";
                }
                dc.SetTextForeground(
                    WithAlpha(en ? wxColour(50, 54, 62) : wxColour(150, 154, 160), legend_alpha_));
                dc.DrawText(lab, box_x + box_pad + 16, ly);

                LegendHit hit;
                hit.kind = LegendHit::Kind::Series;
                hit.series_key = d.series->key;
                hit.rect = wxRect(box_x, ly - 1, box_w, row_h);
                legend_hits_.push_back(hit);
                ly += row_h;
            }
        }
    }

    /* --- Process legend (top-left, separate) --- */
    if (is_process_chart() && !collector_->tracked().empty()) {
        const auto &tracked = collector_->tracked();
        int row_h = 16;
        int box_pad = 6;
        int max_w = 0;
        for (const auto &tp : tracked) {
            wxString lab = wxString::Format("%s (%d)", tp.label.c_str(), static_cast<int>(tp.pid));
            max_w = std::max(max_w, dc.GetTextExtent(lab).x);
        }
        int box_w = max_w + 36 + box_pad * 2;
        int box_h = static_cast<int>(tracked.size()) * row_h + box_pad * 2 + 14;
        int box_x = plot.x + 4;
        int box_y = plot.y + 4;

        dc.SetPen(wxPen(WithAlpha(wxColour(180, 186, 196), legend_alpha_)));
        dc.SetBrush(wxBrush(WithAlpha(wxColour(255, 255, 255), legend_alpha_ * 0.92)));
        dc.DrawRoundedRectangle(box_x, box_y, box_w, box_h, 3);

        dc.SetTextForeground(WithAlpha(wxColour(90, 96, 110), legend_alpha_));
        dc.SetFont(wxFontInfo(7).Family(wxFONTFAMILY_SWISS).Bold());
        dc.DrawText(tr("Processes"), box_x + box_pad, box_y + 2);
        dc.SetFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS));

        int ly = box_y + box_pad + 12;
        for (const auto &tp : tracked) {
            bool en = process_enabled(tp.pid);
            wxColour col = WithAlpha(SeriesColor(tp.style_index, en), legend_alpha_);
            wxPen pen(col, 2);
            apply_process_pen_style(pen, tp.style_index);
            dc.SetPen(pen);
            dc.DrawLine(box_x + box_pad, ly + 7, box_x + box_pad + 18, ly + 7);

            wxString lab = wxString::Format("%s (%d)", tp.label.c_str(), static_cast<int>(tp.pid));
            if (!en) {
                lab = wxString("(") + lab + ")";
            }
            dc.SetTextForeground(
                WithAlpha(en ? wxColour(50, 54, 62) : wxColour(150, 154, 160), legend_alpha_));
            dc.DrawText(lab, box_x + box_pad + 22, ly);

            LegendHit hit;
            hit.kind = LegendHit::Kind::Process;
            hit.pid = tp.pid;
            hit.rect = wxRect(box_x, ly - 1, box_w, row_h);
            legend_hits_.push_back(hit);
            ly += row_h;
        }
    }

    if (!any) {
        dc.SetTextForeground(wxColour(140, 146, 156));
        dc.SetFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS).Italic());
        wxString msg = tr("waiting for samples…");
        wxSize mts = dc.GetTextExtent(msg);
        dc.DrawText(msg, plot.x + (plot.width - mts.x) / 2, plot.y + (plot.height - mts.y) / 2);
    }
}

void ChartPanel::DrawSeries(wxDC &dc, const std::vector<DrawnSeries> &drawn, const wxRect &plot,
                            int64_t tmin, int64_t tmax, double ymin, double ymax,
                            const std::function<int(int64_t)> &map_x,
                            const std::function<int(double)> &map_y, double y_floor) {
    (void)ymin;
    (void)ymax;

    auto sample_value = [&](const Series &s, size_t i) -> double {
        double v = PointDisplayValue(s.points[i]);
        if (y_log_) {
            v = std::max(v, y_floor);
        }
        return v;
    };

    if (show_as_ == ChartShowAs::StackedBars || show_as_ == ChartShowAs::Bars) {
        /* Collect visible sample times across enabled series. */
        std::vector<int64_t> times;
        size_t enabled_n = 0;
        for (const auto &d : drawn) {
            if (!d.enabled) {
                continue;
            }
            ++enabled_n;
            for (const auto &p : d.series->points) {
                if (p.t_ms >= tmin && p.t_ms <= tmax) {
                    times.push_back(p.t_ms);
                }
            }
        }
        if (times.empty() || enabled_n == 0) {
            return;
        }
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());

        double slot = static_cast<double>(plot.width) / static_cast<double>(std::max<size_t>(times.size(), 1));
        int bar_w = std::max(1, static_cast<int>(slot) - 1);

        for (size_t i = 0; i < times.size(); ++i) {
            int64_t t = times[i];
            int x0 = map_x(t) - bar_w / 2;
            if (x0 < plot.x) {
                x0 = plot.x;
            }
            int x1 = x0 + bar_w;

            if (show_as_ == ChartShowAs::StackedBars) {
                double acc = 0;
                for (const auto &d : drawn) {
                    if (!d.enabled) {
                        continue;
                    }
                    double v = 0;
                    bool found = false;
                    for (size_t j = 0; j < d.series->points.size(); ++j) {
                        if (d.series->points[j].t_ms == t) {
                            v = std::max(0.0, sample_value(*d.series, j));
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        continue;
                    }
                    double bottom = acc;
                    acc += v;
                    int y_top = map_y(acc);
                    int y_bot = map_y(bottom);
                    if (y_bot < y_top) {
                        std::swap(y_bot, y_top);
                    }
                    dc.SetPen(*wxTRANSPARENT_PEN);
                    dc.SetBrush(wxBrush(d.color));
                    dc.DrawRectangle(x0, y_top, x1 - x0, std::max(1, y_bot - y_top));
                }
            } else {
                int inner = x1 - x0;
                int bw = std::max(1, inner / static_cast<int>(enabled_n));
                int bi = 0;
                for (const auto &d : drawn) {
                    if (!d.enabled) {
                        continue;
                    }
                    double v = 0;
                    bool found = false;
                    for (size_t j = 0; j < d.series->points.size(); ++j) {
                        if (d.series->points[j].t_ms == t) {
                            v = sample_value(*d.series, j);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        ++bi;
                        continue;
                    }
                    int bx = x0 + bi * bw;
                    int y_top = map_y(v);
                    int y_bot = map_y(y_log_ ? y_floor : 0.0);
                    if (y_bot < y_top) {
                        std::swap(y_bot, y_top);
                    }
                    dc.SetPen(*wxTRANSPARENT_PEN);
                    dc.SetBrush(wxBrush(d.color));
                    dc.DrawRectangle(bx, y_top, std::max(1, bw - 1), std::max(1, y_bot - y_top));
                    ++bi;
                }
            }
        }
        return;
    }

    for (const auto &d : drawn) {
        if (!d.enabled) {
            continue;
        }
        const auto &pts = d.series->points;
        wxPen pen(d.color, 2);
        apply_process_pen_style(pen, is_process_chart() ? d.style_index : 0);
        dc.SetPen(pen);
        dc.SetBrush(wxBrush(d.color));

        auto y_at = [&](size_t i) -> double { return sample_value(*d.series, i); };

        if (show_as_ == ChartShowAs::Dots || pts.size() < 2) {
            for (size_t i = 0; i < pts.size(); ++i) {
                if (pts[i].t_ms < tmin || pts[i].t_ms > tmax) {
                    continue;
                }
                dc.DrawCircle(map_x(pts[i].t_ms), map_y(y_at(i)), 3);
            }
            continue;
        }

        auto segment_visible = [&](size_t i) -> bool {
            if (i == 0 || i >= pts.size()) {
                return false;
            }
            if (pts[i].t_ms < tmin && pts[i - 1].t_ms < tmin) {
                return false;
            }
            if (pts[i].t_ms > tmax && pts[i - 1].t_ms > tmax) {
                return false;
            }
            return true;
        };

        if (curve_style_ == CurveStyle::Segment) {
            for (size_t i = 1; i < pts.size(); ++i) {
                if (!segment_visible(i)) {
                    continue;
                }
                dc.DrawLine(map_x(pts[i - 1].t_ms), map_y(y_at(i - 1)), map_x(pts[i].t_ms),
                            map_y(y_at(i)));
            }
            continue;
        }

        if (curve_style_ == CurveStyle::Bezier) {
            /* Midpoint quadratic Bezier chain (passes near samples). */
            auto draw_quad = [&](double t0, double v0, double tc, double vc, double t1, double v1) {
                int steps = std::max(
                    4, static_cast<int>(std::abs(map_x(static_cast<int64_t>(t1)) -
                                                 map_x(static_cast<int64_t>(t0))) /
                                        4));
                int px = map_x(static_cast<int64_t>(t0 + 0.5));
                int py = map_y(v0);
                for (int s = 1; s <= steps; ++s) {
                    double u = static_cast<double>(s) / static_cast<double>(steps);
                    double omu = 1.0 - u;
                    double tt = omu * omu * t0 + 2 * omu * u * tc + u * u * t1;
                    double vv = omu * omu * v0 + 2 * omu * u * vc + u * u * v1;
                    int x = map_x(static_cast<int64_t>(tt + 0.5));
                    int y = map_y(vv);
                    dc.DrawLine(px, py, x, y);
                    px = x;
                    py = y;
                }
            };
            if (pts.size() == 2) {
                if (segment_visible(1)) {
                    draw_quad(static_cast<double>(pts[0].t_ms), y_at(0),
                              (pts[0].t_ms + pts[1].t_ms) * 0.5, (y_at(0) + y_at(1)) * 0.5,
                              static_cast<double>(pts[1].t_ms), y_at(1));
                }
            } else {
                for (size_t i = 1; i + 1 < pts.size(); ++i) {
                    double t0 = (static_cast<double>(pts[i - 1].t_ms) + pts[i].t_ms) * 0.5;
                    double v0 = (y_at(i - 1) + y_at(i)) * 0.5;
                    double t1 = (static_cast<double>(pts[i].t_ms) + pts[i + 1].t_ms) * 0.5;
                    double v1 = (y_at(i) + y_at(i + 1)) * 0.5;
                    if (t1 < tmin && t0 < tmin) {
                        continue;
                    }
                    if (t0 > tmax && t1 > tmax) {
                        continue;
                    }
                    draw_quad(t0, v0, static_cast<double>(pts[i].t_ms), y_at(i), t1, v1);
                }
                /* Cap ends with straight segments to first/last sample. */
                if (segment_visible(1)) {
                    double tm = (static_cast<double>(pts[0].t_ms) + pts[1].t_ms) * 0.5;
                    double vm = (y_at(0) + y_at(1)) * 0.5;
                    dc.DrawLine(map_x(pts[0].t_ms), map_y(y_at(0)), map_x(static_cast<int64_t>(tm)),
                                map_y(vm));
                }
                size_t last = pts.size() - 1;
                if (segment_visible(last)) {
                    double tm =
                        (static_cast<double>(pts[last - 1].t_ms) + pts[last].t_ms) * 0.5;
                    double vm = (y_at(last - 1) + y_at(last)) * 0.5;
                    dc.DrawLine(map_x(static_cast<int64_t>(tm)), map_y(vm), map_x(pts[last].t_ms),
                                map_y(y_at(last)));
                }
            }
            continue;
        }

        /* Bicubic: Catmull-Rom spline densified between samples. */
        auto catmull = [](double p0, double p1, double p2, double p3, double t) -> double {
            double t2 = t * t;
            double t3 = t2 * t;
            return 0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                          (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
        };
        for (size_t i = 1; i < pts.size(); ++i) {
            if (!segment_visible(i)) {
                continue;
            }
            size_t i0 = i >= 2 ? i - 2 : 0;
            size_t i1 = i - 1;
            size_t i2 = i;
            size_t i3 = (i + 1 < pts.size()) ? i + 1 : pts.size() - 1;
            int steps = std::max(6, static_cast<int>(std::abs(map_x(pts[i2].t_ms) -
                                                              map_x(pts[i1].t_ms)) /
                                                     3));
            int px = map_x(pts[i1].t_ms);
            int py = map_y(y_at(i1));
            for (int s = 1; s <= steps; ++s) {
                double u = static_cast<double>(s) / static_cast<double>(steps);
                double tt = catmull(static_cast<double>(pts[i0].t_ms),
                                    static_cast<double>(pts[i1].t_ms),
                                    static_cast<double>(pts[i2].t_ms),
                                    static_cast<double>(pts[i3].t_ms), u);
                double vv = catmull(y_at(i0), y_at(i1), y_at(i2), y_at(i3), u);
                int x = map_x(static_cast<int64_t>(tt + 0.5));
                int y = map_y(vv);
                dc.DrawLine(px, py, x, y);
                px = x;
                py = y;
            }
        }
    }
}
