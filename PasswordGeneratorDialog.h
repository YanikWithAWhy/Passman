#pragma once
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <string>

class PasswordGeneratorDialog : public wxDialog {
public:
    explicit PasswordGeneratorDialog(wxWindow* parent);

    PasswordGeneratorDialog(wxWindow*    parent,
                            wxTextCtrl*  targetCtrl,
                            wxTextCtrl*  visibleCtrl);

    wxString getPassword() const;

private:
    void buildUI();

    wxSpinCtrl* spinLength  = nullptr;
    wxCheckBox* chkUpper    = nullptr;
    wxCheckBox* chkLower    = nullptr;
    wxCheckBox* chkDigits   = nullptr;
    wxCheckBox* chkSymbols  = nullptr;

    wxTextCtrl* targetCtrl  = nullptr;
    wxTextCtrl* visibleCtrl = nullptr;

    std::string generatedPassword;

    void OnGenerate(wxCommandEvent&);
    void OnOK(wxCommandEvent&);
    void OnCancel(wxCommandEvent&);

    bool generatePassword();
};
