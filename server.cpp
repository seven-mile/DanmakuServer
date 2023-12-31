#include "pch.h"

#include <atomic>
#include <string>
#include <iostream>
#include <thread>
#include <memory>

#include <http.h>
#pragma comment(lib, "httpapi.lib")

#include <atlutil.h>

import server;

namespace server {

void SendHttpJsonResponse(HANDLE req_queue, HTTP_REQUEST &req, USHORT status,
                       std::string const &reason, std::string const &body) {
  HTTP_RESPONSE response;
  HTTP_DATA_CHUNK dataChunk{};
  DWORD bytesSent;

  // Initialize the HTTP response structure.
  RtlZeroMemory(&response, sizeof(response));
  response.StatusCode = status;
  response.pReason = reason.c_str();
  response.ReasonLength = (USHORT)reason.size();

  // Add a known header.
  LPCSTR type = "application/json";
  do {
    response.Headers.KnownHeaders[HttpHeaderContentType].pRawValue = type;
    response.Headers.KnownHeaders[HttpHeaderContentType].RawValueLength =
        (USHORT)strlen(type);
  } while (FALSE);

  // Add an entity chunk.
  dataChunk.DataChunkType = HttpDataChunkFromMemory;
  dataChunk.FromMemory.pBuffer = const_cast<char *>(body.c_str());
  dataChunk.FromMemory.BufferLength = (ULONG)body.size();
  response.EntityChunkCount = 1;
  response.pEntityChunks = &dataChunk;

  // Because the entity body is sent in one call, it is not
  // required to specify the Content-Length.
  winrt::check_win32(HttpSendHttpResponse(req_queue,     // ReqQueueHandle
                                          req.RequestId, // Request ID
                                          0,             // Flags
                                          &response,     // HTTP response
                                          NULL,          // pReserved1
                                          &bytesSent, // bytes sent  (OPTIONAL)
                                          NULL, // pReserved2  (must be NULL)
                                          0,    // Reserved3   (must be 0)
                                          NULL, // LPOVERLAPPED(OPTIONAL)
                                          NULL  // pReserved4  (must be NULL)
                                          ));
}

DanmakuServer::DanmakuServer(danmaku_handler_t handler) {

  cancel_event = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  request_event = ::CreateEvent(NULL, TRUE, FALSE, NULL);

  winrt::check_win32(
      HttpInitialize(HTTPAPI_VERSION_2, HTTP_INITIALIZE_SERVER, NULL));

  HTTP_SERVER_SESSION_ID session_id;
  winrt::check_win32(
      HttpCreateServerSession(HTTPAPI_VERSION_2, &session_id, NULL));

  HTTP_URL_GROUP_ID group_id;
  winrt::check_win32(HttpCreateUrlGroup(session_id, &group_id, NULL));

  winrt::check_win32(HttpAddUrlToUrlGroup(
      group_id, std::format(L"http://127.0.0.1:{}/version", PORT).c_str(),
      (HTTP_URL_CONTEXT)this, NULL));

  winrt::check_win32(HttpAddUrlToUrlGroup(
      group_id, std::format(L"http://127.0.0.1:{}/danmaku", PORT).c_str(),
      (HTTP_URL_CONTEXT)this, NULL));

  HANDLE req_queue;
  winrt::check_win32(
      HttpCreateRequestQueue(HTTPAPI_VERSION_2, NULL, NULL, 0, &req_queue));

  HTTP_BINDING_INFO bding;
  bding.RequestQueueHandle = req_queue;
  bding.Flags.Present = 1;
  winrt::check_win32(HttpSetUrlGroupProperty(
      group_id, HttpServerBindingProperty, &bding, sizeof(bding)));

  std::thread th{[=] {
    constexpr int RECV_BUFFER_SIZE = 4096;
    auto recv_buffer = std::make_unique<char[]>(RECV_BUFFER_SIZE);
    HTTP_REQUEST &req = *(HTTP_REQUEST *)recv_buffer.get();
    while (true) {
      memset(recv_buffer.get(), 0, RECV_BUFFER_SIZE);

      ULONG bytes_returned;
      OVERLAPPED ov{};
      ov.hEvent = request_event;
      auto ret = HttpReceiveHttpRequest(req_queue, HTTP_NULL_ID, 0,
                                                &req, RECV_BUFFER_SIZE,
                                                &bytes_returned, &ov);

      if (ret != ERROR_IO_PENDING) {
        winrt::check_win32(ret);
      }

      DWORD ev = ::WaitForMultipleObjects(2, &cancel_event, FALSE, INFINITE);
      if (ev == 0) {
        // cancel event
        break;
      }
      ResetEvent(request_event);

      bool handled = false;
      if (req.Verb == HttpVerbGET) {
        winrt::Windows::Foundation::Uri url{req.CookedUrl.pFullUrl};

        if (url.Path().starts_with(L"/version")) {
          handled = true;
          SendHttpJsonResponse(req_queue, req, 200, "OK", R"({"version": 1, "repo": "https://github.com/seven-mile/DanmakuServer"})");
        } else if (url.Path().starts_with(L"/danmaku")) {
          handled = true;
          auto args = url.QueryParsed();
          auto name = args.GetFirstValueByName(L"name");
          auto content = args.GetFirstValueByName(L"content");
          handler(std::format(L"{}: {}", name, content));

          SendHttpJsonResponse(req_queue, req, 200, "OK", R"({"code": 0, "message": "ok"})");
        }
      }
      
      if (!handled) {
        SendHttpJsonResponse(req_queue, req, 403, "Unknown request",
                         R"({"code": 1, "message": "unknown request"})");
      }
    }
  }};

  server_thread.swap(th);
}

DanmakuServer::~DanmakuServer() {
  SetEvent(cancel_event);
  // wait util
  server_thread.join();

  winrt::check_win32(HttpTerminate(HTTP_INITIALIZE_SERVER, NULL));
}

} // namespace server
