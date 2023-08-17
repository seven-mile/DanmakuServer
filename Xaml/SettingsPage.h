#pragma once

#include "SettingsPage.g.h"

#include "SettingsPageViewModel.h"

namespace winrt::DanmakuServer::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage() 
        {
            m_viewModel = winrt::make<SettingsPageViewModel>();
        }

        DanmakuServer::SettingsPageViewModel ViewModel() { return m_viewModel; }

        void ApplyButton_Clicked(found::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&) {
          m_viewModel.ApplyChanges();
        }
        void DiscardButton_Clicked(found::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&) {
          m_viewModel.LoadFromCurrent();
        }

        void TestButton_IsCheckedChanged(found::IInspectable const&, ui::Xaml::Controls::ToggleSplitButtonIsCheckedChangedEventArgs const&) {
          if (TestButton().IsChecked()) {
            m_viewModel.IsTestRunning(true);
          } else {
            m_viewModel.IsTestRunning(false);
          }
        }

        void TestMenuFlyoutItem_Clicked(found::IInspectable const&sender, Windows::UI::Xaml::RoutedEventArgs const&) {
          auto elem = sender.as<ui::Xaml::FrameworkElement>();
          auto tag = elem.Tag();
          auto tag_value = unbox_value<hstring>(tag);
          m_viewModel.TestIndex(std::wcstol(tag_value.c_str(), nullptr, 10));
        }

      private:
        DanmakuServer::SettingsPageViewModel m_viewModel{ nullptr };
    };
}

namespace winrt::DanmakuServer::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
