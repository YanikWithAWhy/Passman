#include "NewEntryDialog.h"
#include <chrono>
#include <wx/sizer.h>
#include <wx/wx.h>
#include "..\PasswordGeneratorDialog.h"

NewEntryDialog::NewEntryDialog(wxWindow *parent)
    : wxDialog(parent, -1, "New Password Entry", wxDefaultPosition, wxSize(450, 420)) {
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *hSizer;

    // Title
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    hSizer->Add(new wxStaticText(this, -1, "Title:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    titleCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(300, 25));
    titleCtrl->SetFocus();
    hSizer->Add(titleCtrl, 1, wxEXPAND);
    mainSizer->Add(hSizer, 0, wxEXPAND | wxALL, 10);

    // Username
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    hSizer->Add(new wxStaticText(this, -1, "Username:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    usernameCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(300, 25));
    hSizer->Add(usernameCtrl, 1, wxEXPAND);
    mainSizer->Add(hSizer, 0, wxEXPAND | wxALL, 5);

    // Password
    wxBoxSizer *passwordRowSizer = new wxBoxSizer(wxHORIZONTAL);
    passwordRowSizer->Add(new wxStaticText(this, -1, "Password:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    passwordCtrl = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(300, -1), wxTE_PASSWORD);
    passwordVisibleCtrl = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(300, -1), 0);
    passwordVisibleCtrl->Hide();

    passwordRowSizer->Add(passwordCtrl, 1, wxEXPAND | wxRIGHT, 5);
    passwordRowSizer->Add(passwordVisibleCtrl, 1, wxEXPAND | wxRIGHT, 5);

    generatePasswordBtn = new wxButton(
        this,
        wxID_ANY,
        wxString::FromUTF8("\xF0\x9F\x94\x91"), // key-icon
        wxDefaultPosition,
        wxSize(35, -1)
    );
    generatePasswordBtn->Bind(wxEVT_BUTTON, &NewEntryDialog::OnGeneratePassword, this);
    passwordRowSizer->Add(generatePasswordBtn, 0, wxALIGN_CENTER_VERTICAL);

    mainSizer->Add(passwordRowSizer, 0, wxEXPAND | wxALL, 5);

    // checkbox
    showPasswordCheck = new wxCheckBox(this, -1, "Show password");
    mainSizer->Add(showPasswordCheck, 0, wxLEFT | wxRIGHT, 15);

    // URL
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    hSizer->Add(new wxStaticText(this, -1, "URL:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    urlCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(300, 25));
    hSizer->Add(urlCtrl, 1, wxEXPAND);
    mainSizer->Add(hSizer, 0, wxEXPAND | wxALL, 5);

    // Notes
    mainSizer->Add(new wxStaticText(this, -1, "Notes:"), 0, wxLEFT | wxTOP, 10);
    notesCtrl = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(-1, 80), wxTE_MULTILINE);
    mainSizer->Add(notesCtrl, 1, wxEXPAND | wxALL, 5);

    // Buttons
    wxSizer *buttonSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxALL, 10);

    SetSizerAndFit(mainSizer);

    showPasswordCheck->Bind(wxEVT_CHECKBOX, &NewEntryDialog::OnShowPassword, this);
}

PasswordEntry NewEntryDialog::getEntry() {
    entry.title = titleCtrl->GetValue().ToStdString();
    entry.username = usernameCtrl->GetValue().ToStdString();
    entry.password = passwordCtrl->GetValue().ToStdString();
    entry.notes = notesCtrl->GetValue().ToStdString();
    entry.url = urlCtrl->GetValue().ToStdString();

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entry.created = now;
    entry.modified = now;

    return entry;
}

void NewEntryDialog::OnShowPassword(wxCommandEvent &event) {
    Freeze();

    wxString currentText = passwordCtrl->GetValue();

    if (showPasswordCheck->IsChecked()) {
        passwordCtrl->Hide();
        passwordVisibleCtrl->SetValue(currentText);
        passwordVisibleCtrl->Show();
        passwordVisibleCtrl->SetFocus();
        passwordVisibleCtrl->SetInsertionPointEnd();
    } else {
        passwordVisibleCtrl->Hide();
        passwordCtrl->SetValue(currentText);
        passwordCtrl->Show();
        passwordCtrl->SetFocus();
        passwordCtrl->SetInsertionPointEnd();
    }

    Thaw();
    Layout();
}

void NewEntryDialog::OnGeneratePassword(wxCommandEvent &) {
    PasswordGeneratorDialog dlg(this, passwordCtrl, passwordVisibleCtrl);
    dlg.ShowModal();
}
