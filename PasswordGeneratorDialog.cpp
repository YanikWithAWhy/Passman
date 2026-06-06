
#include "PasswordGeneratorDialog.h"
#include <sodium.h>
#include <wx/spinctrl.h>
#include <stdexcept>
#include <cstring>


static char securePickChar(const std::string& charset) {
    if (charset.empty())
        throw std::invalid_argument("charset must not be empty");

    const size_t   n     = charset.size();
    const unsigned limit = 256 - (256 % n);

    unsigned char pick;
    do {
        randombytes_buf(&pick, 1);
    } while (pick >= limit);

    return charset[pick % n];
}

PasswordGeneratorDialog::PasswordGeneratorDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Password Generator",
               wxDefaultPosition, wxSize(260, 240))
{
    buildUI();
}

PasswordGeneratorDialog::PasswordGeneratorDialog(wxWindow*   parent,
                                                  wxTextCtrl* target,
                                                  wxTextCtrl* visible)
    : wxDialog(parent, wxID_ANY, "Password Generator",
               wxDefaultPosition, wxSize(260, 240))
    , targetCtrl(target)
    , visibleCtrl(visible)
{
    buildUI();
}

void PasswordGeneratorDialog::buildUI() {
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* lenRow = new wxBoxSizer(wxHORIZONTAL);
    lenRow->Add(new wxStaticText(this, wxID_ANY, "Length:"),
                0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    spinLength = new wxSpinCtrl(this, wxID_ANY, "20", wxDefaultPosition,
                                 wxDefaultSize, wxSP_ARROW_KEYS, 8, 128, 20);
    lenRow->Add(spinLength, 1);
    sizer->Add(lenRow, 0, wxEXPAND | wxALL, 6);

    chkUpper   = new wxCheckBox(this, wxID_ANY, "Uppercase  (A-Z)");
    chkLower   = new wxCheckBox(this, wxID_ANY, "Lowercase  (a-z)");
    chkDigits  = new wxCheckBox(this, wxID_ANY, "Digits     (0-9)");
    chkSymbols = new wxCheckBox(this, wxID_ANY, "Symbols    (!@#$%^&*...)");

    chkUpper->SetValue(true);
    chkLower->SetValue(true);
    chkDigits->SetValue(true);
    chkSymbols->SetValue(true);

    sizer->Add(chkUpper,   0, wxLEFT | wxRIGHT | wxTOP, 6);
    sizer->Add(chkLower,   0, wxLEFT | wxRIGHT | wxTOP, 4);
    sizer->Add(chkDigits,  0, wxLEFT | wxRIGHT | wxTOP, 4);
    sizer->Add(chkSymbols, 0, wxLEFT | wxRIGHT | wxTOP, 4);

    auto* btnGen = new wxButton(this, wxID_ANY, "Generate");
    sizer->Add(btnGen, 0, wxEXPAND | wxALL, 6);

    SetSizer(sizer);

    btnGen->Bind(wxEVT_BUTTON, &PasswordGeneratorDialog::OnGenerate, this);

    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
        if (IsModal()) EndModal(wxID_OK);
        else           e.Skip();
    });
}

bool PasswordGeneratorDialog::generatePassword() {
    std::string charset;
    if (chkUpper->GetValue())   charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (chkLower->GetValue())   charset += "abcdefghijklmnopqrstuvwxyz";
    if (chkDigits->GetValue())  charset += "0123456789";
    if (chkSymbols->GetValue()) charset += "!@#$%^&*()-_=+[]{}|;:,.<>?";

    if (charset.empty()) return false;

    const int length = spinLength->GetValue();
    if (length < 1 || length > 128) return false;

    static constexpr int MAX_RETRIES = 100;
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        std::string pwd;
        pwd.reserve(static_cast<size_t>(length));
        for (int i = 0; i < length; ++i)
            pwd += securePickChar(charset);

        auto hasClass = [&](const std::string& cls) {
            return pwd.find_first_of(cls) != std::string::npos;
        };
        bool ok = true;
        if (chkUpper->GetValue()   && !hasClass("ABCDEFGHIJKLMNOPQRSTUVWXYZ")) ok = false;
        if (chkLower->GetValue()   && !hasClass("abcdefghijklmnopqrstuvwxyz")) ok = false;
        if (chkDigits->GetValue()  && !hasClass("0123456789"))                  ok = false;
        if (chkSymbols->GetValue() && !hasClass("!@#$%^&*()-_=+[]{}|;:,.<>?")) ok = false;

        if (!ok) {
            sodium_memzero(&pwd[0], pwd.size());
            continue;
        }

        if (!generatedPassword.empty())
            sodium_memzero(const_cast<char*>(generatedPassword.data()),
                           generatedPassword.size());
        generatedPassword = pwd;
        sodium_memzero(&pwd[0], pwd.size());
        return true;
    }
    return false;
}

void PasswordGeneratorDialog::OnGenerate(wxCommandEvent&) {
    if (!generatePassword()) {
        wxMessageBox("Please select at least one character set.",
                     "No charset selected", wxOK | wxICON_WARNING, this);
        return;
    }

    wxString out = wxString::FromUTF8(generatedPassword.c_str());

    if (targetCtrl)  targetCtrl->SetValue(out);
    if (visibleCtrl) visibleCtrl->SetValue(out);

    EndModal(wxID_OK);
}

void PasswordGeneratorDialog::OnCancel(wxCommandEvent&) {
    if (!generatedPassword.empty()) {
        sodium_memzero(const_cast<char*>(generatedPassword.data()),
                       generatedPassword.size());
        generatedPassword.clear();
    }
    EndModal(wxID_CANCEL);
}

void PasswordGeneratorDialog::OnOK(wxCommandEvent&) { EndModal(wxID_OK); }

wxString PasswordGeneratorDialog::getPassword() const {
    return wxString::FromUTF8(generatedPassword.c_str());
}
