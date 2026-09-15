/*
 * Copyright (C) 2026 Lenik <pidload@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "dialogs.hpp"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>

namespace {

struct ProcRow {
    pid_t pid = 0;
    std::string user;
    std::string cmd;
};

std::vector<ProcRow> list_processes() {
    std::vector<ProcRow> rows;
    DIR *dir = opendir("/proc");
    if (!dir) {
        return rows;
    }
    while (dirent *ent = readdir(dir)) {
        char *end = nullptr;
        long pid = strtol(ent->d_name, &end, 10);
        if (!end || *end || pid <= 0) {
            continue;
        }
        ProcRow row;
        row.pid = static_cast<pid_t>(pid);
        std::ifstream comm("/proc/" + std::string(ent->d_name) + "/comm");
        std::getline(comm, row.cmd);
        if (row.cmd.empty()) {
            row.cmd = "?";
        }
        std::ifstream status("/proc/" + std::string(ent->d_name) + "/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.compare(0, 4, "Uid:") == 0) {
                std::istringstream ss(line.substr(4));
                long uid = 0;
                ss >> uid;
                row.user = std::to_string(uid);
                break;
            }
        }
        rows.push_back(row);
    }
    closedir(dir);
    std::sort(rows.begin(), rows.end(),
              [](const ProcRow &a, const ProcRow &b) { return a.pid < b.pid; });
    return rows;
}

class ProcessPickerDialog : public wxDialog {
public:
    explicit ProcessPickerDialog(wxWindow *parent)
        : wxDialog(parent, wxID_ANY, "Open Process", wxDefaultPosition, wxSize(560, 420),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
        list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL);
        list_->AppendColumn("PID", wxLIST_FORMAT_LEFT, 80);
        list_->AppendColumn("UID", wxLIST_FORMAT_LEFT, 70);
        list_->AppendColumn("Command", wxLIST_FORMAT_LEFT, 360);

        auto procs = list_processes();
        long i = 0;
        for (const auto &p : procs) {
            long idx = list_->InsertItem(i++, wxString::Format("%d", static_cast<int>(p.pid)));
            list_->SetItem(idx, 1, wxString::FromUTF8(p.user.c_str()));
            list_->SetItem(idx, 2, wxString::FromUTF8(p.cmd.c_str()));
            list_->SetItemData(idx, static_cast<long>(p.pid));
        }

        auto *btns = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
        auto *root = new wxBoxSizer(wxVERTICAL);
        root->Add(new wxStaticText(this, wxID_ANY, "Select a process to monitor:"), 0,
                  wxALL, 8);
        root->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);
        if (btns) {
            root->Add(btns, 0, wxEXPAND | wxALL, 8);
        }
        SetSizer(root);
        list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent &) { EndModal(wxID_OK); });
    }

    pid_t selected_pid() const {
        long sel = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel < 0) {
            return -1;
        }
        return static_cast<pid_t>(list_->GetItemData(sel));
    }

private:
    wxListCtrl *list_;
};

} // namespace

std::vector<pid_t> ShowProcessPicker(wxWindow *parent) {
    ProcessPickerDialog dlg(parent);
    if (dlg.ShowModal() != wxID_OK) {
        return {};
    }
    pid_t pid = dlg.selected_pid();
    if (pid <= 0) {
        return {};
    }
    return {pid};
}

pid_t ShowOpenPidDialog(wxWindow *parent) {
    wxTextEntryDialog dlg(parent, "Enter process ID:", "Open PID");
    if (dlg.ShowModal() != wxID_OK) {
        return -1;
    }
    long pid = 0;
    if (!dlg.GetValue().ToLong(&pid) || pid <= 0) {
        wxMessageBox("Invalid PID.", "pidload", wxOK | wxICON_WARNING, parent);
        return -1;
    }
    return static_cast<pid_t>(pid);
}

bool ShowAddCaptureDialog(wxWindow *parent, CaptureRequest *out) {
    wxDialog dlg(parent, wxID_ANY, "Add Capture", wxDefaultPosition, wxSize(420, 160));
    auto *kind = new wxChoice(&dlg, wxID_ANY);
    kind->Append("Device");
    kind->Append("Interface");
    kind->Append("Address");
    kind->Append("Process NAME");
    kind->SetSelection(0);
    auto *value = new wxTextCtrl(&dlg, wxID_ANY);
    auto *hint = new wxStaticText(
        &dlg, wxID_ANY, "NAME: pid, exe, path, window title, or glob (* ?)");

    auto *form = new wxFlexGridSizer(2, 2, 8, 8);
    form->AddGrowableCol(1, 1);
    form->Add(new wxStaticText(&dlg, wxID_ANY, "Type:"), 0, wxALIGN_CENTER_VERTICAL);
    form->Add(kind, 1, wxEXPAND);
    form->Add(new wxStaticText(&dlg, wxID_ANY, "Value:"), 0, wxALIGN_CENTER_VERTICAL);
    form->Add(value, 1, wxEXPAND);

    auto *btns = dlg.CreateSeparatedButtonSizer(wxOK | wxCANCEL);
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(form, 0, wxEXPAND | wxALL, 10);
    root->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    if (btns) {
        root->Add(btns, 0, wxEXPAND | wxALL, 8);
    }
    dlg.SetSizer(root);

    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }
    out->value = value->GetValue().Trim(true).Trim(false);
    if (out->value.empty()) {
        return false;
    }
    switch (kind->GetSelection()) {
    case 0:
        out->kind = CaptureKind::Device;
        break;
    case 1:
        out->kind = CaptureKind::Iface;
        break;
    case 2:
        out->kind = CaptureKind::Addr;
        break;
    default:
        out->kind = CaptureKind::Pid;
        break;
    }
    return true;
}
