
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <wtsapi32.h>
#  pragma comment(lib, "wtsapi32.lib")
#else
#  include <sys/mman.h>       // mlockall
#endif

static constexpr int IDLE_LOCK_SECS     = 300;
static constexpr int CLIPBOARD_CLR_SECS = 20;


static void wipeWxString(wxString& s) {
    if (s.empty()) return;
    wxStringBuffer buf(s, s.length());
    memset(static_cast<wxChar*>(buf), 0, s.length() * sizeof(wxChar));
}

static void clearClipboard() {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->Clear();
        wxTheClipboard->Close();
    }
}

#ifdef _WIN32

static void markClipboardSensitive() {
    static UINT cfExclude = RegisterClipboardFormatW(
        L"ExcludeClipboardContentFromMonitorProcessing");
    static UINT cfCloud   = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
    static UINT cfHistory = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");

    if (cfExclude) SetClipboardData(cfExclude, nullptr);
    if (cfCloud)   SetClipboardData(cfCloud,   nullptr);
    if (cfHistory) SetClipboardData(cfHistory, nullptr);
}
#endif

class PasswordManagerApp : public wxApp {
public:
    bool OnInit() override;
};

class PasswordManagerFrame : public wxFrame {
public:
    PasswordManagerFrame();
    ~PasswordManagerFrame() override;

private:
    std::unique_ptr<PasswordDatabase> database;
    wxListCtrl* entryList;

    wxTimer* clipboardTimer;
    wxTimer* idleTimer;

    enum {
        ID_DELETE        = wxID_HIGHEST + 1,
        ID_LIST_SELECT,
        ID_NEW_DB,
        ID_EDIT          = wxID_HIGHEST + 4,
        ID_CONTEXT_MENU,
        ID_COPY_USERNAME,
        ID_COPY_PASSWORD,
        ID_CLIPBOARD_TIMER,
        ID_IDLE_TIMER,
    };

    void createMenu();
    void createUI();
    void refreshList();
    void enableMenuItems(bool enabled);
    void lockDatabase();

    void OnUnlock(wxCommandEvent&);
    void OnNewDatabase(wxCommandEvent&);
    void OnNewEntry(wxCommandEvent&);
    void OnDeleteEntry(wxCommandEvent&);
    void OnEditEntry(wxCommandEvent&);
    void OnSave(wxCommandEvent&);
    void OnExit(wxCommandEvent&);
    void OnEntrySelected(wxListEvent&);
    void OnCloseWindow(wxCloseEvent&);
    void OnRightClick(wxListEvent&);
    void OnCopyUsername(wxCommandEvent&);
    void OnCopyPassword(wxCommandEvent&);
    void OnClipboardTimeout(wxTimerEvent&);
    void OnIdleTimeout(wxTimerEvent&);

    void OnAnyInput(wxMouseEvent&);
    void OnAnyKey(wxKeyEvent&);

    void resetIdleTimer();

#ifdef _WIN32
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif
};

bool PasswordManagerApp::OnInit() {
#ifdef _WIN32
    SIZE_T minWS = 1 * 1024 * 1024;
    SIZE_T maxWS = 512 * 1024 * 1024;
    SetProcessWorkingSetSize(GetCurrentProcess(), minWS, maxWS);
#else
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        wxLogWarning("mlockall failed — process memory may be swapped to disk");
    }
#endif

    auto* frame = new PasswordManagerFrame();
    frame->Show(true);
    return true;
}

PasswordManagerFrame::PasswordManagerFrame()
    : wxFrame(nullptr, wxID_ANY, "PassMan", wxDefaultPosition, wxSize(1000, 700))
{
    createMenu();
    createUI();
    CreateStatusBar(2);
    SetStatusText("Please unlock database first", 0);

    clipboardTimer = new wxTimer(this, ID_CLIPBOARD_TIMER);
    idleTimer      = new wxTimer(this, ID_IDLE_TIMER);

    Bind(wxEVT_MENU, &PasswordManagerFrame::OnNewDatabase,  this, ID_NEW_DB);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnUnlock,       this, wxID_OPEN);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnNewEntry,     this, wxID_NEW);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnDeleteEntry,  this, ID_DELETE);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnSave,         this, wxID_SAVE);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnExit,         this, wxID_EXIT);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnEditEntry,    this, ID_EDIT);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnCopyUsername, this, ID_COPY_USERNAME);
    Bind(wxEVT_MENU, &PasswordManagerFrame::OnCopyPassword, this, ID_COPY_PASSWORD);

    Bind(wxEVT_LIST_ITEM_SELECTED,  &PasswordManagerFrame::OnEntrySelected, this, ID_LIST_SELECT);
    Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &PasswordManagerFrame::OnRightClick,  this, ID_LIST_SELECT);
    Bind(wxEVT_LIST_ITEM_ACTIVATED, &PasswordManagerFrame::OnEditEntry,     this, ID_LIST_SELECT);

    Bind(wxEVT_CLOSE_WINDOW, &PasswordManagerFrame::OnCloseWindow,    this);
    Bind(wxEVT_TIMER,        &PasswordManagerFrame::OnClipboardTimeout, this, ID_CLIPBOARD_TIMER);
    Bind(wxEVT_TIMER,        &PasswordManagerFrame::OnIdleTimeout,      this, ID_IDLE_TIMER);

    Bind(wxEVT_MOTION,    &PasswordManagerFrame::OnAnyInput, this);
    Bind(wxEVT_LEFT_DOWN, &PasswordManagerFrame::OnAnyInput, this);
    Bind(wxEVT_KEY_DOWN,  &PasswordManagerFrame::OnAnyKey,   this);

    wxAcceleratorEntry entries[2];
    entries[0].Set(wxACCEL_CTRL, (int)'B', ID_COPY_USERNAME);
    entries[1].Set(wxACCEL_CTRL, (int)'C', ID_COPY_PASSWORD);
    SetAcceleratorTable(wxAcceleratorTable(2, entries));

    enableMenuItems(false);

#ifdef _WIN32
    WTSRegisterSessionNotification(GetHWND(), NOTIFY_FOR_THIS_SESSION);
#endif
}

PasswordManagerFrame::~PasswordManagerFrame() {
    if (clipboardTimer) { clipboardTimer->Stop(); delete clipboardTimer; }
    if (idleTimer)      { idleTimer->Stop();      delete idleTimer;      }
#ifdef _WIN32
    WTSUnRegisterSessionNotification(GetHWND());
#endif
}

void PasswordManagerFrame::lockDatabase() {
    if (!database) return;
    database->save();
    database->lock();
    clearClipboard();
    clipboardTimer->Stop();
    idleTimer->Stop();
    refreshList();
    enableMenuItems(false);
    SetStatusText("Database locked", 0);
}

void PasswordManagerFrame::resetIdleTimer() {
    if (database && database->isUnlocked())
        idleTimer->Start(IDLE_LOCK_SECS * 1000, wxTIMER_ONE_SHOT);
}

#ifdef _WIN32
WXLRESULT PasswordManagerFrame::MSWWindowProc(WXUINT msg,
                                               WXWPARAM wParam,
                                               WXLPARAM lParam) {
    if (msg == WM_WTSSESSION_CHANGE) {
        if (wParam == WTS_SESSION_LOCK      ||
            wParam == WTS_REMOTE_DISCONNECT  ||
            wParam == WTS_SESSION_LOGOFF) {
            lockDatabase();
        }
    }
    return wxFrame::MSWWindowProc(msg, wParam, lParam);
}
#endif

void PasswordManagerFrame::OnAnyInput(wxMouseEvent& e) {
    resetIdleTimer(); e.Skip();
}
void PasswordManagerFrame::OnAnyKey(wxKeyEvent& e) {
    resetIdleTimer(); e.Skip();
}
void PasswordManagerFrame::OnIdleTimeout(wxTimerEvent&) {
    if (database && database->isUnlocked()) {
        SetStatusText("Auto-locked after inactivity", 0);
        lockDatabase();
    }
}

void PasswordManagerFrame::createMenu() {
    auto* menuFile = new wxMenu();
    menuFile->Append(ID_NEW_DB,  "&New Database...\tCtrl+N");
    menuFile->Append(wxID_OPEN,  "&Open Database...\tCtrl+O");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_NEW,   "&New Entry\tCtrl+Shift+N");
    menuFile->Append(ID_EDIT,    "&Edit Entry\tCtrl+E");
    menuFile->Append(ID_DELETE,  "&Delete Entry\tDel");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_SAVE,  "&Save\tCtrl+S");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT,  "&Exit\tCtrl+Q");

    auto* menuBar = new wxMenuBar();
    menuBar->Append(menuFile, "&File");
    SetMenuBar(menuBar);
}

void PasswordManagerFrame::createUI() {
    auto* panel = new wxPanel(this);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    entryList = new wxListCtrl(panel, ID_LIST_SELECT, wxDefaultPosition,
                                wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    entryList->InsertColumn(0, "Title",    wxLIST_FORMAT_LEFT, 250);
    entryList->InsertColumn(1, "Username", wxLIST_FORMAT_LEFT, 180);
    entryList->InsertColumn(2, "URL",      wxLIST_FORMAT_LEFT, 250);
    entryList->InsertColumn(3, "Modified", wxLIST_FORMAT_LEFT, 150);
    sizer->Add(entryList, 1, wxEXPAND | wxALL, 10);
    panel->SetSizer(sizer);
}

void PasswordManagerFrame::refreshList() {
    entryList->DeleteAllItems();
    if (!database || !database->isUnlocked()) return;
    for (size_t i = 0; i < database->size(); ++i) {
        const auto& e = database->getEntries()[i];
        long id = entryList->InsertItem(i, e.title);
        entryList->SetItem(id, 1, e.username);
        entryList->SetItem(id, 2, e.url);
        wxDateTime t(static_cast<time_t>(e.modified));
        entryList->SetItem(id, 3, t.Format("%d.%m.%Y %H:%M"));
    }
}

void PasswordManagerFrame::enableMenuItems(bool on) {
    auto* mb = GetMenuBar();
    mb->Enable(wxID_NEW,  on);
    mb->Enable(ID_EDIT,   on);
    mb->Enable(ID_DELETE, on);
    mb->Enable(wxID_SAVE, on);
}

void PasswordManagerFrame::OnUnlock(wxCommandEvent&) {
    wxFileDialog dlg(this, "Open Password Database", "", "passwords.pmdb",
                     "PMDB Files (*.pmdb)|*.pmdb|All Files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    SecurePasswordDialog pwdDlg(this, "Enter master password:", "Unlock Database");
    if (pwdDlg.ShowModal() != wxID_OK) return;

    std::string pwd(pwdDlg.getPassword().begin(), pwdDlg.getPassword().end());

    database = std::make_unique<PasswordDatabase>(dlg.GetPath().ToStdString());

    if (database->unlock(pwd)) {
        sodium_memzero(&pwd[0], pwd.size());
        refreshList();
        SetStatusText(
            wxString::Format("Unlocked — %zu entries", database->size()), 0);
        SetTitle(wxString::Format("PassMan — %s",
            wxFileName(dlg.GetPath()).GetFullName()));
        enableMenuItems(true);
        resetIdleTimer();
    } else {
        sodium_memzero(&pwd[0], pwd.size());
        wxMessageBox("Wrong password or corrupted file.", "Error",
                     wxOK | wxICON_ERROR);
        database.reset();
    }
}

void PasswordManagerFrame::OnNewDatabase(wxCommandEvent&) {
    if (database && database->isUnlocked()) {
        if (wxMessageBox("Save current database before creating new?",
                         "Save?", wxYES_NO | wxICON_QUESTION) == wxYES)
            database->save();
        lockDatabase();
    }

    wxFileDialog dlg(this, "Create New Database", "", "new.pmdb",
                     "PMDB Files (*.pmdb)|*.pmdb|All Files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    SecurePasswordDialog pwdDlg(this, "Set master password:", "New Database",
                                 true /* showConfirm */);
    if (pwdDlg.ShowModal() != wxID_OK) return;

    std::string pwd(pwdDlg.getPassword().begin(), pwdDlg.getPassword().end());

    database = std::make_unique<PasswordDatabase>(dlg.GetPath().ToStdString());
    bool created = database->createNewDatabase(pwd);

    if (created && database->unlock(pwd)) {
        sodium_memzero(&pwd[0], pwd.size());

        PasswordEntry ex1;
        ex1.title = "Example: GitHub"; ex1.username = "user@example.com";
        ex1.password = "SuperSecret123!"; ex1.url = "https://github.com";
        ex1.notes = "Example entry — feel free to delete.";
        database->addEntry(ex1);

        PasswordEntry ex2;
        ex2.title = "Example: Email"; ex2.username = "user@example.com";
        ex2.password = "MyEmailPass456!"; ex2.url = "https://mail.example.com";
        ex2.notes = "Another example entry.";
        database->addEntry(ex2);

        database->save();
        refreshList();
        SetTitle(wxString::Format("PassMan — %s",
            wxFileName(dlg.GetPath()).GetFullName()));
        SetStatusText(wxString::Format("Created — %zu entries", database->size()), 0);
        enableMenuItems(true);
        resetIdleTimer();
    } else {
        sodium_memzero(&pwd[0], pwd.size());
        wxMessageBox("Failed to create database!", "Error", wxOK | wxICON_ERROR);
        database.reset();
    }
}

void PasswordManagerFrame::OnNewEntry(wxCommandEvent&) {
    if (!database || !database->isUnlocked()) return;
    resetIdleTimer();
    NewEntryDialog dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        database->addEntry(dlg.getEntry());
        refreshList();
        SetStatusText("Entry added", 0);
    }
}

void PasswordManagerFrame::OnEditEntry(wxCommandEvent&) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item < 0 || !database || !database->isUnlocked()) return;
    resetIdleTimer();
    if (static_cast<size_t>(item) >= database->size()) return;
    EditEntryDialog dlg(this, database->getEntries()[item]);
    if (dlg.ShowModal() == wxID_OK) {
        database->updateEntry(item, dlg.getEntry());
        refreshList();
        SetStatusText("Entry updated", 0);
    }
}

void PasswordManagerFrame::OnDeleteEntry(wxCommandEvent&) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item < 0 || !database || !database->isUnlocked()) return;
    resetIdleTimer();
    if (wxMessageBox("Delete this entry?", "Confirm",
                     wxYES_NO | wxICON_QUESTION) != wxYES) return;
    database->deleteEntry(item);
    refreshList();
    SetStatusText("Entry deleted", 0);
}

void PasswordManagerFrame::OnSave(wxCommandEvent&) {
    if (!database || !database->isUnlocked()) return;
    resetIdleTimer();
    if (database->save()) SetStatusText("Saved", 0);
    else wxMessageBox("Save failed!", "Error", wxOK | wxICON_ERROR);
}

void PasswordManagerFrame::OnExit(wxCommandEvent&) { Close(); }

void PasswordManagerFrame::OnCloseWindow(wxCloseEvent& e) {
    if (database && database->isUnlocked()) database->save();
    clearClipboard();
    e.Skip();
}

void PasswordManagerFrame::OnEntrySelected(wxListEvent&) { enableMenuItems(true); }

void PasswordManagerFrame::OnCopyUsername(wxCommandEvent&) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item < 0 || !database || !database->isUnlocked()) return;
    resetIdleTimer();
    if (static_cast<size_t>(item) >= database->size()) return;
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(
            new wxTextDataObject(database->getEntries()[item].username));
#ifdef _WIN32
        markClipboardSensitive();
#endif
        wxTheClipboard->Close();
        SetStatusText("Username copied — clears in 20 s", 0);
        clipboardTimer->Start(CLIPBOARD_CLR_SECS * 1000, wxTIMER_ONE_SHOT);
    }
}

void PasswordManagerFrame::OnCopyPassword(wxCommandEvent&) {
    long item = entryList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item < 0 || !database || !database->isUnlocked()) return;
    resetIdleTimer();
    if (static_cast<size_t>(item) >= database->size()) return;
    if (wxTheClipboard->Open()) {
        wxTheClipboard->Clear();
        wxTheClipboard->SetData(
            new wxTextDataObject(database->getEntries()[item].password));
#ifdef _WIN32
        markClipboardSensitive();
#endif
        wxTheClipboard->Close();
        SetStatusText("Password copied — clears in 20 s", 0);
        clipboardTimer->Start(CLIPBOARD_CLR_SECS * 1000, wxTIMER_ONE_SHOT);
    }
}

void PasswordManagerFrame::OnClipboardTimeout(wxTimerEvent&) {
    clearClipboard();
    SetStatusText("Clipboard cleared", 0);
}

void PasswordManagerFrame::OnRightClick(wxListEvent& e) {
    long item = e.GetIndex();
    if (item < 0 || !database || !database->isUnlocked()) return;
    entryList->SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    wxMenu menu;
    menu.Append(ID_EDIT,          "Edit Entry");
    menu.Append(ID_DELETE,        "Delete Entry");
    menu.AppendSeparator();
    menu.Append(ID_COPY_USERNAME, "Copy Username\tCtrl+B");
    menu.Append(ID_COPY_PASSWORD, "Copy Password\tCtrl+C");
    PopupMenu(&menu);
}

wxIMPLEMENT_APP(PasswordManagerApp);