#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "remote_frame_provider.h"

class HttpServer {
public:
  HttpServer(std::shared_ptr<RemoteFrameProvider> frames, int port);
  ~HttpServer();

  bool Start();
  void Stop();

private:
  void Run();
  void HandleClient(int client, bool is_loopback, const std::string& client_id);
  bool Authorized(const std::string& request, bool is_loopback) const;
  void SendResponse(int client, const std::string& status,
                    const std::string& content_type,
                    const std::string& body) const;
  void SendBytes(int client, const std::string& status,
                 const std::string& content_type,
                 const std::vector<unsigned char>& body) const;

  std::shared_ptr<RemoteFrameProvider> frames_;
  int port_;
  std::atomic<bool> running_{false};
  std::thread thread_;
  int listen_fd_ = -1;
};
