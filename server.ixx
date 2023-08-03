module;
#include <functional>
#include <string>
#include <thread>
#include <atomic>
export module server;

export namespace server {

constexpr int PORT = 7654;

using danmaku_handler_t = std::function<void(std::wstring const &danmaku)>;

class DanmakuServer {

  danmaku_handler_t handler;
  std::thread server_thread;
  std::atomic<bool> running;

public:

  DanmakuServer(danmaku_handler_t handler);
  ~DanmakuServer();
};

}
