#ifndef PASSWORD_GENERATOR_DIALOG_H
#define PASSWORD_GENERATOR_DIALOG_H

#include <wx/wx.h>
#include <wx/clipbrd.h>

class PasswordGeneratorDialog : public wxDialog {
public:
    PasswordGeneratorDialog(wxWindow* parent, wxTextCtrl* targetPasswordCtrl, wxTextCtrl* targetVisibleCtrl);

private:
    wxTextCtrl* lengthCtrl;
    wxCheckBox* lowercaseCheck;
    wxCheckBox* uppercaseCheck;
    wxCheckBox* numbersCheck;
    wxCheckBox* symbolsCheck;
    wxTextCtrl* targetPasswordCtrl;
    wxTextCtrl* targetVisibleCtrl;

    std::string pendingPassword;
    wxString originalPassword;
    wxString originalVisiblePassword;

    void OnCheckbox(wxCommandEvent& event);
    void OnLengthChange(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

    std::string generateSecurePassword(int length, bool useLower, bool useUpper, bool useNumbers, bool useSymbols);

    wxDECLARE_EVENT_TABLE();
};

#endif
