#include "opencpn_remote_pi.h"

#include <wx/log.h>
#include <wx/button.h>
#include <wx/fileconf.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/toplevel.h>
#include <wx/wx.h>

#include <random>
#include <sstream>

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new OpenCpnRemotePi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

namespace {

std::string MakeToken() {
  std::random_device rd;
  std::uniform_int_distribution<unsigned int> dist(0, 15);
  std::ostringstream out;
  for (int i = 0; i < 32; ++i) out << std::hex << dist(rd);
  return out.str();
}

void WriteAccessUrlFile(int port, const std::string& token) {
  wxString dir = wxStandardPaths::Get().GetUserDataDir();
  if (!wxFileName::DirExists(dir)) wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

  wxFileName path(dir, "opencpn_remote_url.txt");
  wxString text;
  text.Printf("On this computer:\nhttp://127.0.0.1:%d/?token=%s\n\n"
              "From another device on the same network, replace 127.0.0.1 "
              "with this computer's LAN IP address.\n",
              port, token.c_str());
  wxFile file(path.GetFullPath(), wxFile::write);
  if (file.IsOpened()) {
    file.Write(text);
    wxLogMessage("OpenCPN Remote access URL saved to %s", path.GetFullPath());
  }
}

wxBitmap MakePluginBitmap() {
  wxBitmap bmp(32, 32);
  wxMemoryDC dc(bmp);
  dc.SetBackground(wxBrush(wxColour(20, 95, 130)));
  dc.Clear();
  dc.SetPen(wxPen(*wxWHITE, 2));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawRoundedRectangle(5, 7, 22, 16, 3);
  dc.DrawLine(11, 26, 21, 26);
  dc.DrawLine(16, 23, 16, 26);
  dc.SelectObject(wxNullBitmap);
  return bmp;
}

}  // namespace

class RemoteCaptureTimer : public wxTimer {
public:
  explicit RemoteCaptureTimer(OpenCpnRemotePi* plugin) : plugin_(plugin) {}
  void Notify() override {
    if (plugin_) plugin_->CaptureFrame();
  }

private:
  OpenCpnRemotePi* plugin_;
};

class RemoteLockDialog : public wxFrame {
public:
  RemoteLockDialog(wxWindow* parent, OpenCpnRemotePi* plugin)
      : wxFrame(nullptr, wxID_ANY, "OpenCPN Remote",
                 wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT),
        plugin_(plugin) {
    SetBackgroundColour(wxColour(16, 20, 24));

    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->AddStretchSpacer(1);

    auto* title = new wxStaticText(this, wxID_ANY, "OpenCPN window paused");
    title->SetForegroundColour(*wxWHITE);
    auto font = title->GetFont();
    font.SetPointSize(font.GetPointSize() + 5);
    font.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(font);
    outer->Add(title, 0, wxALIGN_CENTER | wxBOTTOM, 10);

    auto* text = new wxStaticText(
        this, wxID_ANY,
        "A web remote is connected. Resume here to use this OpenCPN window.");
    text->SetForegroundColour(wxColour(210, 218, 224));
    outer->Add(text, 0, wxALIGN_CENTER | wxBOTTOM, 18);

    auto* button = new wxButton(this, wxID_ANY, "Resume");
    button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
      if (plugin_) plugin_->UnlockLocalControl();
    });
    auto* minimize = new wxButton(this, wxID_ANY, "Minimize");
    minimize->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
      if (plugin_) plugin_->pause_window_suppressed_ = true;
      auto* top = wxDynamicCast(wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
                                wxTopLevelWindow);
      if (top) top->Iconize(true);
      Iconize(true);
    });
    Bind(wxEVT_ICONIZE, [this](wxIconizeEvent& event) {
      if (event.IsIconized() && plugin_) {
        plugin_->pause_window_suppressed_ = true;
        auto* top = wxDynamicCast(wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
                                  wxTopLevelWindow);
        if (top) top->Iconize(true);
      }
      event.Skip();
    });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
      if (plugin_) plugin_->pause_window_suppressed_ = true;
      Hide();
      event.Veto();
    });
    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->Add(button, 0, wxRIGHT, 8);
    buttons->Add(minimize, 0);
    outer->Add(buttons, 0, wxALIGN_CENTER);
    outer->AddStretchSpacer(1);

    SetSizer(outer);
  }

  void FitOverParent() {
    if (IsIconized()) return;
    wxWindow* top = wxTheApp ? wxTheApp->GetTopWindow() : nullptr;
    if (top) {
      SetSize(top->GetScreenRect());
    }
    Raise();
  }

private:
  OpenCpnRemotePi* plugin_;
};

OpenCpnRemotePi::OpenCpnRemotePi(void* ppimgr)
    : opencpn_plugin_117(ppimgr), frames_(std::make_shared<RemoteFrameProvider>()) {
  bitmap_ = MakePluginBitmap();
  timer_ = std::make_unique<RemoteCaptureTimer>(this);
}

OpenCpnRemotePi::~OpenCpnRemotePi() { DeInit(); }

int OpenCpnRemotePi::Init() {
  if (server_) {
    wxLogMessage("OpenCPN Remote web server is already running from the plugin");
    return WANTS_CONFIG;
  }

  const int port = 8765;
  const std::string token = MakeToken();
  LoadConfig();
  frames_->SetToken(token);
  frames_->SetPassword(password_);

  server_ = std::make_unique<HttpServer>(frames_, port);
  if (!server_->Start()) {
    wxLogError("OpenCPN Remote failed to start on port %d", port);
    server_.reset();
    return WANTS_CONFIG;
  }

  timer_->Start(33, wxTIMER_CONTINUOUS);
  WriteAccessUrlFile(port, token);
  wxLogMessage("OpenCPN Remote web server started by OpenCPN plugin lifecycle");
  wxLogMessage("OpenCPN Remote listening on http://0.0.0.0:%d/?token=%s",
               port, token.c_str());
  return WANTS_CONFIG | WANTS_PREFERENCES;
}

bool OpenCpnRemotePi::DeInit() {
  if (timer_ && timer_->IsRunning()) timer_->Stop();
  RestoreLocalWindow();
  lock_dialog_.reset();
  if (server_) {
    wxLogMessage("OpenCPN Remote web server stopping from plugin DeInit");
    server_->Stop();
    server_.reset();
  }
  return true;
}

int OpenCpnRemotePi::GetAPIVersionMajor() { return API_VERSION_MAJOR; }
int OpenCpnRemotePi::GetAPIVersionMinor() { return API_VERSION_MINOR; }
int OpenCpnRemotePi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }
int OpenCpnRemotePi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }
int OpenCpnRemotePi::GetPlugInVersionPatch() { return PLUGIN_VERSION_PATCH; }
int OpenCpnRemotePi::GetPlugInVersionPost() { return 0; }
const char* OpenCpnRemotePi::GetPlugInVersionPre() { return ""; }
const char* OpenCpnRemotePi::GetPlugInVersionBuild() { return ""; }

wxBitmap* OpenCpnRemotePi::GetPlugInBitmap() { return &bitmap_; }
wxString OpenCpnRemotePi::GetCommonName() { return "OpenCPN Remote"; }
wxString OpenCpnRemotePi::GetShortDescription() {
  return "Hosts OpenCPN-only remote access in a web browser.";
}
wxString OpenCpnRemotePi::GetLongDescription() {
  return "Streams only the OpenCPN application window to a responsive web "
         "client and forwards browser input back to OpenCPN.";
}

void OpenCpnRemotePi::ShowPreferencesDialog(wxWindow* parent) {
  wxDialog dialog(parent, wxID_ANY, "OpenCPN Remote Settings",
                  wxDefaultPosition, wxDefaultSize,
                  wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto* sizer = new wxBoxSizer(wxVERTICAL);
  auto* label = new wxStaticText(
      &dialog, wxID_ANY,
      "Password required before a browser can control or pause OpenCPN.");
  sizer->Add(label, 0, wxALL, 10);
  auto* password = new wxTextCtrl(&dialog, wxID_ANY, password_,
                                  wxDefaultPosition, wxSize(280, -1),
                                  wxTE_PASSWORD);
  sizer->Add(password, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  auto* buttons = dialog.CreateSeparatedButtonSizer(wxOK | wxCANCEL);
  sizer->Add(buttons, 0, wxALL | wxEXPAND, 10);
  dialog.SetSizerAndFit(sizer);

  if (dialog.ShowModal() == wxID_OK) {
    password_ = password->GetValue().ToStdString();
    frames_->SetPassword(password_);
    SaveConfig();
  }
}

void OpenCpnRemotePi::CaptureFrame() {
  frames_->ProcessPendingInput();
  UpdateRemoteLock();
  frames_->CaptureOpenCpnWindow();
}

void OpenCpnRemotePi::UpdateRemoteLock() {
  RestoreLocalWindow();
}

void OpenCpnRemotePi::ApplyLocalInputLock(bool locked) {
  input_lock_applied_ = locked;
}

void OpenCpnRemotePi::UnlockLocalControl() {
  frames_->ClaimLocalControl();
  last_active_client_ = frames_->ActiveClientId();
  local_unlocked_ = true;
  pause_window_suppressed_ = false;
  RestoreLocalWindow();
}

void OpenCpnRemotePi::RestoreLocalWindow() {
  if (lock_dialog_ && lock_dialog_->IsShown()) lock_dialog_->Hide();
  ApplyLocalInputLock(false);
}

void OpenCpnRemotePi::LoadConfig() {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;
  wxString password;
  config->SetPath("/PlugIns/OpenCPNRemote");
  config->Read("Password", &password, "");
  password_ = password.ToStdString();
}

void OpenCpnRemotePi::SaveConfig() {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;
  config->SetPath("/PlugIns/OpenCPNRemote");
  config->Write("Password", wxString(password_));
  config->Flush();
}
