#include "http_server.h"

#include <cstring>
#include <sstream>
#include <vector>
#include <string>

#include "web_assets.h"

#ifdef _WIN32
#include <winsock2.h>
#define CLOSESOCKET closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSESOCKET close
#endif

namespace {

bool SendAll(int fd, const char* data, size_t len) {
  while (len > 0) {
#ifdef _WIN32
    const int sent = send(fd, data, static_cast<int>(len), 0);
#else
    const ssize_t sent = send(fd, data, len, 0);
#endif
    if (sent <= 0) return false;
    data += sent;
    len -= static_cast<size_t>(sent);
  }
  return true;
}

std::string RequestPath(const std::string& request) {
  const size_t first_space = request.find(' ');
  if (first_space == std::string::npos) return "/";
  const size_t second_space = request.find(' ', first_space + 1);
  if (second_space == std::string::npos) return "/";
  return request.substr(first_space + 1, second_space - first_space - 1);
}

std::string RequestBody(const std::string& request) {
  const size_t split = request.find("\r\n\r\n");
  if (split == std::string::npos) return "";
  return request.substr(split + 4);
}

std::string RequestParam(const std::string& path, const std::string& key) {
  const std::string needle = key + "=";
  const size_t pos = path.find(needle);
  if (pos == std::string::npos) return "";
  std::string value = path.substr(pos + needle.size());
  const size_t end = value.find_first_of("&# ");
  if (end != std::string::npos) value.resize(end);
  return value;
}

void SetSocketTimeouts(int fd) {
#ifdef _WIN32
  const DWORD timeout_ms = 150;
#else
  timeval timeout {};
  timeout.tv_sec = 0;
  timeout.tv_usec = 150000;
#endif
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
#ifdef _WIN32
             reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
             reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
             reinterpret_cast<const char*>(&timeout), sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
             reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#endif
}

}  // namespace

HttpServer::HttpServer(std::shared_ptr<RemoteFrameProvider> frames, int port)
    : frames_(std::move(frames)), port_(port) {}

HttpServer::~HttpServer() { Stop(); }

bool HttpServer::Start() {
#ifdef _WIN32
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
#endif

  listen_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
  if (listen_fd_ < 0) return false;

  int yes = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&yes), sizeof(yes));

  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<unsigned short>(port_));
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    CLOSESOCKET(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (listen(listen_fd_, 16) < 0) {
    CLOSESOCKET(listen_fd_);
    listen_fd_ = -1;
    return false;
  }

  running_ = true;
  thread_ = std::thread(&HttpServer::Run, this);
  return true;
}

void HttpServer::Stop() {
  running_ = false;
  if (listen_fd_ >= 0) {
    const int wake_fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (wake_fd >= 0) {
      sockaddr_in addr {};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr.sin_port = htons(static_cast<unsigned short>(port_));
      connect(wake_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      CLOSESOCKET(wake_fd);
    }
  }
  if (thread_.joinable()) thread_.join();
  if (listen_fd_ >= 0) {
    CLOSESOCKET(listen_fd_);
    listen_fd_ = -1;
  }
#ifdef _WIN32
  WSACleanup();
#endif
}

void HttpServer::Run() {
  while (running_) {
    const int fd = listen_fd_;
    if (fd < 0) break;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;

    const int ready = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (!running_) break;
    if (ready <= 0) continue;

    sockaddr_in client_addr {};
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif
    const int client = static_cast<int>(
        accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len));
    if (client < 0) continue;
    SetSocketTimeouts(client);
    const bool is_loopback =
        ntohl(client_addr.sin_addr.s_addr) == INADDR_LOOPBACK;
    char addr_text[INET_ADDRSTRLEN] {};
    inet_ntop(AF_INET, &client_addr.sin_addr, addr_text, sizeof(addr_text));
    HandleClient(client, is_loopback, addr_text);
  }
}

void HttpServer::HandleClient(int client, bool is_loopback,
                              const std::string& client_id) {
  std::string request;
  std::vector<char> buffer(8192);
#ifdef _WIN32
  const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
  const ssize_t received = recv(client, buffer.data(), buffer.size(), 0);
#endif
  if (received > 0) request.assign(buffer.data(), static_cast<size_t>(received));

  const std::string path = RequestPath(request);
  std::string request_client_id = RequestParam(path, "client");
  if (request_client_id.empty()) request_client_id = client_id;
  const bool is_index = path == "/" || path.rfind("/?", 0) == 0;
  if (!is_index && !Authorized(request, is_loopback)) {
    SendResponse(client, "403 Forbidden", "text/html; charset=utf-8",
                 WebAccessHelpHtml());
    CLOSESOCKET(client);
    return;
  }

  const bool is_session = path.rfind("/session", 0) == 0;
  if (!is_index && !is_session && !frames_->ClientAllowed(request_client_id)) {
    SendResponse(client, "409 Conflict", "text/plain",
                 "Another OpenCPN Remote client is already connected.\n");
  } else if (path.rfind("/frame", 0) == 0) {
    int width = 0;
    int height = 0;
    unsigned long sequence = 0;
    auto frame = frames_->GetJpegFrame(&width, &height, &sequence);
    if (frame.empty()) {
      SendResponse(client, "503 Service Unavailable", "text/plain", "No frame\n");
    } else {
      SendBytes(client, "200 OK", "image/jpeg", frame);
    }
  } else if (path.rfind("/input/pointer", 0) == 0) {
    frames_->HandlePointerEvent(RequestBody(request));
    SendResponse(client, "204 No Content", "text/plain", "");
  } else if (path.rfind("/input/key", 0) == 0) {
    frames_->HandleKeyEvent(RequestBody(request));
    SendResponse(client, "204 No Content", "text/plain", "");
  } else if (path.rfind("/session", 0) == 0) {
    const auto result =
        frames_->HandleSessionEvent(RequestBody(request), request_client_id);
    if (result == RemoteFrameProvider::SessionResult::Accepted) {
      SendResponse(client, "204 No Content", "text/plain", "");
    } else if (result == RemoteFrameProvider::SessionResult::Busy) {
      SendResponse(client, "409 Conflict", "text/plain",
                   "Another OpenCPN Remote client is active.\n");
    } else {
      SendResponse(client, "403 Forbidden", "text/plain",
                   "OpenCPN Remote password is required or incorrect.\n");
    }
  } else {
    SendResponse(client, "200 OK", "text/html; charset=utf-8",
                 WebIndexHtmlWithToken(frames_->GetToken(),
                                       frames_->PasswordRequired()));
  }
  CLOSESOCKET(client);
}

bool HttpServer::Authorized(const std::string& request, bool is_loopback) const {
  if (is_loopback) return true;

  const std::string path = RequestPath(request);
  const size_t pos = path.find("token=");
  if (pos == std::string::npos) return false;
  std::string token = path.substr(pos + 6);
  const size_t end = token.find_first_of("&# ");
  if (end != std::string::npos) token.resize(end);
  return frames_->TokenMatches(token);
}

void HttpServer::SendResponse(int client, const std::string& status,
                              const std::string& content_type,
                              const std::string& body) const {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << content_type << "\r\n"
      << "Cache-Control: no-store\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << body;
  const std::string response = out.str();
  SendAll(client, response.data(), response.size());
}

void HttpServer::SendBytes(int client, const std::string& status,
                           const std::string& content_type,
                           const std::vector<unsigned char>& body) const {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << content_type << "\r\n"
      << "Cache-Control: no-store\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n";
  const std::string header = out.str();
  SendAll(client, header.data(), header.size());
  if (!body.empty()) {
    SendAll(client, reinterpret_cast<const char*>(body.data()), body.size());
  }
}
