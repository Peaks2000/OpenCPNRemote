#pragma once

#include <mutex>
#include <deque>
#include <string>
#include <vector>

#include <wx/window.h>
#include <wx/gdicmn.h>

class RemoteFrameProvider {
public:
  enum class SessionResult { Accepted, BadPassword, Busy };

  void SetToken(std::string token);
  std::string GetToken() const;
  bool TokenMatches(const std::string& token) const;
  void SetPassword(std::string password);
  bool PasswordRequired() const;

  void CaptureOpenCpnWindow();
  void ProcessPendingInput();
  std::vector<unsigned char> GetJpegFrame(int* width, int* height,
                                          unsigned long* sequence) const;
  void HandlePointerEvent(const std::string& body);
  void HandleKeyEvent(const std::string& body);
  SessionResult HandleSessionEvent(const std::string& body,
                                   const std::string& client_id);
  void ClaimLocalControl();
  bool ClientAllowed(const std::string& client_id) const;
  std::string ActiveClientId() const;
  bool IsRemoteActive() const;
  wxSize GetRemoteViewport() const;

private:
  struct QueuedInput {
    enum class Kind { Pointer, Key };
    Kind kind;
    std::string type;
    wxPoint point;
    int key_code = 0;
    int wheel_delta = 0;
    int sequence = 0;
  };

  struct CaptureRegion {
    wxWindow* window = nullptr;
    wxRect screen_rect;
    wxPoint frame_origin;
  };

  wxWindow* GetTargetWindow() const;
  std::vector<wxWindow*> GetOpenCpnWindows() const;
  wxWindow* WindowForFramePoint(const wxPoint& point, wxPoint* local_point) const;
  wxWindow* FindTapTarget(const wxPoint& point, wxPoint* local_point) const;
  wxPoint ExtractPoint(const std::string& body) const;
  std::string ExtractString(const std::string& body,
                            const std::string& key) const;
  bool ExtractBool(const std::string& body, const std::string& key,
                   bool fallback) const;
  int ExtractInt(const std::string& body, const std::string& key,
                 int fallback) const;
  void PostMouse(const QueuedInput& input);
  void PostKey(const QueuedInput& input);

  mutable std::mutex mutex_;
  std::string token_;
  std::string password_;
  std::vector<unsigned char> jpeg_;
  std::deque<QueuedInput> input_queue_;
  long long remote_seen_ms_ = 0;
  std::string remote_client_id_;
  int remote_width_ = 0;
  int remote_height_ = 0;
  int width_ = 0;
  int height_ = 0;
  unsigned long sequence_ = 0;
  int last_pointer_sequence_ = 0;
  bool remote_mouse_down_ = false;
  std::vector<CaptureRegion> capture_regions_;
};
