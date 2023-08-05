#include "pch.h"
#include "SettingsPageViewModel.h"
#if __has_include("SettingsPageViewModel.g.cpp")
#include "SettingsPageViewModel.g.cpp"
#endif

#include "../config.hpp"

import dcomp;

namespace winrt::DanmakuServer::implementation
{
    SettingsPageViewModel::SettingsPageViewModel() {
      // list fonts
      for (auto &fontname : dcomp::GetSystemFontList()) {
        installed_fonts.Append(fontname);
      }

      // load settings
      LoadFromCurrent();
    }

    void SettingsPageViewModel::ApplyChanges() {
      auto &cfg = config::DanmakuConfig::singleton();
      cfg.Opacity(opacity);
      cfg.Duration(std::chrono::duration_cast<found::TimeSpan>(duration_in_sec * 1s));
      cfg.DanmakuHeight(danmaku_height);
      cfg.RequestedLaneCount(req_lane_count);
      cfg.FontSize(font_size);
      cfg.FontFamily(FontFamily().c_str());
    }

    void SettingsPageViewModel::LoadFromCurrent() {
      auto &cfg = config::DanmakuConfig::singleton();
      this->Opacity(cfg.Opacity());
      this->DurationInSec(std::chrono::duration_cast<
        std::chrono::duration<float>>(cfg.Duration()).count());
      
      this->DanmakuHeight(cfg.DanmakuHeight());
      this->RequestedLaneCount(cfg.RequestedLaneCount());
      this->FontSize(cfg.FontSize());

      auto font_family = cfg.FontFamily();
      // find font index
      font_index = -1;
      for (int i = 0; i < installed_fonts.Size(); i++) {
        if (installed_fonts.GetAt(i) == font_family) {
          this->FontIndex(i);
          break;
        }
      }
      assert(font_index != -1);
    }

    void SettingsPageViewModel::ResetDefault() {
      auto &cfg = config::DanmakuConfig::singleton();
      throw winrt::hresult_not_implemented{};
    }
}
