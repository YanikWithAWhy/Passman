#include "EditEntryDialog.h"
#include <chrono>
#include <wx/sizer.h>

EditEntryDialog::EditEntryDialog(wxWindow* parent, const PasswordEntry& initialEntry)
    : wxDialog(parent, -1, "Edit Password Entry", wxDefaultPosition, wxSize(450, 420))
    , entry(initialEntry) {

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* hSizer;

    // Title (vorgefüllt)
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    hSizer->Add(new wxStaticText(this, -1, "Title:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    titleCtrl = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(entry.title.c_str()),
                              wxDefaultPosition, wxSize(300, 25));
    titleCtrl->SetFocus();
    hSizer->Add(titleCtrl, 1, wxEXPAND);
    mainSizer->Add(hSizer, 0, wxEXPAND | wxALL, 10);

    // Username
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    hSizer->Add(new wxStaticText(this, -1, "Username:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    usernameCtrl = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(entry.username.c_str()),
                                 wxDefaultPosition, wxSize(300, 25));
    hSizer->Add(usernameCtrl, 1, wxEXPAND);
    mainSizer->Add(hSizer, 0, wxEXPAND | wxALL, 5);

    // Password Bereich (mit gleicher Logik wie NewEntryDialog)
    wxBoxSizer* passwordSizer = new wxBoxSizer(wxHORIZONTAL);
    passwordSizer->Add(new wxStaticText(this, -1, "Password:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    passwordCtrl = new wxTextCtrl(this, -1, wxString::FromUTF8(entry.password.c_str()),
                                 wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    passwordVisibleCtrl = new wxTextCtrl(this, -1, wxString::FromUTF8(entry.password.c_str()),
                                        wxDefaultPosition, wxDefaultSize, 0);
    passwordVisibleCtrl->Hide();

    passwordSizer->Add(passwordCtrl, 1, wxEXPAND);
    passwordSizer->Add(passwordVisibleCtrl, 1, wxEXPAND);
    mainSizer->Add(passwordSizer, 0, wxEXPAND | wxALL, 5);

    // Checkbox
    showPasswordCheck = new wxCheckBox(this, -1, "Show password");
    mainSizer->Add(showPasswordCheck, 0, wxLEFT | wxRIGHT, 15);

    // URL
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    hSizer->Add(new wxStaticText(this, -1, "URL:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    urlCtrl = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(entry.url.c_str()),
                            wxDefaultPosition, wxSize(300, 25));
    hSizer->Add(urlCtrl, 1, wxEXPAND);
    mainSizer->Add(hSizer, 0, wxEXPAND | wxALL, 5);

    // Notes
    mainSizer->Add(new wxStaticText(this, -1, "Notes:"), 0, wxLEFT | wxTOP, 10);
    notesCtrl = new wxTextCtrl(this, -1, wxString::FromUTF8(entry.notes.c_str()),
                              wxDefaultPosition, wxSize(-1, 80), wxTE_MULTILINE);
    mainSizer->Add(notesCtrl, 1, wxEXPAND | wxALL, 5);

    // Buttons
    wxSizer* buttonSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxALL, 10);

    SetSizerAndFit(mainSizer);

    // Event-Binding
    showPasswordCheck->Bind(wxEVT_CHECKBOX, &EditEntryDialog::OnShowPassword, this);
}

PasswordEntry EditEntryDialog::getEntry() {
    entry.title = titleCtrl->GetValue().ToStdString();
    entry.username = usernameCtrl->GetValue().ToStdString();
    entry.notes = notesCtrl->GetValue().ToStdString();
    entry.url = urlCtrl->GetValue().ToStdString();

    // ✅ FIX: IMMER das SICHBARE Passwort-Feld lesen!
    wxString passwordValue;
    if (showPasswordCheck->IsChecked()) {
        // Checkbox an = sichtbares Feld aktiv
        passwordValue = passwordVisibleCtrl->GetValue();
    } else {
        // Checkbox aus = passwordCtrl aktiv
        passwordValue = passwordCtrl->GetValue();
    }
    entry.password = passwordValue.ToStdString();

    // Timestamp updaten
    entry.modified = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return entry;
}

void EditEntryDialog::OnShowPassword(wxCommandEvent& event) {
    Freeze();
    wxString currentText = passwordCtrl->GetValue();  // Basisquelle

    if (showPasswordCheck->IsChecked()) {
        passwordCtrl->Hide();
        passwordVisibleCtrl->SetValue(currentText);
        passwordVisibleCtrl->Show();
        passwordVisibleCtrl->SetFocus();
        passwordVisibleCtrl->SetInsertionPointEnd();
    } else {
        passwordVisibleCtrl->Hide();
        passwordCtrl->SetValue(currentText);  // ← Synchronisation!
        passwordCtrl->Show();
        passwordCtrl->SetFocus();
        passwordCtrl->SetInsertionPointEnd();
    }

    Thaw();
    Layout();
}
