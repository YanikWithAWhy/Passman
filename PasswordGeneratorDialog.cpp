#include "PasswordGeneratorDialog.h"
#include <wx/sizer.h>
#include <random>
#include <algorithm>

wxBEGIN_EVENT_TABLE(PasswordGeneratorDialog, wxDialog)
    EVT_CHECKBOX(-1, PasswordGeneratorDialog::OnCheckbox)
    EVT_TEXT(wxID_ANY, PasswordGeneratorDialog::OnLengthChange)
    EVT_BUTTON(wxID_OK, PasswordGeneratorDialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, PasswordGeneratorDialog::OnCancel)
wxEND_EVENT_TABLE()

PasswordGeneratorDialog::PasswordGeneratorDialog(wxWindow* parent,
                                               wxTextCtrl* targetPasswordCtrl,
                                               wxTextCtrl* targetVisibleCtrl)
    : wxDialog(parent, -1, "Passwort Generator", wxDefaultPosition, wxSize(350, 300))
    , targetPasswordCtrl(targetPasswordCtrl)
    , targetVisibleCtrl(targetVisibleCtrl)
    , pendingPassword("") {

    if (targetPasswordCtrl && targetVisibleCtrl) {
        originalPassword = targetPasswordCtrl->GetValue();
        originalVisiblePassword = targetVisibleCtrl->GetValue();
    }

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* lengthSizer = new wxBoxSizer(wxHORIZONTAL);
    lengthSizer->Add(new wxStaticText(this, -1, "Length:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    lengthCtrl = new wxTextCtrl(this, wxID_ANY, "16", wxDefaultPosition, wxSize(60, -1));
    lengthCtrl->Bind(wxEVT_TEXT, &PasswordGeneratorDialog::OnLengthChange, this);
    lengthSizer->Add(lengthCtrl, 0);
    mainSizer->Add(lengthSizer, 0, wxEXPAND | wxALL, 10);

    lowercaseCheck = new wxCheckBox(this, -1, "lower case letters (a-z)");
    lowercaseCheck->SetValue(true);
    mainSizer->Add(lowercaseCheck, 0, wxLEFT | wxRIGHT, 15);

    uppercaseCheck = new wxCheckBox(this, -1, "upper case letters (A-Z)");
    uppercaseCheck->SetValue(true);
    mainSizer->Add(uppercaseCheck, 0, wxLEFT | wxRIGHT, 15);

    numbersCheck = new wxCheckBox(this, -1, "numbers (0-9)");
    numbersCheck->SetValue(true);
    mainSizer->Add(numbersCheck, 0, wxLEFT | wxRIGHT, 15);

    symbolsCheck = new wxCheckBox(this, -1, "special characters (!@#$%^&*)");
    symbolsCheck->SetValue(true);
    mainSizer->Add(symbolsCheck, 0, wxLEFT | wxRIGHT, 15);

    wxSizer* dialogButtons = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    mainSizer->Add(dialogButtons, 0, wxALIGN_RIGHT | wxALL, 10);

    SetSizerAndFit(mainSizer);
    Center();

    wxCommandEvent dummy;
    OnLengthChange(dummy);
}

void PasswordGeneratorDialog::OnCheckbox(wxCommandEvent& event) {
    wxCommandEvent dummy;
    OnLengthChange(dummy);
}

void PasswordGeneratorDialog::OnLengthChange(wxCommandEvent& event) {
    int length;
    try {
        length = std::stoi(lengthCtrl->GetValue().ToStdString());
        if (length < 8 || length > 128) {
            pendingPassword = "";
            return;
        }
    } catch (...) {
        pendingPassword = "";
        return;
    }

    bool useLower = lowercaseCheck->IsChecked();
    bool useUpper = uppercaseCheck->IsChecked();
    bool useNums = numbersCheck->IsChecked();
    bool useSyms = symbolsCheck->IsChecked();

    if (useLower || useUpper || useNums || useSyms) {
        pendingPassword = generateSecurePassword(length, useLower, useUpper, useNums, useSyms);
    } else {
        pendingPassword = "";
    }

}

void PasswordGeneratorDialog::OnOK(wxCommandEvent& event) {

    if (!pendingPassword.empty() && targetPasswordCtrl && targetVisibleCtrl) {
        wxString pwd = wxString::FromUTF8(pendingPassword.c_str());
        targetPasswordCtrl->SetValue(pwd);
        targetVisibleCtrl->SetValue(pwd);
    }
    EndModal(wxID_OK);
}

void PasswordGeneratorDialog::OnCancel(wxCommandEvent& event) {

    if (targetPasswordCtrl && targetVisibleCtrl) {
        targetPasswordCtrl->SetValue(originalPassword);
        targetVisibleCtrl->SetValue(originalVisiblePassword);
    }
    EndModal(wxID_CANCEL);
}

std::string PasswordGeneratorDialog::generateSecurePassword(int length,
                                                          bool useLower, bool useUpper,
                                                          bool useNumbers, bool useSymbols) {
    std::string charset;
    if (useLower) charset += "abcdefghijklmnopqrstuvwxyz";
    if (useUpper) charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (useNumbers) charset += "0123456789";
    if (useSymbols) charset += "!@#$%^&*()_+-=[]{}|;:,.<>?";

    if (charset.empty()) return "";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.size() - 1);

    std::string password;
    password.reserve(length);
    for (int i = 0; i < length; ++i) {
        password += charset[dis(gen)];
    }

    int pos = 0;
    if (useLower && password.find_first_of("abcdefghijklmnopqrstuvwxyz") == std::string::npos && pos < length) {
        password[pos++] = 'a' + dis(gen) % 26;
    }
    if (useUpper && password.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos && pos < length) {
        password[pos++] = 'A' + dis(gen) % 26;
    }
    if (useNumbers && password.find_first_of("0123456789") == std::string::npos && pos < length) {
        password[pos++] = '0' + dis(gen) % 10;
    }
    if (useSymbols && password.find_first_of("!@#$%^&*()_+-=[]{}|;:,.<>?") == std::string::npos && pos < length) {
        password[pos] = "!@#$%^&*()_+-=[]{}|;:,.<>?"[dis(gen) % 28];
    }

    return password;
}
