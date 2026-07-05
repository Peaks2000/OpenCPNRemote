#pragma once

#include <memory>

#include <wx/bitmap.h>
#include <wx/gdicmn.h>

#include "http_server.h"
#include "ocpn_plugin.h"
#include "remote_frame_provider.h"

class RemoteCaptureTimer;
class RemoteLockDialog;

class OpenCpnRemotePi : public opencpn_plugin_117 {
public:
  explicit OpenCpnRemotePi(void* ppimgr);
  ~OpenCpnRemotePi() override;

  int Init() override;
  bool DeInit() override;

  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetPlugInVersionPatch() override;
  int GetPlugInVersionPost() override;
  const char* GetPlugInVersionPre() override;
  const char* GetPlugInVersionBuild() override;

  wxBitmap* GetPlugInBitmap() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;
  void ShowPreferencesDialog(wxWindow* parent) override;

private:
  friend class RemoteCaptureTimer;
  friend class RemoteLockDialog;
  void CaptureFrame();
  void UpdateRemoteLock();
  void ApplyLocalInputLock(bool locked);
  void UnlockLocalControl();
  void RestoreLocalWindow();
  void LoadConfig();
  void SaveConfig();

  wxBitmap bitmap_;
  std::shared_ptr<RemoteFrameProvider> frames_;
  std::unique_ptr<HttpServer> server_;
  std::unique_ptr<RemoteCaptureTimer> timer_;
  std::unique_ptr<RemoteLockDialog> lock_dialog_;
  std::string password_;
  bool input_lock_applied_ = false;
  bool local_unlocked_ = false;
  bool pause_window_suppressed_ = false;
  std::string last_active_client_;
};
