#pragma once

#ifndef PASSMAN_SECURE_PASSWORD_DIALOG_H
#define PASSMAN_SECURE_PASSWORD_DIALOG_H

#include <wx/wx.h>
#include "SecureMemory.h"

class SecurePasswordDialog : public wxDialog {
public:
    SecurePasswordDialog(wxWindow*      parent,
                         const wxString& prompt,
                         const wxString& title,
                         bool            showConfirm = false)
        : wxDialog(parent, wxID_ANY, title,
                   wxDefaultPosition, wxSize(420, showConfirm ? 220 : 170),
                   wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
        , hasConfirm(showConfirm)
    {
        auto* sizer = new wxBoxSizer(wxVERTICAL);

        sizer->Add(new wxStaticText(this, wxID_ANY, prompt),
                   0, wxALL, 12);

        pwdCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_PASSWORD | wxTE_PROCESS_ENTER);
        sizer->Add(pwdCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);

        if (showConfirm) {
            sizer->AddSpacer(8);
            sizer->Add(new wxStaticText(this, wxID_ANY, "Confirm password:"),
                       0, wxLEFT | wxRIGHT, 12);
            confirmCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxTE_PASSWORD | wxTE_PROCESS_ENTER);
            sizer->Add(confirmCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);
        }

        sizer->AddSpacer(12);
        sizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL),
                   0, wxEXPAND | wxALL, 12);

        SetSizer(sizer);
        pwdCtrl->SetFocus();

        pwdCtrl->Bind(wxEVT_TEXT_PASTE,
                      [](wxClipboardTextEvent& e) { /* swallow paste */ });
        if (confirmCtrl)
            confirmCtrl->Bind(wxEVT_TEXT_PASTE,
                              [](wxClipboardTextEvent& e) { /* swallow paste */ });

        Bind(wxEVT_BUTTON, &SecurePasswordDialog::OnOK,     this, wxID_OK);
        Bind(wxEVT_BUTTON, &SecurePasswordDialog::OnCancel, this, wxID_CANCEL);

        pwdCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
            wxCommandEvent dummy(wxEVT_BUTTON, wxID_OK);
            OnOK(dummy);
        });
    }

    ~SecurePasswordDialog() override { wipeControls(); }

    const SecureString& getPassword()        const { return password; }
    const SecureString& getConfirmPassword() const { return confirm;  }

    bool passwordsMatch() const {
        if (!hasConfirm) return true;
        if (password.size() != confirm.size()) return false;
        return sodium_memcmp(password.data(), confirm.data(), password.size()) == 0;
    }

private:
    wxTextCtrl* pwdCtrl     = nullptr;
    wxTextCtrl* confirmCtrl = nullptr;
    SecureString password;
    SecureString confirm;
    bool hasConfirm;

    static SecureString drain(wxTextCtrl* ctrl) {
        if (!ctrl) return {};
        wxString wx = ctrl->GetValue();

        SecureString result;
        result.reserve(wx.size());
        for (size_t i = 0; i < wx.size(); ++i)
            result += static_cast<char>(wx[i]);

        if (!wx.empty()) {
            wxStringBuffer buf(wx, wx.length());
            memset(static_cast<wxChar*>(buf), 0, wx.length() * sizeof(wxChar));
        }

        ctrl->ChangeValue(wxString(wx.length(), ' '));
        ctrl->Clear();
        return result;
    }

    void wipeControls() {
        if (pwdCtrl)     { auto tmp = drain(pwdCtrl);     pwdCtrl = nullptr; }
        if (confirmCtrl) { auto tmp = drain(confirmCtrl); confirmCtrl = nullptr; }
    }

    void OnOK(wxCommandEvent&) {
        password = drain(pwdCtrl);
        if (hasConfirm) confirm = drain(confirmCtrl);
        if (password.empty()) {
            wxMessageBox("Please enter a password.", "Error",
                         wxOK | wxICON_WARNING, this);
            return;
        }
        if (hasConfirm && !passwordsMatch()) {
            sodium_memzero(const_cast<char*>(password.data()), password.size());
            password.clear();
            sodium_memzero(const_cast<char*>(confirm.data()), confirm.size());
            confirm.clear();
            wxMessageBox("Passwords don't match. Please try again.", "Error",
                         wxOK | wxICON_WARNING, this);
            return;
        }
        EndModal(wxID_OK);
    }

    void OnCancel(wxCommandEvent&) {
        wipeControls();
        EndModal(wxID_CANCEL);
    }
};

#endif // PASSMAN_SECURE_PASSWORD_DIALOG_H