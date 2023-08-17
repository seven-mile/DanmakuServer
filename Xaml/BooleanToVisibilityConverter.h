#pragma once

#include "BooleanToVisibilityConverter.g.h"

namespace winrt::DanmakuServer::implementation
{
    struct BooleanToVisibilityConverter: BooleanToVisibilityConverterT<BooleanToVisibilityConverter> {
		    BooleanToVisibilityConverter() noexcept;

		    Windows::Foundation::IInspectable Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& targetType, Windows::Foundation::IInspectable const& parameter, hstring const& language) const;
		    Windows::Foundation::IInspectable ConvertBack(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& targetType, Windows::Foundation::IInspectable const& parameter, hstring const& language) const;

        bool Positive() { return positive; }
        void Positive(bool value) { positive = value; }

        bool positive;
	  };
}

namespace winrt::DanmakuServer::factory_implementation
{
    struct BooleanToVisibilityConverter : BooleanToVisibilityConverterT<BooleanToVisibilityConverter, implementation::BooleanToVisibilityConverter>
    {
    };
}
