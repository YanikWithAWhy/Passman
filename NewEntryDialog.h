#ifndef NEW_ENTRY_DIALOG_H
#define NEW_ENTRY_DIALOG_H

#pragma once
#include <wx/wx.h>
#include "PasswordDatabase.h"

class NewEntryDialog : public wxDialog {
public:
    NewEntryDialog(wxWindow* parent);
    PasswordEntry getEntry();

private:
    void OnShowPassword(wxCommandEvent& event);

    wxTextCtrl* titleCtrl;
    wxTextCtrl* usernameCtrl;
    wxTextCtrl* passwordCtrl;           // Verstecktes Feld (immer wxTE_PASSWORD)
    wxTextCtrl* passwordVisibleCtrl;    // Sichtbares Feld (normal)
    wxTextCtrl* notesCtrl;
    wxTextCtrl* urlCtrl;
    wxCheckBox* showPasswordCheck;
    PasswordEntry entry;
};

#endif //NEW_ENTRY_DIALOG_H
