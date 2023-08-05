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

std::wstring UrlDecodeWString(std::wstring const &input) {
  std::wstring result;
  DWORD cnt;
  AtlUnescapeUrl(input.c_str(), result.data(), &cnt, DWORD(result.size()));
  result.resize(cnt);
  winrt::check_bool(AtlUnescapeUrl(input.c_str(), result.data(), &cnt, DWORD(result.size())));
  result.resize(lstrlenW(result.data()));
  return result;
}

std::map<std::wstring, std::wstring> ParseQueryString(std::wstring_view query_string) {
    std::map<std::wstring, std::wstring> result;

    if (query_string.data() == nullptr)
      return result;

    // eat '?'
    query_string = query_string.substr(1);

    // Split the query_string by '&' to get individual key-value pairs
    std::wstring key_value_pair;
    std::wstringstream ss{std::wstring{query_string}};
    while (std::getline(ss, key_value_pair, L'&')) {
        // Split each key-value pair by '='
        std::wstring key, value;
        size_t equal_pos = key_value_pair.find(L'=');
        if (equal_pos != std::wstring::npos) {
            key = key_value_pair.substr(0, equal_pos);
            value = key_value_pair.substr(equal_pos + 1);
        } else {
            key = key_value_pair;
            value = L"";
        }

        key = UrlDecodeWString(key);
        value = UrlDecodeWString(value);

        result[key] = value;
    }

    return result;
}

DanmakuServer::DanmakuServer(danmaku_handler_t handler) {

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

  running = true;

  std::thread th{[=] {
    constexpr int RECV_BUFFER_SIZE = 4096;
    auto recv_buffer = std::make_unique<char[]>(RECV_BUFFER_SIZE);
    HTTP_REQUEST &req = *(HTTP_REQUEST *)recv_buffer.get();
    while (running.load()) {
      memset(recv_buffer.get(), 0, RECV_BUFFER_SIZE);

      ULONG bytes_returned;
      winrt::check_win32(HttpReceiveHttpRequest(req_queue, HTTP_NULL_ID, 0,
                                                &req, RECV_BUFFER_SIZE,
                                                &bytes_returned, NULL));
      bool handled = false;
      if (req.Verb == HttpVerbGET) {
        std::wstring_view abs_path{req.CookedUrl.pAbsPath, req.CookedUrl.AbsPathLength / sizeof(wchar_t)};
        std::wstring_view query_string{req.CookedUrl.pQueryString, req.CookedUrl.QueryStringLength / sizeof(wchar_t)};

        if (abs_path.starts_with(L"/version")) {
          handled = true;
          SendHttpJsonResponse(req_queue, req, 200, "OK", R"({"version": 1, "repo": "https://github.com/iceking2nd/real-url"})");
        } else if (abs_path.starts_with(L"/danmaku")) {
          handled = true;
          auto args = ParseQueryString(query_string);
          auto &name = args[L"name"], &content = args[L"content"];
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
  running = false;
  // wait util
  server_thread.join();

  winrt::check_win32(HttpTerminate(HTTP_INITIALIZE_SERVER, NULL));
}

} // namespace server
