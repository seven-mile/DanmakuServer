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
