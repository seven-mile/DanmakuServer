#pragma once

#define NOMINMAX

#include <unknwn.h>

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Navigation.h>

#include <winrt/Windows.System.Threading.h>

#include <atlbase.h>
#include <atlwin.h>
#include <tchar.h>
#include <shellapi.h>

#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <d3d11_2.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi1_3.h>
#include <wrl/implements.h>

#include <tuple>
#include <random>
#include <format>
#include <functional>
#include <queue>
#include <map>
#include <unordered_map>
#include <optional>

#include <shared_mutex>

namespace found = winrt::Windows::Foundation;
namespace ui = winrt::Windows::UI;
namespace comp = ui::Composition;
namespace uicore = ui::Core;
namespace threading = winrt::Windows::System::Threading;

using namespace std::chrono_literals;
