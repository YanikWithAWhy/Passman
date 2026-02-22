#ifndef EDIT_ENTRY_DIALOG_H
#define EDIT_ENTRY_DIALOG_H

#pragma once
#include <wx/wx.h>
#include "PasswordDatabase.h"

class EditEntryDialog : public wxDialog {
public:
    EditEntryDialog(wxWindow *parent, const PasswordEntry &entry);

    PasswordEntry getEntry();

private:
    void OnShowPassword(wxCommandEvent &event);

    wxTextCtrl *titleCtrl;
    wxTextCtrl *usernameCtrl;
    wxTextCtrl *passwordCtrl;
    wxTextCtrl *passwordVisibleCtrl;
    wxTextCtrl *notesCtrl;
    wxTextCtrl *urlCtrl;
    wxCheckBox *showPasswordCheck;
    PasswordEntry entry;
};

#endif
