#include <wx/wx.h>

class PasswordManagerApp : public wxApp {
public:
    bool OnInit() override;
};

class PasswordManagerFrame : public wxFrame {
public:
    PasswordManagerFrame() : wxFrame(NULL, wxID_ANY, "PassMan", wxDefaultPosition, wxSize(800, 600)) {

        wxMenu* menuFile = new wxMenu();
        menuFile->Append(wxID_NEW, wxT("New Database"));
        menuFile->Append(wxID_NEW, "&New Entry");
        menuFile->AppendSeparator();
        menuFile->Append(wxID_EXIT, "&Exit");

        wxMenu* menuEdit = new wxMenu();
        wxMenu* menuTools = new wxMenu();

        wxMenuBar* menuBar = new wxMenuBar();
        menuBar->Append(menuFile, "&File");
        menuBar->Append(menuEdit, "&Edit");
        menuBar->Append(menuTools, "&Tools");
        SetMenuBar(menuBar);

        CreateStatusBar(2);
        SetStatusText("Ready.");

        Bind(wxEVT_MENU, &PasswordManagerFrame::OnExit, this, wxID_EXIT);

        // Catch window close
        Bind(wxEVT_CLOSE_WINDOW, &PasswordManagerFrame::OnCloseWindow, this);
    }

private:
    void OnExit(wxCommandEvent& event) {
        Close();
    }

    void OnCloseWindow(wxCloseEvent& event) {
        int ans = wxMessageBox("Confirm Exit?", "Exit", wxYES_NO | wxCANCEL, this);

        if (ans == wxYES) {
            event.Skip();
            SetStatusText("Closing", 0);
        } else {
            event.Veto();
        }

    }

};

bool PasswordManagerApp::OnInit() {
    PasswordManagerFrame* frame = new PasswordManagerFrame();
    frame->Show(true);
    return true;
}

wxIMPLEMENT_APP(PasswordManagerApp);
