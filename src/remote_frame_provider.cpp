#include "remote_frame_provider.h"

#include <algorithm>
#include <cstdlib>
#include <chrono>

#include <wx/app.h>
#include <wx/dcclient.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/wx.h>

#include "ocpn_plugin.h"

namespace {

constexpr const char* kLocalClientId = "__opencpn_local__";

long long NowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

void RemoteFrameProvider::SetToken(std::string token) {
  std::lock_guard<std::mutex> lock(mutex_);
  token_ = std::move(token);
}

bool RemoteFrameProvider::TokenMatches(const std::string& token) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !token_.empty() && token == token_;
}

std::string RemoteFrameProvider::GetToken() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return token_;
}

void RemoteFrameProvider::SetPassword(std::string password) {
  std::lock_guard<std::mutex> lock(mutex_);
  password_ = std::move(password);
}

bool RemoteFrameProvider::PasswordRequired() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !password_.empty();
}

wxWindow* RemoteFrameProvider::GetTargetWindow() const {
  wxWindow* canvas = GetOCPNCanvasWindow();
  if (canvas) return canvas;
  return wxTheApp ? wxTheApp->GetTopWindow() : nullptr;
}

void RemoteFrameProvider::CaptureOpenCpnWindow() {
  wxWindow* window = GetTargetWindow();
  if (!window || !window->IsShownOnScreen()) return;

  wxSize size = window->GetClientSize();
  if (size.x <= 0 || size.y <= 0) return;

  wxBitmap bmp(size.x, size.y, 24);
  wxMemoryDC memdc(bmp);
  wxWindowDC windc(window);
  memdc.Blit(0, 0, size.x, size.y, &windc, 0, 0);
  memdc.SelectObject(wxNullBitmap);

  wxImage image = bmp.ConvertToImage();
  image.SetOption(wxIMAGE_OPTION_QUALITY, 72);
  wxMemoryOutputStream stream;
  if (!image.SaveFile(stream, wxBITMAP_TYPE_JPEG)) return;

  const size_t len = stream.GetSize();
  std::vector<unsigned char> bytes(len);
  stream.CopyTo(bytes.data(), len);

  std::lock_guard<std::mutex> lock(mutex_);
  jpeg_ = std::move(bytes);
  width_ = size.x;
  height_ = size.y;
  ++sequence_;
}

void RemoteFrameProvider::ProcessPendingInput() {
  std::deque<QueuedInput> inputs;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    inputs.swap(input_queue_);
  }

  for (const auto& input : inputs) {
    if (input.kind == QueuedInput::Kind::Pointer) {
      PostMouse(input);
    } else {
      PostKey(input);
    }
  }
}

std::vector<unsigned char> RemoteFrameProvider::GetJpegFrame(
    int* width, int* height, unsigned long* sequence) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (width) *width = width_;
  if (height) *height = height_;
  if (sequence) *sequence = sequence_;
  return jpeg_;
}

void RemoteFrameProvider::HandlePointerEvent(const std::string& body) {
  QueuedInput input;
  input.kind = QueuedInput::Kind::Pointer;
  input.type = ExtractString(body, "type");
  input.point = ExtractPoint(body);
  input.wheel_delta = ExtractInt(body, "delta", 0);

  std::lock_guard<std::mutex> lock(mutex_);
  if (input.type == "move" && !input_queue_.empty()) {
    QueuedInput& back = input_queue_.back();
    if (back.kind == QueuedInput::Kind::Pointer && back.type == "move") {
      back = input;
      return;
    }
  }
  if (input_queue_.size() < 512) input_queue_.push_back(input);
}

void RemoteFrameProvider::HandleKeyEvent(const std::string& body) {
  QueuedInput input;
  input.kind = QueuedInput::Kind::Key;
  input.type = ExtractString(body, "type");
  input.key_code = ExtractInt(body, "keyCode", 0);

  std::lock_guard<std::mutex> lock(mutex_);
  if (input_queue_.size() < 512) input_queue_.push_back(input);
}

RemoteFrameProvider::SessionResult RemoteFrameProvider::HandleSessionEvent(
    const std::string& body, const std::string& client_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const long long now = NowMs();
  const bool claim = ExtractBool(body, "claim", false);
  if (!password_.empty() && ExtractString(body, "password") != password_) {
    return SessionResult::BadPassword;
  }
  const bool active_remote =
      !remote_client_id_.empty() &&
      (remote_client_id_ == kLocalClientId || now - remote_seen_ms_ < 3500);
  if (active_remote && remote_client_id_ != client_id && !claim) {
    return SessionResult::Busy;
  }
  remote_client_id_ = client_id;
  remote_seen_ms_ = now;
  remote_width_ = ExtractInt(body, "width", remote_width_);
  remote_height_ = ExtractInt(body, "height", remote_height_);
  return SessionResult::Accepted;
}

void RemoteFrameProvider::ClaimLocalControl() {
  std::lock_guard<std::mutex> lock(mutex_);
  remote_client_id_ = kLocalClientId;
  remote_seen_ms_ = NowMs();
  input_queue_.clear();
}

std::string RemoteFrameProvider::ActiveClientId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (remote_client_id_ == kLocalClientId) return remote_client_id_;
  if (remote_seen_ms_ <= 0 || NowMs() - remote_seen_ms_ >= 3500) return "";
  return remote_client_id_;
}

bool RemoteFrameProvider::ClientAllowed(const std::string& client_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (remote_client_id_ == kLocalClientId) return false;
  return remote_client_id_.empty() || remote_client_id_ == client_id ||
         NowMs() - remote_seen_ms_ >= 3500;
}

bool RemoteFrameProvider::IsRemoteActive() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (remote_client_id_ == kLocalClientId) return false;
  return remote_seen_ms_ > 0 && NowMs() - remote_seen_ms_ < 3500;
}

wxSize RemoteFrameProvider::GetRemoteViewport() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return wxSize(remote_width_, remote_height_);
}

wxPoint RemoteFrameProvider::ExtractPoint(const std::string& body) const {
  return wxPoint(ExtractInt(body, "x", 0), ExtractInt(body, "y", 0));
}

std::string RemoteFrameProvider::ExtractString(const std::string& body,
                                               const std::string& key) const {
  const std::string needle = "\"" + key + "\"";
  size_t pos = body.find(needle);
  if (pos == std::string::npos) return "";
  pos = body.find(':', pos);
  if (pos == std::string::npos) return "";
  pos = body.find('"', pos);
  if (pos == std::string::npos) return "";
  const size_t end = body.find('"', pos + 1);
  if (end == std::string::npos) return "";
  return body.substr(pos + 1, end - pos - 1);
}

int RemoteFrameProvider::ExtractInt(const std::string& body,
                                    const std::string& key,
                                    int fallback) const {
  const std::string needle = "\"" + key + "\"";
  size_t pos = body.find(needle);
  if (pos == std::string::npos) return fallback;
  pos = body.find(':', pos);
  if (pos == std::string::npos) return fallback;
  ++pos;
  while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;
  return std::atoi(body.c_str() + pos);
}

bool RemoteFrameProvider::ExtractBool(const std::string& body,
                                      const std::string& key,
                                      bool fallback) const {
  const std::string needle = "\"" + key + "\"";
  size_t pos = body.find(needle);
  if (pos == std::string::npos) return fallback;
  pos = body.find(':', pos);
  if (pos == std::string::npos) return fallback;
  ++pos;
  while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;
  if (body.compare(pos, 4, "true") == 0) return true;
  if (body.compare(pos, 5, "false") == 0) return false;
  return fallback;
}

void RemoteFrameProvider::PostMouse(const QueuedInput& input) {
  wxWindow* target = GetTargetWindow();
  if (!target) return;

  wxEventType event_type = wxEVT_MOTION;
  if (input.type == "down") event_type = wxEVT_LEFT_DOWN;
  if (input.type == "up") event_type = wxEVT_LEFT_UP;
  if (input.type == "move") event_type = wxEVT_MOTION;
  if (input.type == "wheel") event_type = wxEVT_MOUSEWHEEL;

  wxMouseEvent event(event_type);
  event.SetEventObject(target);
  event.m_x = std::clamp(input.point.x, 0, std::max(0, target->GetClientSize().x - 1));
  event.m_y = std::clamp(input.point.y, 0, std::max(0, target->GetClientSize().y - 1));
  event.m_leftDown = input.type == "down" || input.type == "move";
  if (event_type == wxEVT_MOUSEWHEEL) {
    event.m_wheelRotation = input.wheel_delta;
  }
  wxPostEvent(target, event);
}

void RemoteFrameProvider::PostKey(const QueuedInput& input) {
  wxWindow* target = GetTargetWindow();
  if (!target) return;

  wxKeyEvent event(input.type == "keyup" ? wxEVT_KEY_UP : wxEVT_KEY_DOWN);
  event.SetEventObject(target);
  event.m_keyCode = input.key_code;
  wxPostEvent(target, event);
}
