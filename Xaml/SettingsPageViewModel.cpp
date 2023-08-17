#include "pch.h"
#include "SettingsPageViewModel.h"
#if __has_include("SettingsPageViewModel.g.cpp")
#include "SettingsPageViewModel.g.cpp"
#endif

#include "../config.hpp"

import danmaku;
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
    
    void SettingsPageViewModel::StartTest() {
      assert(!test_timer);
      static std::default_random_engine rd{998244353};
      static std::wstring lorem{L"Lorem ipsum dolor sit amet Allocated, linux? kernel function is that"};

      static std::uniform_int_distribution text_gen{1, (int)lorem.size()};
      static auto gen_text = [](){
        std::wstring text{wchar_t(L'A' + (text_gen(rd) % 26))};
        for (int i = 0; i < text_gen(rd); ++i) {
          text += lorem[text_gen(rd)];
        }
        return text;
      };

      test_timer = threading::ThreadPoolTimer::CreatePeriodicTimer([this](auto const &){
        auto batch_size = std::map<int, int>{
          {0, 2},
          {1, 8},
          {2, 12},
          {3, 20}
        }[test_index];
        for (int i=0; i<batch_size; i++) {
          danmaku::DanmakuManager::singleton().IssueDanmaku(gen_text());
        }
      }, 300ms);
    }

    void SettingsPageViewModel::StopTest() {
      assert(test_timer);
      if (test_timer) {
        test_timer.Cancel();
        test_timer = nullptr;
      }
    }

}
