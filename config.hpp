#pragma once

#include "pch.h"

#define DefineTypedProp(type, name, field, default_value) \
  private: \
  type field = (default_value); \
  std::vector<std::function<void(type const &)>> handlers_##field; \
  public: \
  type name() { std::shared_lock g{lock}; return field; } \
  DanmakuConfig &name(type const &value) { \
    { \
      std::unique_lock g{lock}; \
      field = value; \
    } \
    for (auto &f : handlers_##field) f(value); \
    return *this; \
  } \
  void Listen##name(std::function<void(type const &)> const &f, bool init = false) { \
    { \
      std::unique_lock g{handler_lock}; \
      handlers_##field.emplace_back(f); \
    } \
    if (init) f(field); \
  }

namespace config {

class DanmakuConfig {

  std::shared_mutex lock;
  std::mutex handler_lock;

  DefineTypedProp(float, Opacity, opacity, 1.f);
  DefineTypedProp(found::TimeSpan, Duration, duration, 10000ms);

  DefineTypedProp(float, DanmakuHeight, danmaku_height, 32.f);
  DefineTypedProp(int, RequestedLaneCount, req_lane_count, 1000);

  DefineTypedProp(float, FontSize, font_size, 28.f);
  DefineTypedProp(std::wstring, FontFamily, font_family, L"Microsoft YaHei UI");

public:

  static DanmakuConfig &singleton() {
    // this is thread safe
    static DanmakuConfig config;
    return config;
  }

  void Load(std::string const &path);
  void Save(std::string const &path);


};

}
