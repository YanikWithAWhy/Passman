#include "NewEntryDialog.h"
#include <chrono>
#include <wx/sizer.h>
#include <wx/wx.h>

NewEntryDialog::NewEntryDialog(wxWindow* parent)
    : wxDialog(parent, -1, "New Password Entry", wxDefaultPosition, wxSize(450, 420)) {

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* hSizer;

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

    // Password Bereich
    wxBoxSizer* passwordSizer = new wxBoxSizer(wxHORIZONTAL);
    passwordSizer->Add(new wxStaticText(this, -1, "Password:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    // Beide Felder erstellen
    passwordCtrl = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    passwordVisibleCtrl = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxDefaultSize, 0);
    passwordVisibleCtrl->Hide();  // Initial versteckt

    // BEIDE in EINEM Sizer!
    passwordSizer->Add(passwordCtrl, 1, wxEXPAND);
    passwordSizer->Add(passwordVisibleCtrl, 1, wxEXPAND);

    mainSizer->Add(passwordSizer, 0, wxEXPAND | wxALL, 5);

    // Checkbox
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
    wxSizer* buttonSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxALL, 10);

    SetSizerAndFit(mainSizer);

    // Event-Binding
    showPasswordCheck->Bind(wxEVT_CHECKBOX, &NewEntryDialog::OnShowPassword, this);
}

PasswordEntry NewEntryDialog::getEntry() {
    // IMMER vom sicheren (versteckten) Feld lesen!
    entry.title = titleCtrl->GetValue().ToStdString();
    entry.username = usernameCtrl->GetValue().ToStdString();
    entry.password = passwordCtrl->GetValue().ToStdString();  // ← Sicher!
    entry.notes = notesCtrl->GetValue().ToStdString();
    entry.url = urlCtrl->GetValue().ToStdString();

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entry.created = now;
    entry.modified = now;

    return entry;
}

void NewEntryDialog::OnShowPassword(wxCommandEvent& event) {

        Freeze();  // ← Layout einfrieren

        wxString currentText = passwordCtrl->GetValue();

        if (showPasswordCheck->IsChecked()) {
            // Verstecktes aus, Sichtbares an
            passwordCtrl->Hide();
            passwordVisibleCtrl->SetValue(currentText);
            passwordVisibleCtrl->Show();
            passwordVisibleCtrl->SetFocus();
            passwordVisibleCtrl->SetInsertionPointEnd();
        } else {
            // Sichtbares aus, Verstecktes an
            passwordVisibleCtrl->Hide();
            passwordCtrl->SetValue(currentText);
            passwordCtrl->Show();
            passwordCtrl->SetFocus();
            passwordCtrl->SetInsertionPointEnd();
        }

        Thaw();  // ← Layout wieder freigeben
        Layout();
}

