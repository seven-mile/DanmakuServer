#pragma once

#include "SettingsPageViewModel.g.h"

namespace winrt::DanmakuServer::implementation
{
    namespace xdata = Windows::UI::Xaml::Data;
    
    struct SettingsPageViewModel : SettingsPageViewModelT<SettingsPageViewModel>
    {
        SettingsPageViewModel();

        float Opacity() { return opacity; }
        void Opacity(float value) { opacity = value; NotifyChanged(L"Opacity"); }

        float DurationInSec() { return duration_in_sec; }
        void DurationInSec(float value) { duration_in_sec = value; NotifyChanged(L"DurationInSec"); }

        float DanmakuHeight() { return danmaku_height; }
        void DanmakuHeight(float value) { danmaku_height = value; NotifyChanged(L"DanmakuHeight"); }

        int32_t RequestedLaneCount() { return req_lane_count; }
        void RequestedLaneCount(int32_t value) { req_lane_count = value; NotifyChanged(L"RequestedLaneCount"); }

        float FontSize() { return font_size; }
        void FontSize(float value) { font_size = value; NotifyChanged(L"FontSize"); }

        hstring FontFamily() { return installed_fonts.GetAt(font_index); }

        int FontIndex() { return font_index; }
        void FontIndex(int value) { font_index = value; NotifyChanged(L"FontIndex"); }

        found::Collections::IObservableVector<hstring> InstalledFonts() {
          return installed_fonts;
        }

        bool IsTestRunning() { return is_test_running; }
        void IsTestRunning(bool value) {
          is_test_running = value;
          if (value) {
            StartTest();
          } else {
            StopTest();
          }
          NotifyChanged(L"IsTestRunning");
        }

        int TestIndex() { return test_index; }
        void TestIndex(int value) {
          test_index = value;
          NotifyChanged(L"TestIndex");
          NotifyChanged(L"TestLevelString");
        }

        hstring TestLevelString() {
            switch (test_index) {
              case 0:
                return L"Silent";
              case 1:
                return L"Normal";
              case 2:
                return L"Noisy";
              default:
                return L"Overwhelming";
            }
        }

        // configs
        void ApplyChanges();
        void LoadFromCurrent();
        void ResetDefault();

        winrt::event_token PropertyChanged(
            xdata::PropertyChangedEventHandler const
                &handler) {
          return property_changed.add(handler);
        }

        void PropertyChanged(winrt::event_token const &token) {
          property_changed.remove(token);
        }

      private:

        // test
        void StartTest();
        void StopTest();

        float opacity = 1.f;
        float duration_in_sec = 10.f;
        float danmaku_height = 32.f;
        int32_t req_lane_count = 1000;
        float font_size = 28.f;
        int font_index = 0;

        found::Collections::IObservableVector<hstring> installed_fonts
          = single_threaded_observable_vector<hstring>();

        bool is_test_running = false;
        int test_index = 0;
        threading::ThreadPoolTimer test_timer{nullptr};

        winrt::event<xdata::PropertyChangedEventHandler> property_changed;
        void NotifyChanged(hstring const &prop) {
          property_changed(*this, xdata::PropertyChangedEventArgs{prop});
        }
    };
}

namespace winrt::DanmakuServer::factory_implementation
{
    struct SettingsPageViewModel : SettingsPageViewModelT<SettingsPageViewModel, implementation::SettingsPageViewModel>
    {
    };
}
