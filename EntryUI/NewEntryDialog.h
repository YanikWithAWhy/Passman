#ifndef NEW_ENTRY_DIALOG_H
#define NEW_ENTRY_DIALOG_H

#pragma once
#include <wx/wx.h>
#include "..\PasswordDatabase.h"

class NewEntryDialog : public wxDialog {
public:
    NewEntryDialog(wxWindow *parent);

    PasswordEntry getEntry();

private:
    void OnShowPassword(wxCommandEvent &event);

    void OnGeneratePassword(wxCommandEvent &event);

    wxTextCtrl *titleCtrl;
    wxTextCtrl *usernameCtrl;
    wxTextCtrl *passwordCtrl;
    wxTextCtrl *passwordVisibleCtrl;
    wxTextCtrl *notesCtrl;
    wxTextCtrl *urlCtrl;
    wxCheckBox *showPasswordCheck;
    wxButton *generatePasswordBtn;
    PasswordEntry entry;
};

#endif //NEW_ENTRY_DIALOG_H
