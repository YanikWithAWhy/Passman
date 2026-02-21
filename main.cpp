#include <memory>
#include <wx/wx.h>
#include "PasswordDatabase.h"
#include "NewEntryDialog.h"
#include <wx/listctrl.h>

using namespace std;

class PasswordManagerApp : public wxApp {
public:
    bool OnInit() override;
};

class PasswordManagerFrame : public wxFrame {
private:
    unique_ptr<PasswordDatabase> database;;
    wxListCtrl* entryList;

    enum {
        ID_DELETE = wxID_HIGHEST + 1,
        ID_LIST_SELECT
    };

public:
    PasswordManagerFrame();

private:
    void createMenu();
    void createUI();
    void refreshList();
    void enableMenuItems(bool enabled);

    void OnUnlock(wxCommandEvent&);
    void OnNewEntry(wxCommandEvent&);
    void OnDeleteEntry(wxCommandEvent&);
    void OnSave(wxCommandEvent&);
    void OnExit(wxCommandEvent&);
    void OnEntrySelected(wxListEvent&);
    void OnCloseWindow(wxCloseEvent&);
};

bool PasswordManagerApp::OnInit() {
    PasswordManagerFrame* frame = new PasswordManagerFrame();
    frame->Show(true);
    return true;
}

PasswordManagerFrame::PasswordManagerFrame()
    : wxFrame(nullptr, wxID_ANY, "Password Manager v1.0", wxDefaultPosition, wxSize(1000, 700)) {

    createMenu();
    createUI();
    CreateStatusBar(2);
    SetStatusText("Please unlock database first", 0);

    Bind(wxEVT_MENU, &PasswordManagerFrame::OnUnlock, this, wxID_OPEN);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnNewEntry, this, wxID_NEW);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnDeleteEntry, this, ID_DELETE);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_LIST_ITEM_SELECTED, &PasswordManagerFrame::OnEntrySelected, this, ID_LIST_SELECT);
    Bind(wxEVT_CLOSE_WINDOW, &PasswordManagerFrame::OnCloseWindow, this);

    enableMenuItems(false);
}

void PasswordManagerFrame::createMenu() {
    wxMenu* menuFile = new wxMenu();
    menuFile->Append(wxID_OPEN, "&Open Database...\tCtrl+O");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_NEW, "&New Entry\tCtrl+N");
    menuFile->Append(ID_DELETE, "&Delete Entry\tDel");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_SAVE, "&Save\tCtrl+S");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT, "&Exit\tCtrl+Q");

    wxMenuBar* menuBar = new wxMenuBar();
    menuBar->Append(menuFile, "&File");
    SetMenuBar(menuBar);
}

void PasswordManagerFrame::createUI() {
    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    entryList = new wxListCtrl(panel, ID_LIST_SELECT, wxDefaultPosition, wxDefaultSize,
                              wxLC_REPORT | wxLC_SINGLE_SEL);

    entryList->InsertColumn(0, "Title", wxLIST_FORMAT_LEFT, 250);
    entryList->InsertColumn(1, "Username", wxLIST_FORMAT_LEFT, 180);
    entryList->InsertColumn(2, "URL", wxLIST_FORMAT_LEFT, 250);
    entryList->InsertColumn(3, "Modified", wxLIST_FORMAT_LEFT, 150);

    mainSizer->Add(entryList, 1, wxEXPAND | wxALL, 10);
    panel->SetSizer(mainSizer);
}

void PasswordManagerFrame::OnUnlock(wxCommandEvent& event) {
    wxFileDialog dialog(this, "Open Password Database", "", "passwords.pmdb",
                       "PMDB Files (*.pmdb)|*.pmdb|All Files (*.*)|*.*",
                       wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dialog.ShowModal() != wxID_OK) return;

    wxString password = wxGetPasswordFromUser(
        "Enter master password:", "Unlock Database", "", this);

    database = make_unique<PasswordDatabase>(dialog.GetPath().ToStdString());

    if (password.empty()) return;

    database = std::make_unique<PasswordDatabase>(dialog.GetPath().ToStdString());
    if (database->unlock(password.ToStdString())) {
        refreshList();
        SetStatusText(wxString::Format("Database unlocked: %zu entries", database->size()), 0);
        enableMenuItems(true);
    } else {
        wxMessageBox("Wrong password or corrupted file!", "Error", wxOK | wxICON_ERROR);
    }
}

void PasswordManagerFrame::OnNewEntry(wxCommandEvent& event) {
    if (!database || !database->isUnlocked()) {
        wxMessageBox("Please unlock database first!", "Error");
        return;
    }

    NewEntryDialog dialog(this);
    if (dialog.ShowModal() == wxID_OK) {
        PasswordEntry entry = dialog.getEntry();
        if (database->addEntry(entry)) {
            refreshList();
            SetStatusText("New entry added", 0);
        }
    }
}

void PasswordManagerFrame::OnDeleteEntry(wxCommandEvent& event) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1 || !database || !database->isUnlocked()) return;

    if (wxMessageBox("Really delete this entry?", "Confirm Delete",
                    wxYES_NO | wxICON_QUESTION) == wxYES) {
        database->deleteEntry(item);
        refreshList();
        SetStatusText("Entry deleted", 0);
    }
}

void PasswordManagerFrame::OnSave(wxCommandEvent& event) {
    if (!database || !database->isUnlocked()) return;
    if (database->save()) {
        SetStatusText("Database saved", 0);
    } else {
        wxMessageBox("Save failed!", "Error");
    }
}

void PasswordManagerFrame::OnExit(wxCommandEvent& event) {
    Close();
}

void PasswordManagerFrame::OnEntrySelected(wxListEvent& event) {
    enableMenuItems(true);
}

void PasswordManagerFrame::OnCloseWindow(wxCloseEvent& event) {
    if (database && database->isUnlocked()) {
        database->save();
    }
    event.Skip();
}

void PasswordManagerFrame::refreshList() {
    entryList->DeleteAllItems();
    if (!database || !database->isUnlocked()) return;

    const auto& entries = database->getEntries();
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        long itemId = entryList->InsertItem(i, entry.title);
        entryList->SetItem(itemId, 1, entry.username);
        entryList->SetItem(itemId, 2, entry.url);

        wxDateTime modifiedTime((time_t)entry.modified);
        entryList->SetItem(itemId, 3, modifiedTime.Format("%d.%m.%Y %H:%M"));
    }
}

void PasswordManagerFrame::enableMenuItems(bool enabled) {
    wxMenuBar* menuBar = GetMenuBar();
    menuBar->Enable(wxID_NEW, enabled);
    menuBar->Enable(ID_DELETE, enabled);
    menuBar->Enable(wxID_SAVE, enabled);
}

wxIMPLEMENT_APP(PasswordManagerApp);
