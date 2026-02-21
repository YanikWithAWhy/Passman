#include <memory>
#include <wx/wx.h>
#include "PasswordDatabase.h"
#include "NewEntryDialog.h"
#include <wx/listctrl.h>
#include <wx/clipbrd.h>

#include "EditEntryDialog.h"

using namespace std;

class PasswordManagerApp : public wxApp {
public:
    bool OnInit() override;
};

class PasswordManagerFrame : public wxFrame {
private:
    unique_ptr<PasswordDatabase> database;
    wxListCtrl* entryList;

    enum {
        ID_DELETE = wxID_HIGHEST + 1,
        ID_LIST_SELECT,
        ID_NEW_DB,
        ID_EDIT = wxID_HIGHEST + 4,
        ID_CONTEXT_MENU,
        ID_COPY_USERNAME,
        ID_COPY_PASSWORD
    };

    wxTimer* clipboardTimer;

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
    void OnNewDatabase(wxCommandEvent&);
    void OnEditEntry(wxCommandEvent&);
    void OnRightClick(wxListEvent&);
    void OnCopyUsername(wxCommandEvent& event);
    void OnCopyPassword(wxCommandEvent& event);
    void OnClipboardTimeout(wxTimerEvent& event);

    ~PasswordManagerFrame();
};

bool PasswordManagerApp::OnInit() {
    PasswordManagerFrame* frame = new PasswordManagerFrame();
    frame->Show(true);
    return true;
}

PasswordManagerFrame::PasswordManagerFrame()
    : wxFrame(nullptr, wxID_ANY, "PassMan v1.0", wxDefaultPosition, wxSize(1000, 700)) {

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
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnNewDatabase, this, ID_NEW_DB);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnEditEntry, this, ID_EDIT);
    Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &PasswordManagerFrame::OnRightClick, this, ID_LIST_SELECT);

    clipboardTimer = new wxTimer(this, wxID_ANY);  // ← Timer erstellen
    Bind(wxEVT_TIMER, &PasswordManagerFrame::OnClipboardTimeout, this, wxID_ANY);

    Bind(wxEVT_MENU, &PasswordManagerFrame::OnCopyUsername, this, ID_COPY_USERNAME);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnCopyPassword, this, ID_COPY_PASSWORD);

    wxAcceleratorEntry entries[2];
    entries[0].Set(wxACCEL_CTRL, (int)'B', ID_COPY_USERNAME);
    entries[1].Set(wxACCEL_CTRL, (int)'C', ID_COPY_PASSWORD);

    wxAcceleratorTable accelTable(2, entries);
    SetAcceleratorTable(accelTable);

    enableMenuItems(false);
}

void PasswordManagerFrame::createMenu() {
    wxMenu* menuFile = new wxMenu();
    menuFile->Append(ID_NEW_DB, "&New Database...\tCtrl+N");
    menuFile->Append(wxID_OPEN, "&Open Database...\tCtrl+O");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_NEW, "&New Entry\tCtrl+Shift+N");
    menuFile->Append(ID_EDIT, "&Edit Entry\tCtrl+E");
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

void PasswordManagerFrame::OnNewDatabase(wxCommandEvent& event) {

    if (database && database->isUnlocked()) {
        if (wxMessageBox("Save current database before creating new?",
                        "Save?", wxYES_NO | wxICON_QUESTION) == wxYES) {
            database->save();
                        }
    }

    wxFileDialog dialog(this, "Create New Database", "", "new.pmdb",
                       "PMDB Files (*.pmdb)|*.pmdb|All Files (*.*)|*.*",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dialog.ShowModal() != wxID_OK) return;

    wxString password1 = wxGetPasswordFromUser(
        "Set master password:", "New Database", "", this);
    if (password1.empty()) return;

    wxString password2 = wxGetPasswordFromUser(
        "Confirm master password:", "New Database", "", this);
    if (password1 != password2) {
        wxMessageBox("Passwords don't match!", "Error");
        return;
    }

    database = std::make_unique<PasswordDatabase>(dialog.GetPath().ToStdString());
    if (database->createNewDatabase(password1.ToStdString())) {
        refreshList();
        SetStatusText("New database created! (locked)", 0);
        enableMenuItems(false);
    } else {
        wxMessageBox("Failed to create new database!", "Error");
    }
}

void PasswordManagerFrame::enableMenuItems(bool enabled) {
    wxMenuBar* menuBar = GetMenuBar();
    menuBar->Enable(wxID_NEW, enabled);
    menuBar->Enable(ID_EDIT, enabled);
    menuBar->Enable(ID_DELETE, enabled);
    menuBar->Enable(wxID_SAVE, enabled);
}

void PasswordManagerFrame::OnEditEntry(wxCommandEvent& event) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1 || !database || !database->isUnlocked()) {
        wxMessageBox("Please select an entry first!", "Error");
        return;
    }

    const auto& entries = database->getEntries();
    if (item >= (long)entries.size()) return;

    EditEntryDialog dialog(this, entries[item]);
    if (dialog.ShowModal() == wxID_OK) {
        PasswordEntry updatedEntry = dialog.getEntry();
        if (database->updateEntry(item, updatedEntry)) {  // ← EINFACH!
            refreshList();
            SetStatusText("Entry updated", 0);
        } else {
            wxMessageBox("Update failed!", "Error");
        }
    }
}

void PasswordManagerFrame::OnRightClick(wxListEvent& event) {
    long item = event.GetIndex();
    if (item == -1 || !database || !database->isUnlocked()) return;

    entryList->SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

    wxMenu menu;
    menu.Append(ID_EDIT, "Edit Entry");
    menu.Append(ID_DELETE, "Delete Entry");
    menu.AppendSeparator();
    menu.Append(ID_COPY_USERNAME, "Copy Username\tCtrl+B");
    menu.Append(ID_COPY_PASSWORD, "Copy Password\tCtrl+C");

    PopupMenu(&menu);
}

void PasswordManagerFrame::OnCopyUsername(wxCommandEvent& event) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1 || !database || !database->isUnlocked()) return;

    const auto& entries = database->getEntries();
    if (item < (long)entries.size()) {
        if (wxTheClipboard->Open()) {  // ← Open() statt OpenData()
            wxTheClipboard->SetData(new wxTextDataObject(entries[item].username));
            wxTheClipboard->Close();
            SetStatusText("Username copied - clears in 20s", 0);
            clipboardTimer->Start(20000, wxTIMER_ONE_SHOT);
        }
    }
}

void PasswordManagerFrame::OnCopyPassword(wxCommandEvent& event) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1 || !database || !database->isUnlocked()) return;

    const auto& entries = database->getEntries();
    if (item < (long)entries.size()) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->Clear(); // alten Inhalt erst mal weg
            wxTheClipboard->SetData(new wxTextDataObject(entries[item].password));
            wxTheClipboard->Close();
            SetStatusText("Password copied - clears in 20s", 0);
            clipboardTimer->Start(20000, wxTIMER_ONE_SHOT);
        }
    }
}

void PasswordManagerFrame::OnClipboardTimeout(wxTimerEvent& event) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->Clear();  // ← Clear() statt SetData(nullptr)
        wxTheClipboard->Close();
        SetStatusText("Clipboard cleared", 0);
    }
}

PasswordManagerFrame::~PasswordManagerFrame() {
    if (clipboardTimer) {
        clipboardTimer->Stop();
        delete clipboardTimer;
    }
}

wxIMPLEMENT_APP(PasswordManagerApp);
