
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
    void initUI();

    wxTextCtrl* titleCtrl;
    wxTextCtrl* usernameCtrl;
    wxTextCtrl* passwordCtrl;
    wxTextCtrl* notesCtrl;
    wxTextCtrl* urlCtrl;
    PasswordEntry entry;
};


#endif //NEW_ENTRY_DIALOG_H
