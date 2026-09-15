/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PIDLOAD_MAIN_FRAME_HPP
#define PIDLOAD_MAIN_FRAME_HPP

#include "chart_panel.hpp"
#include "collector.hpp"
#include "options.hpp"
#include "recorder.hpp"
#include "time_view.hpp"

#include <wx/aui/framemanager.h>
#include <wx/frame.h>
#include <wx/timer.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MainFrame : public wxFrame {
public:
    MainFrame(const Options &opt);
    ~MainFrame() override;

private:
    enum {
        ID_OpenProcess = wxID_HIGHEST + 1,
        ID_OpenPid,
        ID_ClearHistory,
        ID_CopySnapshot,
        ID_ViewCpu,
        ID_ViewMemory,
        ID_ViewNetwork,
        ID_ViewThreads,
        ID_ViewNumFd,
        ID_ViewConnections,
        ID_AddCapture,
        ID_RemoveCapture,
        ID_ViewLegends,
        ID_ViewYLog,
        ID_ShowDots,
        ID_ShowCurve,
        ID_ShowBars,
        ID_ShowStacked,
        ID_CurveSeg,
        ID_CurveQuad,
        ID_CurveCubic,
        ID_Interval1s,
        ID_Interval2s,
        ID_Interval5s,
        ID_ResetLayout,
        ID_ResetView,
        ID_About,
    };

    void BuildMenu();
    void RebuildPanes();
    void ApplyDefaultLayout();
    void SyncViewMenu();
    void ApplyShowAsToAll(ChartShowAs mode);
    void ApplyCurveStyleToAll(CurveStyle style);
    void SetIntervalMs(int64_t ms);
    void RefreshAllCharts();
    void AssignTimeAxis();
    ChartPanel *FocusedPanel();

    void OnTimer(wxTimerEvent &event);
    void OnClose(wxCloseEvent &event);
    void OnOpenProcess(wxCommandEvent &event);
    void OnOpenPid(wxCommandEvent &event);
    void OnQuit(wxCommandEvent &event);
    void OnClearHistory(wxCommandEvent &event);
    void OnCopySnapshot(wxCommandEvent &event);
    void OnToggleCpu(wxCommandEvent &event);
    void OnToggleMemory(wxCommandEvent &event);
    void OnToggleNetwork(wxCommandEvent &event);
    void OnToggleThreads(wxCommandEvent &event);
    void OnToggleNumFd(wxCommandEvent &event);
    void OnToggleConnections(wxCommandEvent &event);
    void OnAddCapture(wxCommandEvent &event);
    void OnRemoveCapture(wxCommandEvent &event);
    void OnToggleLegends(wxCommandEvent &event);
    void OnToggleYLog(wxCommandEvent &event);
    void OnShowAs(wxCommandEvent &event);
    void OnCurveStyle(wxCommandEvent &event);
    void OnInterval(wxCommandEvent &event);
    void OnResetLayout(wxCommandEvent &event);
    void OnResetView(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);

    Options opt_;
    std::unique_ptr<Collector> collector_;
    std::unique_ptr<Recorder> recorder_;
    TimeView time_view_;
    wxAuiManager aui_;
    std::unordered_map<std::string, ChartPanel *> panels_;
    std::string active_chart_id_;
    wxTimer timer_;
    int64_t start_ms_ = 0;
};

#endif /* PIDLOAD_MAIN_FRAME_HPP */
