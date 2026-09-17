/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "main_frame.hpp"
#include "config.h"
#include "dialogs.hpp"
#include "view_state.hpp"
#include "wx_tr.hpp"

#include <bas/log/uselog.h>

#include <wx/aboutdlg.h>
#include <wx/artprov.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/statusbr.h>

#include <chrono>
#include <sstream>

static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static void set_item_bitmap(wxMenuItem *item, const wxArtID &id) {
    if (!item) {
        return;
    }
    wxBitmap bmp = wxArtProvider::GetBitmap(id, wxART_MENU);
    if (bmp.IsOk()) {
        item->SetBitmap(bmp);
    }
}

MainFrame::MainFrame(const Options &opt)
    : wxFrame(nullptr, wxID_ANY, "pidload", wxDefaultPosition, wxSize(1100, 720)),
      opt_(opt) {
    state_path_ = view_state_path_for_names(opt_.names);
    ViewState loaded;
    if (load_view_state(state_path_, loaded)) {
        apply_view_state(opt_, loaded);
        aui_perspective_ = loaded.aui_perspective;
        if (loaded.frame_w > 200 && loaded.frame_h > 150) {
            SetSize(loaded.frame_w, loaded.frame_h);
        }
        if (loaded.have_frame_pos) {
            SetPosition(wxPoint(loaded.frame_x, loaded.frame_y));
        }
        std::string material = view_state_key_material(opt_.names);
        loginfo_fmt("restored view state key=%s file=%s",
                    utf8sha1_hex(material).c_str(), state_path_.c_str());
    }

    collector_ = std::make_unique<Collector>(opt_);
    recorder_ = std::make_unique<Recorder>(opt_);
    if (!recorder_->ok()) {
        wxMessageBox(tr("Failed to create output directory."), "pidload", wxOK | wxICON_ERROR);
    }

    time_view_.set_capture_window(opt_.window_ms);

    aui_.SetManagedWindow(this);
    SetMinSize(wxSize(640, 400));
    CreateStatusBar(1);
    BuildMenu();
    RebuildPanes();
    SyncViewMenu();

    start_ms_ = now_ms();
    collector_->tick(0);
    timer_.SetOwner(this);
    Bind(wxEVT_TIMER, &MainFrame::OnTimer, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    timer_.Start(static_cast<int>(opt_.interval_ms));
    SetStatusText(wxString::Format(
        tr("interval %lld ms  |  view %lld ms  |  drag to pan, wheel to zoom  |  drag panes to "
           "rearrange"),
        static_cast<long long>(opt_.interval_ms), static_cast<long long>(opt_.window_ms)));
}

MainFrame::~MainFrame() {
    aui_.UnInit();
}

void MainFrame::BuildMenu() {
    auto *file = new wxMenu;
    wxMenuItem *open =
        file->Append(ID_OpenProcess, tr("Open...\tCtrl+O"), tr("Select a process from the list"));
    set_item_bitmap(open, wxART_LIST_VIEW);
    wxMenuItem *open_pid =
        file->Append(ID_OpenPid, tr("Open PID...\tCtrl+P"), tr("Monitor a process by PID"));
    set_item_bitmap(open_pid, wxART_EXECUTABLE_FILE);
    file->AppendSeparator();
    wxMenuItem *quit = file->Append(wxID_EXIT, tr("Quit\tCtrl+Q"));
    set_item_bitmap(quit, wxART_QUIT);

    auto *edit = new wxMenu;
    wxMenuItem *clear =
        edit->Append(ID_ClearHistory, tr("Clear History\tCtrl+L"), tr("Clear all chart history"));
    set_item_bitmap(clear, wxART_DELETE);
    wxMenuItem *copy =
        edit->Append(ID_CopySnapshot, tr("Copy Snapshot\tCtrl+C"), tr("Copy latest sample values"));
    set_item_bitmap(copy, wxART_COPY);
    edit->AppendSeparator();
    wxMenuItem *reset_layout = edit->Append(ID_ResetLayout, tr("Reset Layout\tCtrl+Shift+R"),
                                            tr("Restore balanced auto layout"));
    set_item_bitmap(reset_layout, wxART_REDO);

    auto *view = new wxMenu;
    view->AppendCheckItem(ID_ViewCpu, tr("CPU\tCtrl+Shift+C"),
                          tr("System CPU overall and per-core"));
    view->AppendCheckItem(ID_ViewMemory, tr("Memory\tCtrl+Shift+M"),
                          tr("System memory and swap"));
    view->AppendCheckItem(ID_ViewNetwork, tr("Network\tCtrl+Shift+N"),
                          tr("Network interface traffic"));
    view->AppendCheckItem(ID_ViewThreads, tr("Threads\tCtrl+Shift+T"),
                          tr("Thread counts (alive / wait / total)"));
    view->AppendCheckItem(ID_ViewNumFd, tr("File Descriptors\tCtrl+Shift+F"),
                          tr("Open file-descriptor counts"));
    view->AppendCheckItem(ID_ViewConnections, tr("Connections\tCtrl+Shift+K"),
                          tr("Net connections (alive / wait / total)"));
    view->AppendSeparator();
    wxMenuItem *add = view->Append(ID_AddCapture, tr("Add...\tCtrl+Shift+A"),
                                   tr("Add a capture (device/iface/addr/name)"));
    set_item_bitmap(add, wxART_PLUS);
    wxMenuItem *rem = view->Append(ID_RemoveCapture, tr("Remove\tDelete"),
                                   tr("Remove the focused chart capture"));
    set_item_bitmap(rem, wxART_MINUS);
    view->AppendSeparator();
    view->AppendCheckItem(ID_ViewLegends, tr("Legends\tCtrl+Shift+L"), tr("Show chart legends"));
    view->AppendCheckItem(ID_ViewYLog, tr("Y-Log\tCtrl+Shift+Y"), tr("Logarithmic Y axis"));

    auto *show_as = new wxMenu;
    show_as->AppendRadioItem(ID_ShowDots, tr("Dots\tAlt+1"));
    show_as->AppendRadioItem(ID_ShowCurve, tr("Curve\tAlt+2"));
    show_as->AppendRadioItem(ID_ShowBars, tr("Bars (siblings)\tAlt+3"));
    show_as->AppendRadioItem(ID_ShowStacked, tr("Stacked Bars\tAlt+4"));
    view->AppendSubMenu(show_as, tr("Show as"));

    auto *curve = new wxMenu;
    curve->AppendRadioItem(ID_CurveSeg, tr("Segment\tAlt+Shift+1"));
    curve->AppendRadioItem(ID_CurveBezier, tr("Bezier\tAlt+Shift+2"));
    curve->AppendRadioItem(ID_CurveCubic, tr("Bicubic\tAlt+Shift+3"));
    view->AppendSubMenu(curve, tr("Display Curve"));

    view->AppendSeparator();
    auto *interval = new wxMenu;
    interval->AppendRadioItem(ID_Interval1s, tr("1 second\tCtrl+1"));
    interval->AppendRadioItem(ID_Interval2s, tr("2 seconds\tCtrl+2"));
    interval->AppendRadioItem(ID_Interval5s, tr("5 seconds\tCtrl+5"));
    view->AppendSubMenu(interval, tr("Refresh Interval"));
    view->AppendSeparator();
    wxMenuItem *reset_view =
        view->Append(ID_ResetView, tr("Reset Live View\tHome"), tr("Follow the live edge again"));
    set_item_bitmap(reset_view, wxART_GO_FORWARD);

    auto *help = new wxMenu;
    wxMenuItem *about = help->Append(ID_About, tr("About pidload\tF1"));
    set_item_bitmap(about, wxART_INFORMATION);

    auto *bar = new wxMenuBar;
    bar->Append(file, tr("&File"));
    bar->Append(edit, tr("&Edit"));
    bar->Append(view, tr("&View"));
    bar->Append(help, tr("&Help"));
    SetMenuBar(bar);

    Bind(wxEVT_MENU, &MainFrame::OnOpenProcess, this, ID_OpenProcess);
    Bind(wxEVT_MENU, &MainFrame::OnOpenPid, this, ID_OpenPid);
    Bind(wxEVT_MENU, &MainFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnClearHistory, this, ID_ClearHistory);
    Bind(wxEVT_MENU, &MainFrame::OnCopySnapshot, this, ID_CopySnapshot);
    Bind(wxEVT_MENU, &MainFrame::OnToggleCpu, this, ID_ViewCpu);
    Bind(wxEVT_MENU, &MainFrame::OnToggleMemory, this, ID_ViewMemory);
    Bind(wxEVT_MENU, &MainFrame::OnToggleNetwork, this, ID_ViewNetwork);
    Bind(wxEVT_MENU, &MainFrame::OnToggleThreads, this, ID_ViewThreads);
    Bind(wxEVT_MENU, &MainFrame::OnToggleNumFd, this, ID_ViewNumFd);
    Bind(wxEVT_MENU, &MainFrame::OnToggleConnections, this, ID_ViewConnections);
    Bind(wxEVT_MENU, &MainFrame::OnAddCapture, this, ID_AddCapture);
    Bind(wxEVT_MENU, &MainFrame::OnRemoveCapture, this, ID_RemoveCapture);
    Bind(wxEVT_MENU, &MainFrame::OnToggleLegends, this, ID_ViewLegends);
    Bind(wxEVT_MENU, &MainFrame::OnToggleYLog, this, ID_ViewYLog);
    Bind(wxEVT_MENU, &MainFrame::OnShowAs, this, ID_ShowDots);
    Bind(wxEVT_MENU, &MainFrame::OnShowAs, this, ID_ShowCurve);
    Bind(wxEVT_MENU, &MainFrame::OnShowAs, this, ID_ShowBars);
    Bind(wxEVT_MENU, &MainFrame::OnShowAs, this, ID_ShowStacked);
    Bind(wxEVT_MENU, &MainFrame::OnCurveStyle, this, ID_CurveSeg);
    Bind(wxEVT_MENU, &MainFrame::OnCurveStyle, this, ID_CurveBezier);
    Bind(wxEVT_MENU, &MainFrame::OnCurveStyle, this, ID_CurveCubic);
    Bind(wxEVT_MENU, &MainFrame::OnInterval, this, ID_Interval1s);
    Bind(wxEVT_MENU, &MainFrame::OnInterval, this, ID_Interval2s);
    Bind(wxEVT_MENU, &MainFrame::OnInterval, this, ID_Interval5s);
    Bind(wxEVT_MENU, &MainFrame::OnResetLayout, this, ID_ResetLayout);
    Bind(wxEVT_MENU, &MainFrame::OnResetView, this, ID_ResetView);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, ID_About);
}

void MainFrame::SyncViewMenu() {
    wxMenuBar *bar = GetMenuBar();
    if (!bar) {
        return;
    }
    bar->Check(ID_ViewCpu, collector_->options().show_cpu);
    bar->Check(ID_ViewMemory, collector_->options().show_memory);
    bar->Check(ID_ViewNetwork, collector_->options().monitor_network);
    bar->Check(ID_ViewThreads, collector_->options().show_threads);
    bar->Check(ID_ViewNumFd, collector_->options().show_numfd);
    bar->Check(ID_ViewConnections, collector_->options().show_connections);
    bar->Check(ID_ViewLegends, opt_.show_legends);
    bar->Check(ID_ViewYLog, opt_.y_log);
    switch (opt_.show_as) {
    case ChartShowAs::Dots:
        bar->Check(ID_ShowDots, true);
        break;
    case ChartShowAs::Curve:
        bar->Check(ID_ShowCurve, true);
        break;
    case ChartShowAs::Bars:
        bar->Check(ID_ShowBars, true);
        break;
    case ChartShowAs::StackedBars:
        bar->Check(ID_ShowStacked, true);
        break;
    }
    switch (opt_.curve_style) {
    case CurveStyle::Segment:
        bar->Check(ID_CurveSeg, true);
        break;
    case CurveStyle::Bezier:
        bar->Check(ID_CurveBezier, true);
        break;
    case CurveStyle::Bicubic:
        bar->Check(ID_CurveCubic, true);
        break;
    }

    int64_t ms = collector_->options().interval_ms;
    if (ms <= 1000) {
        bar->Check(ID_Interval1s, true);
    } else if (ms <= 2000) {
        bar->Check(ID_Interval2s, true);
    } else {
        bar->Check(ID_Interval5s, true);
    }
}

void MainFrame::RebuildPanes() {
    if (!panels_.empty()) {
        wxString persp = aui_.SavePerspective();
        if (!persp.empty()) {
            aui_perspective_ = persp.ToStdString();
        }
    }

    for (auto &kv : panels_) {
        aui_.DetachPane(kv.second);
        kv.second->Hide();
        kv.second->Destroy();
    }
    panels_.clear();
    aui_.Update();

    const auto &charts = collector_->charts();
    for (const auto &ch : charts) {
        auto *panel = new ChartPanel(this, ch, collector_.get(), &time_view_);
        panel->ApplyViewDefaults(opt_);
        panel->SetViewChangedHandler([this]() { RefreshAllCharts(); });
        panel->Bind(wxEVT_LEFT_DOWN, [this, id = ch.id](wxMouseEvent &e) {
            active_chart_id_ = id;
            e.Skip();
        });
        panel->SetRemoveHandler([this, id = ch.id]() {
            active_chart_id_ = id;
            wxCommandEvent ev(wxEVT_MENU, ID_RemoveCapture);
            AddPendingEvent(ev);
        });
        panels_[ch.id] = panel;
    }

    AssignTimeAxis();
    ApplyDefaultLayout();
    ApplySavedPerspective();
    if (!charts.empty()) {
        active_chart_id_ = charts.front().id;
    } else {
        active_chart_id_.clear();
        SetStatusText(tr("No charts — use View → Add… or enable CPU/Memory/Network/…"));
    }
}

void MainFrame::ApplySavedPerspective() {
    if (aui_perspective_.empty() || panels_.empty()) {
        return;
    }
    aui_.LoadPerspective(wxString::FromUTF8(aui_perspective_.c_str()), true);
    aui_.Update();
}

void MainFrame::AssignTimeAxis() {
    /* Put time labels on one chart (prefer last in collector order). */
    for (auto &kv : panels_) {
        kv.second->SetShowTimeAxis(false);
    }
    const auto &charts = collector_->charts();
    if (charts.empty()) {
        return;
    }
    auto it = panels_.find(charts.back().id);
    if (it != panels_.end()) {
        it->second->SetShowTimeAxis(true);
    }
}

void MainFrame::RefreshAllCharts() {
    for (auto &kv : panels_) {
        kv.second->RefreshData();
    }
    SetStatusText(wxString::Format(
        tr("view %lld–%lld ms (%s)  |  drag to pan, wheel to zoom"),
        static_cast<long long>(time_view_.view_start()),
        static_cast<long long>(time_view_.view_end),
        time_view_.follow_live ? _("live") : _("paused")));
}

void MainFrame::ApplyDefaultLayout() {
    for (auto &kv : panels_) {
        if (aui_.GetPane(kv.second).IsOk()) {
            aui_.DetachPane(kv.second);
        }
    }

    /* Prefer Network in the tall column-span slot. */
    std::vector<std::pair<ChartPanel *, const ChartSpec *>> ordered;
    const ChartSpec *network = nullptr;
    ChartPanel *network_panel = nullptr;
    for (const auto &ch : collector_->charts()) {
        auto it = panels_.find(ch.id);
        if (it == panels_.end()) {
            continue;
        }
        if (ch.type == ChartType::Network && !network) {
            network = &ch;
            network_panel = it->second;
        } else {
            ordered.push_back({it->second, &ch});
        }
    }
    if (network_panel) {
        ordered.insert(ordered.begin(), {network_panel, network});
    }

    const size_t n = ordered.size();
    auto make_pane = [](const ChartSpec &ch) {
        return wxAuiPaneInfo()
            .Name(wxString::FromUTF8(ch.id.c_str()))
            .Caption(wxString::FromUTF8(ch.title.c_str()))
            .CloseButton(false)
            .MaximizeButton(true)
            .PinButton(true)
            .Dockable(true)
            .Floatable(true)
            .Movable(true)
            .PaneBorder(true)
            .MinSize(180, 120);
    };
    auto P = [&](size_t i) -> ChartPanel * { return ordered[i].first; };
    auto C = [&](size_t i) -> const ChartSpec & { return *ordered[i].second; };
    auto add = [&](size_t i, wxAuiPaneInfo info) { aui_.AddPane(P(i), info); };

    if (n == 0) {
        aui_.Update();
        return;
    }

    /*
     * Balanced layouts inspired by count-based splits. Index 0 is preferred
     * for a spanning column when Network (or first chart) should dominate.
     */
    if (n == 1) {
        add(0, make_pane(C(0)).CenterPane());
    } else if (n == 2) {
        /* 1 | 2 */
        add(0, make_pane(C(0)).Left().Layer(1).BestSize(GetClientSize().x / 2, -1));
        add(1, make_pane(C(1)).CenterPane());
    } else if (n == 3) {
        /* column-span 1 | 2 / 3 */
        add(0, make_pane(C(0)).Left().Layer(1).BestSize(GetClientSize().x / 2, -1));
        add(1, make_pane(C(1)).CenterPane());
        add(2, make_pane(C(2)).Bottom().BestSize(-1, GetClientSize().y / 2));
    } else if (n == 4) {
        /* 4 on top span; 1|2 middle; 3 bottom span — with 0 as left column when Network */
        add(0, make_pane(C(0)).Left().Layer(1).BestSize(GetClientSize().x / 3, -1));
        add(1, make_pane(C(1)).CenterPane());
        add(2, make_pane(C(2)).Bottom().Layer(0).Position(0));
        add(3, make_pane(C(3)).Top().Layer(0).BestSize(-1, GetClientSize().y / 4));
    } else if (n == 5) {
        add(0, make_pane(C(0)).Left().Layer(1).BestSize(GetClientSize().x / 3, -1));
        add(1, make_pane(C(1)).CenterPane());
        add(2, make_pane(C(2)).Bottom().Layer(0));
        add(3, make_pane(C(3)).Top().Layer(0));
        add(4, make_pane(C(4)).Right().Layer(1));
    } else if (n == 6) {
        add(0, make_pane(C(0)).Left().Layer(1).BestSize(GetClientSize().x / 3, -1));
        add(1, make_pane(C(1)).CenterPane());
        add(2, make_pane(C(2)).Bottom().Layer(0).Position(0));
        add(3, make_pane(C(3)).Top().Layer(0));
        add(4, make_pane(C(4)).Right().Layer(1).Position(0));
        add(5, make_pane(C(5)).Right().Layer(1).Position(1));
    } else if (n == 7) {
        add(0, make_pane(C(0)).Left().Layer(1).BestSize(GetClientSize().x / 4, -1));
        add(1, make_pane(C(1)).CenterPane());
        add(2, make_pane(C(2)).Bottom().Layer(0));
        add(3, make_pane(C(3)).Top().Layer(0).Position(0));
        add(4, make_pane(C(4)).Top().Layer(0).Position(1));
        add(5, make_pane(C(5)).Right().Layer(1).Position(0));
        add(6, make_pane(C(6)).Right().Layer(1).Position(1));
    } else {
        /* 8+: center core + ring of docks, keep [0] as tall left column. */
        add(0, make_pane(C(0)).Left().Layer(2).BestSize(GetClientSize().x / 4, -1));
        add(1, make_pane(C(1)).CenterPane());
        for (size_t i = 2; i < n; ++i) {
            auto info = make_pane(C(i));
            size_t k = i - 2;
            if (k % 3 == 0) {
                info.Top().Layer(1).Position(static_cast<int>(k / 3));
            } else if (k % 3 == 1) {
                info.Right().Layer(1).Position(static_cast<int>(k / 3));
            } else {
                info.Bottom().Layer(1).Position(static_cast<int>(k / 3));
            }
            aui_.AddPane(P(i), info);
        }
    }
    aui_.Update();
}

ChartPanel *MainFrame::FocusedPanel() {
    auto it = panels_.find(active_chart_id_);
    if (it != panels_.end()) {
        return it->second;
    }
    if (!panels_.empty()) {
        return panels_.begin()->second;
    }
    return nullptr;
}

void MainFrame::SetIntervalMs(int64_t ms) {
    collector_->set_interval_ms(ms);
    opt_.interval_ms = ms;
    timer_.Start(static_cast<int>(ms));
    SetStatusText(wxString::Format(tr("interval %lld ms"), static_cast<long long>(ms)));
    SyncViewMenu();
}

void MainFrame::OnTimer(wxTimerEvent &) {
    int64_t t = now_ms() - start_ms_;
    bool changed = collector_->tick(t);
    time_view_.on_sample(t);
    recorder_->write_sample(*collector_);
    if (changed) {
        RebuildPanes();
        SyncViewMenu();
        return;
    }
    RefreshAllCharts();
}

void MainFrame::PersistViewState() {
    if (state_path_.empty()) {
        return;
    }
    ViewState st;
    capture_view_state_from_options(collector_ ? collector_->options() : opt_, st);

    /* Prefer live panel values (context menu may not sync Options). */
    if (!panels_.empty()) {
        auto *p = panels_.begin()->second;
        st.show_legends = p->show_legends();
        st.y_log = p->y_log();
        st.show_as = p->show_as();
        st.curve_style = p->curve_style();
        opt_.show_legends = st.show_legends;
        opt_.y_log = st.y_log;
        opt_.show_as = st.show_as;
        opt_.curve_style = st.curve_style;
        for (auto &kv : panels_) {
            if (kv.second->spec().type == ChartType::Network) {
                st.net_unit = kv.second->net_unit();
                opt_.net_unit = st.net_unit;
                break;
            }
        }
    } else {
        st.show_legends = opt_.show_legends;
        st.y_log = opt_.y_log;
        st.show_as = opt_.show_as;
        st.curve_style = opt_.curve_style;
        st.net_unit = opt_.net_unit;
    }

    wxSize sz = GetSize();
    st.frame_w = sz.GetWidth();
    st.frame_h = sz.GetHeight();
    wxPoint pos = GetPosition();
    st.frame_x = pos.x;
    st.frame_y = pos.y;
    st.have_frame_pos = true;
    wxString persp = aui_.SavePerspective();
    if (!persp.empty()) {
        st.aui_perspective = persp.ToStdString();
        aui_perspective_ = st.aui_perspective;
    } else {
        st.aui_perspective = aui_perspective_;
    }
    save_view_state(state_path_, st);
}

void MainFrame::OnClose(wxCloseEvent &event) {
    timer_.Stop();
    PersistViewState();
    event.Skip();
}

void MainFrame::OnQuit(wxCommandEvent &) {
    Close(true);
}

void MainFrame::OnOpenProcess(wxCommandEvent &) {
    auto pids = ShowProcessPicker(this);
    bool changed = false;
    for (pid_t pid : pids) {
        changed = collector_->add_name(std::to_string(static_cast<int>(pid))) || changed;
    }
    if (changed) {
        RebuildPanes();
        SyncViewMenu();
    }
}

void MainFrame::OnOpenPid(wxCommandEvent &) {
    pid_t pid = ShowOpenPidDialog(this);
    if (pid > 0 && collector_->add_name(std::to_string(static_cast<int>(pid)))) {
        RebuildPanes();
        SyncViewMenu();
    }
}

void MainFrame::OnClearHistory(wxCommandEvent &) {
    collector_->clear_history();
    time_view_ = TimeView{};
    time_view_.set_capture_window(opt_.window_ms);
    start_ms_ = now_ms();
    collector_->tick(0);
    RefreshAllCharts();
}

void MainFrame::OnCopySnapshot(wxCommandEvent &) {
    std::ostringstream ss;
    if (collector_->options().show_cpu) {
        ss << "cpu overall=" << collector_->last_cpu().overall_pct << "%\n";
    }
    if (collector_->options().show_memory) {
        const auto &m = collector_->last_mem();
        ss << "mem used_kb=" << m.used_kb << " avail_kb=" << m.avail_kb
           << " swap_used_kb=" << m.swap_used_kb << "\n";
    }
    for (const auto &d : collector_->last_ifaces()) {
        ss << d.name << " in=" << d.inbound << " out=" << d.outbound << " drop=" << d.dropped
           << "\n";
    }
    for (const auto &d : collector_->last_devs()) {
        ss << d.name << " read=" << d.read_bytes << " write=" << d.write_bytes << "\n";
    }
    for (const auto &d : collector_->last_pids()) {
        ss << "pid " << d.pid << " cpu=" << d.cpu_pct << "% read=" << d.read_bytes
           << " write=" << d.write_bytes << "\n";
    }
    for (const auto &d : collector_->last_threads()) {
        ss << "threads " << d.label << " alive=" << d.alive << " wait=" << d.wait
           << " total=" << d.total_opened << "\n";
    }
    for (const auto &d : collector_->last_fds()) {
        ss << "fds " << d.label << " open=" << d.open_fds << "\n";
    }
    for (const auto &d : collector_->last_conns()) {
        ss << "conn " << d.label << " alive=" << d.alive << " wait=" << d.wait
           << " total=" << d.total << "\n";
    }
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(ss.str().c_str())));
        wxTheClipboard->Close();
        SetStatusText(tr("Snapshot copied to clipboard"));
    }
}

void MainFrame::OnToggleCpu(wxCommandEvent &event) {
    if (collector_->set_show_cpu(event.IsChecked())) {
        RebuildPanes();
    }
    SyncViewMenu();
}

void MainFrame::OnToggleMemory(wxCommandEvent &event) {
    if (collector_->set_show_memory(event.IsChecked())) {
        RebuildPanes();
    }
    SyncViewMenu();
}

void MainFrame::OnToggleNetwork(wxCommandEvent &event) {
    if (event.IsChecked()) {
        collector_->set_monitor_network(true, true, {});
    } else {
        collector_->set_monitor_network(false, false, {});
    }
    RebuildPanes();
    SyncViewMenu();
}

void MainFrame::OnToggleThreads(wxCommandEvent &event) {
    if (collector_->set_show_threads(event.IsChecked())) {
        RebuildPanes();
    }
    SyncViewMenu();
}

void MainFrame::OnToggleNumFd(wxCommandEvent &event) {
    if (collector_->set_show_numfd(event.IsChecked())) {
        RebuildPanes();
    }
    SyncViewMenu();
}

void MainFrame::OnToggleConnections(wxCommandEvent &event) {
    if (collector_->set_show_connections(event.IsChecked())) {
        RebuildPanes();
    }
    SyncViewMenu();
}

void MainFrame::OnAddCapture(wxCommandEvent &) {
    CaptureRequest req;
    if (!ShowAddCaptureDialog(this, &req)) {
        return;
    }
    bool ok = false;
    switch (req.kind) {
    case CaptureKind::Device:
        ok = collector_->add_device(req.value.ToStdString());
        break;
    case CaptureKind::Iface:
        ok = collector_->add_iface(req.value.ToStdString());
        break;
    case CaptureKind::Addr:
        ok = collector_->add_addr(req.value.ToStdString());
        break;
    case CaptureKind::Pid: {
        ok = collector_->add_name(req.value.ToStdString());
        break;
    }
    }
    if (ok) {
        RebuildPanes();
        SyncViewMenu();
    }
}

void MainFrame::OnRemoveCapture(wxCommandEvent &) {
    if (active_chart_id_.empty()) {
        wxMessageBox(tr("Click a chart first, then Remove."), "pidload", wxOK | wxICON_INFORMATION,
                     this);
        return;
    }
    if (collector_->remove_chart(active_chart_id_)) {
        RebuildPanes();
        SyncViewMenu();
    }
}

void MainFrame::OnToggleLegends(wxCommandEvent &event) {
    opt_.show_legends = event.IsChecked();
    for (auto &kv : panels_) {
        kv.second->SetShowLegends(opt_.show_legends);
    }
}

void MainFrame::OnToggleYLog(wxCommandEvent &event) {
    opt_.y_log = event.IsChecked();
    for (auto &kv : panels_) {
        kv.second->SetYLog(opt_.y_log);
    }
}

void MainFrame::ApplyShowAsToAll(ChartShowAs mode) {
    opt_.show_as = mode;
    for (auto &kv : panels_) {
        kv.second->SetShowAs(mode);
    }
    SyncViewMenu();
}

void MainFrame::ApplyCurveStyleToAll(CurveStyle style) {
    opt_.curve_style = style;
    for (auto &kv : panels_) {
        kv.second->SetCurveStyle(style);
    }
    SyncViewMenu();
}

void MainFrame::OnShowAs(wxCommandEvent &event) {
    if (event.GetId() == ID_ShowDots) {
        ApplyShowAsToAll(ChartShowAs::Dots);
    } else if (event.GetId() == ID_ShowCurve) {
        ApplyShowAsToAll(ChartShowAs::Curve);
    } else if (event.GetId() == ID_ShowBars) {
        ApplyShowAsToAll(ChartShowAs::Bars);
    } else {
        ApplyShowAsToAll(ChartShowAs::StackedBars);
    }
}

void MainFrame::OnCurveStyle(wxCommandEvent &event) {
    if (event.GetId() == ID_CurveSeg) {
        ApplyCurveStyleToAll(CurveStyle::Segment);
    } else if (event.GetId() == ID_CurveBezier) {
        ApplyCurveStyleToAll(CurveStyle::Bezier);
    } else {
        ApplyCurveStyleToAll(CurveStyle::Bicubic);
    }
}

void MainFrame::OnInterval(wxCommandEvent &event) {
    if (event.GetId() == ID_Interval1s) {
        SetIntervalMs(1000);
    } else if (event.GetId() == ID_Interval2s) {
        SetIntervalMs(2000);
    } else {
        SetIntervalMs(5000);
    }
}

void MainFrame::OnResetLayout(wxCommandEvent &) {
    aui_perspective_.clear();
    ApplyDefaultLayout();
}

void MainFrame::OnResetView(wxCommandEvent &) {
    time_view_.set_capture_window(opt_.window_ms);
    time_view_.reset_live();
    RefreshAllCharts();
}

void MainFrame::OnAbout(wxCommandEvent &) {
    wxAboutDialogInfo info;
    info.SetName("pidload");
    info.SetVersion(PROJECT_VERSION);
    info.SetDescription(tr("Process and traffic monitor with dockable charts."));
    info.SetCopyright(wxString::Format("(C) %d %s", PROJECT_YEAR, PROJECT_AUTHOR));
    info.SetWebSite(wxString::Format("mailto:%s", PROJECT_EMAIL));
    info.AddDeveloper(PROJECT_AUTHOR);
    info.SetLicence("AGPL-3.0-or-later");
    wxAboutBox(info, this);
}
