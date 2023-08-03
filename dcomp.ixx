module;
#define NOMINMAX

#include <Windows.h>

#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>

export module dcomp;

export namespace dcomp {

winrt::Windows::UI::Core::CoreDispatcher GetOrCreateCoreDispatcher();

std::tuple<winrt::Windows::UI::Composition::Compositor, winrt::Windows::UI::Composition::ContainerVisual>
CreateCompositorForWindow(HWND hWnd);

winrt::Windows::UI::Composition::SpriteVisual CreateTextVisual(
  winrt::Windows::UI::Composition::Compositor compositor,
  std::wstring const &text
);

} // namespace dcomp
