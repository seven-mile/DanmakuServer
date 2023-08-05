#include "pch.h"

#include <dwrite.h>

#include <windows.ui.composition.interop.h>

#include "config.hpp"

#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "dwmapi")

import dcomp;

namespace dcomp {

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Core;

//-------------------- Definitions
enum THUMBNAIL_TYPE {
  TT_DEFAULT = 0x0,
  TT_SNAPSHOT = 0x1,
  TT_ICONIC = 0x2,
  TT_BITMAPPENDING = 0x3,
  TT_BITMAP = 0x4
};

typedef HRESULT(WINAPI *DwmpCreateSharedThumbnailVisual)(
    IN HWND hwndDestination, IN HWND hwndSource, IN DWORD dwThumbnailFlags,
    IN DWM_THUMBNAIL_PROPERTIES *pThumbnailProperties, IN VOID *pDCompDevice,
    OUT VOID **ppVisual, OUT PHTHUMBNAIL phThumbnailId);

typedef HRESULT(WINAPI *DwmpQueryWindowThumbnailSourceSize)(
    IN HWND hwndSource, IN BOOL fSourceClientAreaOnly, OUT SIZE *pSize);

typedef HRESULT(WINAPI *DwmpQueryThumbnailType)(IN HTHUMBNAIL hThumbnailId,
                                                OUT THUMBNAIL_TYPE *thumbType);

// pre-cobalt/pre-iron
typedef HRESULT(WINAPI *DwmpCreateSharedVirtualDesktopVisual)(
    IN HWND hwndDestination, IN VOID *pDCompDevice, OUT VOID **ppVisual,
    OUT PHTHUMBNAIL phThumbnailId);

// cobalt/iron (20xxx+)
// No changes except for the function name.
typedef HRESULT(WINAPI *DwmpCreateSharedMultiWindowVisual)(
    IN HWND hwndDestination, IN VOID *pDCompDevice, OUT VOID **ppVisual,
    OUT PHTHUMBNAIL phThumbnailId);

// pre-cobalt/pre-iron
typedef HRESULT(WINAPI *DwmpUpdateSharedVirtualDesktopVisual)(
    IN HTHUMBNAIL hThumbnailId, IN HWND *phwndsInclude, IN DWORD chwndsInclude,
    IN HWND *phwndsExclude, IN DWORD chwndsExclude, OUT RECT *prcSource,
    OUT SIZE *pDestinationSize);

// cobalt/iron (20xxx+)
// Change: function name + new DWORD parameter.
// Pass "1" in dwFlags. Feel free to explore other flags.
typedef HRESULT(WINAPI *DwmpUpdateSharedMultiWindowVisual)(
    IN HTHUMBNAIL hThumbnailId, IN HWND *phwndsInclude, IN DWORD chwndsInclude,
    IN HWND *phwndsExclude, IN DWORD chwndsExclude, OUT RECT *prcSource,
    OUT SIZE *pDestinationSize, IN DWORD dwFlags);

#define DWM_TNP_FREEZE 0x100000
#define DWM_TNP_ENABLE3D 0x4000000
#define DWM_TNP_DISABLE3D 0x8000000
#define DWM_TNP_FORCECVI 0x40000000
#define DWM_TNP_DISABLEFORCECVI 0x80000000

//-------- CoreDispatcher

// Windows::UI::Core::IInternalCoreDispatcherStatic (14393+)
DECLARE_INTERFACE_IID_(IInternalCoreDispatcherStatic, IInspectable,
                       "4B4D0861-D718-4F7C-BEC7-735C065F7C73") {
  STDMETHOD(GetForCurrentThread)
  (winrt::Windows::UI::Core::CoreDispatcher * ppDispatcher) PURE;
  STDMETHOD(GetOrCreateForCurrentThread)
  (winrt::Windows::UI::Core::CoreDispatcher * ppDispatcher) PURE;
};

//-------- Interop Composition interfaces

// Windows::UI::Composition::HwndTarget (14393-15063)
DECLARE_INTERFACE_IID_(HwndTarget, IUnknown,
                       "6677DA68-C80C-407A-A4D2-3AA118AD7C46") {
  STDMETHOD(GetRoot)
  (THIS_ OUT ABI::Windows::UI::Composition::IVisual * *value) PURE;
  STDMETHOD(SetRoot)
  (THIS_ IN ABI::Windows::UI::Composition::IVisual * value) PURE;
};

// Windows::UI::Composition::InteropCompositorTarget
DECLARE_INTERFACE_IID_(InteropCompositionTarget, IUnknown,
                       "EACDD04C-117E-4E17-88F4-D1B12B0E3D89") {
  STDMETHOD(SetRoot)(THIS_ IN IDCompositionVisual * visual) PURE;
};

// Windows::UI::Composition::IInteropCompositorPartner
DECLARE_INTERFACE_IID_(IInteropCompositorPartner, IUnknown,
                       "e7894c70-af56-4f52-b382-4b3cd263dc6f") {
  STDMETHOD(MarkDirty)(THIS_) PURE;

  STDMETHOD(ClearCallback)(THIS_) PURE;

  STDMETHOD(CreateManipulationTransform)
  (THIS_ IN IDCompositionTransform * transform, IN REFIID iid,
   OUT VOID * *result) PURE;

  STDMETHOD(RealClose)(THIS_) PURE;
};

// Windows.UI.Composition.IInteropCompositorPartnerCallback
DECLARE_INTERFACE_IID_(IInteropCompositorPartnerCallback, IUnknown,
                       "9bb59fc9-3326-4c32-bf06-d6b415ac2bc5") {
  STDMETHOD(NotifyDirty)(THIS_) PURE;

  STDMETHOD(NotifyDeferralState)(THIS_ bool deferRequested) PURE;
};

// Windows::UI::Composition::IInteropCompositorFactoryPartner
DECLARE_INTERFACE_IID_(IInteropCompositorFactoryPartner, IInspectable,
                       "22118adf-23f1-4801-bcfa-66cbf48cc51b") {
  STDMETHOD(CreateInteropCompositor)
  (THIS_ IN IUnknown * renderingDevice,
   IN IInteropCompositorPartnerCallback * callback, IN REFIID iid,
   OUT VOID * *instance) PURE;

  STDMETHOD(CheckEnabled)
  (THIS_ OUT bool *enableInteropCompositor, OUT bool *enableExposeVisual) PURE;
};

// We also need these if you want to use MultiWindow/VirtualDesktop.

enum WINDOWCOMPOSITIONATTRIB {
  WCA_UNDEFINED = 0x0,
  WCA_NCRENDERING_ENABLED = 0x1,
  WCA_NCRENDERING_POLICY = 0x2,
  WCA_TRANSITIONS_FORCEDISABLED = 0x3,
  WCA_ALLOW_NCPAINT = 0x4,
  WCA_CAPTION_BUTTON_BOUNDS = 0x5,
  WCA_NONCLIENT_RTL_LAYOUT = 0x6,
  WCA_FORCE_ICONIC_REPRESENTATION = 0x7,
  WCA_EXTENDED_FRAME_BOUNDS = 0x8,
  WCA_HAS_ICONIC_BITMAP = 0x9,
  WCA_THEME_ATTRIBUTES = 0xA,
  WCA_NCRENDERING_EXILED = 0xB,
  WCA_NCADORNMENTINFO = 0xC,
  WCA_EXCLUDED_FROM_LIVEPREVIEW = 0xD,
  WCA_VIDEO_OVERLAY_ACTIVE = 0xE,
  WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 0xF,
  WCA_DISALLOW_PEEK = 0x10,
  WCA_CLOAK = 0x11,
  WCA_CLOAKED = 0x12,
  WCA_ACCENT_POLICY = 0x13,
  WCA_FREEZE_REPRESENTATION = 0x14,
  WCA_EVER_UNCLOAKED = 0x15,
  WCA_VISUAL_OWNER = 0x16,
  WCA_HOLOGRAPHIC = 0x17,
  WCA_EXCLUDED_FROM_DDA = 0x18,
  WCA_PASSIVEUPDATEMODE = 0x19,
  WCA_LAST = 0x1A,
};

struct WINDOWCOMPOSITIONATTRIBDATA {
  WINDOWCOMPOSITIONATTRIB Attrib;
  void *pvData;
  DWORD cbData;
};

typedef BOOL(WINAPI *SetWindowCompositionAttribute)(
    IN HWND hwnd, IN WINDOWCOMPOSITIONATTRIBDATA *pwcad);

//------------------------- Getting functions
DwmpQueryThumbnailType lDwmpQueryThumbnailType;
DwmpCreateSharedThumbnailVisual lDwmpCreateSharedThumbnailVisual;
DwmpQueryWindowThumbnailSourceSize lDwmpQueryWindowThumbnailSourceSize;

// PRE-IRON
DwmpCreateSharedVirtualDesktopVisual lDwmpCreateSharedVirtualDesktopVisual;
DwmpUpdateSharedVirtualDesktopVisual lDwmpUpdateSharedVirtualDesktopVisual;

// 20xxx+
DwmpCreateSharedMultiWindowVisual lDwmpCreateSharedMultiWindowVisual;
DwmpUpdateSharedMultiWindowVisual lDwmpUpdateSharedMultiWindowVisual;

SetWindowCompositionAttribute lSetWindowCompositionAttribute;

bool InitPrivateDwmAPIs() {
  auto dwmapiLib = LoadLibrary(L"dwmapi.dll");

  if (!dwmapiLib)
    return false;

  lDwmpQueryThumbnailType =
      (DwmpQueryThumbnailType)GetProcAddress(dwmapiLib, MAKEINTRESOURCEA(114));
  lDwmpCreateSharedThumbnailVisual =
      (DwmpCreateSharedThumbnailVisual)GetProcAddress(dwmapiLib,
                                                      MAKEINTRESOURCEA(147));
  lDwmpQueryWindowThumbnailSourceSize =
      (DwmpQueryWindowThumbnailSourceSize)GetProcAddress(dwmapiLib,
                                                         MAKEINTRESOURCEA(162));

  // PRE-IRON
  lDwmpCreateSharedVirtualDesktopVisual =
      (DwmpCreateSharedVirtualDesktopVisual)GetProcAddress(
          dwmapiLib, MAKEINTRESOURCEA(163));
  lDwmpUpdateSharedVirtualDesktopVisual =
      (DwmpUpdateSharedVirtualDesktopVisual)GetProcAddress(
          dwmapiLib, MAKEINTRESOURCEA(164));

  // 20xxx+
  lDwmpCreateSharedMultiWindowVisual =
      (DwmpCreateSharedMultiWindowVisual)GetProcAddress(dwmapiLib,
                                                        MAKEINTRESOURCEA(163));
  lDwmpUpdateSharedMultiWindowVisual =
      (DwmpUpdateSharedMultiWindowVisual)GetProcAddress(dwmapiLib,
                                                        MAKEINTRESOURCEA(164));

  if (false) // Just a placeholder, don't.
    return false;

  return true;
}

bool InitPrivateUser32APIs() {
  auto user32Lib = LoadLibrary(L"user32.dll");

  if (!user32Lib)
    return false;

  lSetWindowCompositionAttribute =
      (SetWindowCompositionAttribute)GetProcAddress(
          user32Lib, "SetWindowCompositionAttribute");

  if (!lSetWindowCompositionAttribute)
    return false;

  return true;
}

//--------------------- Create Device func
using namespace Microsoft::WRL;

ComPtr<ID3D11Device> direct3dDevice;
ComPtr<IDXGIDevice2> dxgiDevice;
ComPtr<ID2D1Factory2> d2dFactory2;
ComPtr<ID2D1Device> d2dDevice;
ComPtr<IDCompositionDesktopDevice> dcompDevice;
ComPtr<IDWriteFactory> dwFactory;
ComPtr<IDWriteTextFormat> dwTextFormat;


bool CreateDevice() {
  if (D3D11CreateDevice(0, // Adapter
                        D3D_DRIVER_TYPE_HARDWARE, NULL,
                        D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                        D3D11_SDK_VERSION, direct3dDevice.GetAddressOf(),
                        nullptr, nullptr) != S_OK) {
    // Maybe try creating with D3D_DRIVER_TYPE_WARP before returning false.
    // Always listen to device changes.
    return false;
  }

  if (direct3dDevice->QueryInterface(dxgiDevice.GetAddressOf()) != S_OK) {
    return false;
  }

  if (D2D1CreateFactory(D2D1_FACTORY_TYPE::D2D1_FACTORY_TYPE_SINGLE_THREADED,
                        __uuidof(ID2D1Factory2),
                        (void **)d2dFactory2.GetAddressOf()) != S_OK) {
    return false;
  }

  if (d2dFactory2->CreateDevice(dxgiDevice.Get(), d2dDevice.GetAddressOf()) !=
      S_OK) {
    return false;
  }

  if (::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown **>(dwFactory.GetAddressOf())) != S_OK)
    return false;

  if (dwFactory->CreateTextFormat(L"Microsoft Yahei", NULL, DWRITE_FONT_WEIGHT_BOLD,
                        DWRITE_FONT_STYLE_NORMAL,
                        DWRITE_FONT_STRETCH_NORMAL, 28.0f, L"zh-cn",
                        dwTextFormat.GetAddressOf()) != S_OK)
    return false;

  return true;
}

//------------------------ Windows.UI.Composit* / IDComposition* interop

bool InitializeInteropCompositor(HWND hwnd,
                                 Compositor *compositor,
                                 IUnknown **compositionTarget,
                                 ContainerVisual *rootVisual) {
  auto interopCompositorFactory =
      winrt::get_activation_factory<Compositor,
                                    IInteropCompositorFactoryPartner>();

  winrt::com_ptr<IInteropCompositorPartner> interopCompositor;
  auto interopRes = interopCompositorFactory->CreateInteropCompositor(
      d2dDevice.Get(), NULL, winrt::guid_of<IInteropCompositorPartner>(),
      interopCompositor.put_void());
  if (interopRes != S_OK)
    return false;

  // Get as Compositor and as IDCompositionDevice
  auto m_compositor = interopCompositor.as<Compositor>();
  dcompDevice = interopCompositor.as<IDCompositionDesktopDevice>().detach();

  // Create a target for our window
  winrt::com_ptr<IDCompositionTarget> dcompTarget;
  auto res = dcompDevice->CreateTargetForHwnd(hwnd, TRUE, dcompTarget.put());

  if (res != S_OK)
    return false;

  // Create a container visual to hold all our visuals and then set it as root.
  // InteropCompositionTarget derives from DesktopWindowTarget which ultimately
  // derives from CompositionTarget - 16299+ InteropCompositionTarget derives
  // from HwndTarget - 14393-15063

  auto containerVisual = m_compositor.CreateContainerVisual();

  if (auto compTarget = dcompTarget.try_as<CompositionTarget>()) {
    compTarget.Root(containerVisual);
    *compositionTarget = compTarget.as<IUnknown>().detach();
  } else {
    // We are on 15063 or 14393

    // Get "raw" pointer to IVisual
    winrt::com_ptr<ABI::Windows::UI::Composition::IVisual> visualAbi;
    winrt::get_unknown(containerVisual)->QueryInterface(visualAbi.put());

    auto hwndTarget = dcompTarget.as<HwndTarget>();
    hwndTarget->SetRoot(visualAbi.get());

    *compositionTarget = hwndTarget.as<IUnknown>().detach();
  }

  *compositor = m_compositor;
  *rootVisual = containerVisual;

  return true;
}

CoreDispatcher GetOrCreateCoreDispatcher() {
  // CoreDispatcher internally creates a DispatcherQueue if it doesn't exist
  // (and if you are on 15063 or newer)
  auto dispatcher =
      winrt::try_get_activation_factory<CoreDispatcher,
                                        IInternalCoreDispatcherStatic>();

  // On 10586 there isn't a way to create a CoreDispatcher (unless you create a
  // CoreWindow, lol.)
  if (!dispatcher)
    return nullptr;

  CoreDispatcher coreDispatcher{nullptr};
  dispatcher->GetOrCreateForCurrentThread(&coreDispatcher);
  return coreDispatcher;
}

inline bool InitDcompModule() {
  static std::atomic<bool> initialized{false};
  if (initialized)
    return true;
  initialized = true;

  winrt::init_apartment();

  if (!InitPrivateDwmAPIs() || !InitPrivateUser32APIs() || !CreateDevice())
    return false;
  return true;
}

std::tuple<Compositor, ContainerVisual>
CreateCompositorForWindow(HWND hWnd) {
  InitDcompModule();

  Compositor compositor{nullptr};
  ContainerVisual rootVisual{nullptr};
  IUnknown *compositionTarget{nullptr};

  if (!InitializeInteropCompositor(hWnd, &compositor,
                                   &compositionTarget, &rootVisual))
    return {nullptr, nullptr};

  // This part is needed ONLY if you plan on using MultiWindow/VirtualDesktop
  // functions.
  BOOL enable = TRUE;
  WINDOWCOMPOSITIONATTRIBDATA wData{};
  wData.Attrib = WCA_EXCLUDED_FROM_LIVEPREVIEW;
  wData.pvData = &enable;
  wData.cbData = sizeof(BOOL);
  winrt::check_bool(lSetWindowCompositionAttribute(hWnd, &wData));

  BOOL disable = FALSE;
  winrt::check_hresult(DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &disable, sizeof(disable)));

  return {compositor, rootVisual};
}

Visual CreateTextVisualDComp(Compositor compositor, std::wstring const &text) {
  auto dcompositor = compositor.as<IDCompositionDesktopDevice>();
  
  winrt::com_ptr<IDCompositionVisual2> textVisual;
  winrt::check_hresult(dcompositor->CreateVisual(textVisual.put()));

  winrt::com_ptr<IDCompositionSurface> textSurface;
  winrt::check_hresult(dcompositor->CreateSurface(
    10000, 500,
    DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED,
    textSurface.put())
  );

  winrt::check_hresult(textVisual->SetContent(textSurface.get()));

  winrt::com_ptr<ID2D1DeviceContext> d2dCtx;
  POINT updateOffset = {0, 0};
  winrt::check_hresult(textSurface->BeginDraw(NULL, __uuidof(ID2D1DeviceContext), d2dCtx.put_void(), &updateOffset));

  D2D1_RECT_F rectText{0, 0, 1000, 200};
  winrt::com_ptr<::ID2D1SolidColorBrush> brush;
            winrt::check_hresult(d2dCtx->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::White, 1.0f), brush.put()));

  d2dCtx->DrawTextW(text.c_str(), (UINT32)text.size(), dwTextFormat.Get(), rectText, brush.get());

  winrt::check_hresult(textSurface->EndDraw());

  winrt::check_hresult(dcompositor->Commit());

  return textVisual.as<Visual>();
}

SpriteVisual CreateTextVisual(Compositor compositor, std::wstring const &text) {

  auto icompositor = compositor.try_as<ABI::Windows::UI::Composition::ICompositorInterop>();

  winrt::com_ptr<ABI::Windows::UI::Composition::ICompositionGraphicsDevice> device;
  winrt::check_hresult(icompositor->CreateGraphicsDevice(d2dDevice.Get(), device.put()));

  winrt::com_ptr<IDWriteTextLayout> textLayout;

  winrt::check_hresult(
      dwFactory->CreateTextLayout(
          text.c_str(),
          (uint32_t)text.size(),
          dwTextFormat.Get(),
          1e6,
          1e6,
          textLayout.put()
      )
  );

  DWRITE_TEXT_RANGE full_range{0, (uint32_t)text.size()};

  winrt::check_hresult(textLayout->SetFontSize(
    config::DanmakuConfig::singleton().FontSize(),
    full_range));

  winrt::check_hresult(textLayout->SetFontFamilyName(
    config::DanmakuConfig::singleton().FontFamily().c_str(),
    full_range));

  DWRITE_TEXT_METRICS metrics;
  winrt::check_hresult(textLayout->GetMetrics(&metrics));
  ABI::Windows::Foundation::Size surfaceSize = { metrics.width, metrics.height };

  winrt::com_ptr<ABI::Windows::UI::Composition::ICompositionDrawingSurface> surface;
  winrt::check_hresult(device->CreateDrawingSurface(
    surfaceSize,
    ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,
    ABI::Windows::Graphics::DirectX::DirectXAlphaMode_Premultiplied,
    surface.put()
  ));

  auto drawingSurface = surface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();

  winrt::com_ptr<ID2D1DeviceContext> d2dCtx;
  POINT updateOffset = {0, 0};
  winrt::check_hresult(drawingSurface->BeginDraw(NULL, __uuidof(ID2D1DeviceContext), d2dCtx.put_void(), &updateOffset));

  winrt::com_ptr<::ID2D1SolidColorBrush> brush;
  winrt::check_hresult(d2dCtx->CreateSolidColorBrush(
      D2D1::ColorF(D2D1::ColorF::White, 1.0f), brush.put()));

  d2dCtx->DrawTextLayout(D2D1::Point2F(1.f * updateOffset.x, 1.f * updateOffset.y),
    textLayout.get(), brush.get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

  winrt::check_hresult(drawingSurface->EndDraw());

  auto abi_compositor = compositor.as<ABI::Windows::UI::Composition::ICompositor>();
  winrt::com_ptr<ABI::Windows::UI::Composition::ICompositionSurfaceBrush> abi_brushSurface;
  winrt::check_hresult(abi_compositor->CreateSurfaceBrushWithSurface(
    drawingSurface.as<ABI::Windows::UI::Composition::ICompositionSurface>().get(),
    abi_brushSurface.put()
  ));

  CompositionSurfaceBrush textBrush = abi_brushSurface.as<CompositionSurfaceBrush>();
  auto visual = compositor.CreateSpriteVisual();
  visual.Brush(textBrush);
  visual.Size({ metrics.width, metrics.height });
  return visual;
}

std::vector<std::wstring> GetSystemFontList() {

  InitDcompModule();

  std::vector<std::wstring> fontList;

  winrt::com_ptr<IDWriteFontCollection> coll;
  winrt::check_hresult(dwFactory->GetSystemFontCollection(coll.put()));

  
  uint32_t familyCount = coll->GetFontFamilyCount();

  for (uint32_t i = 0; i < familyCount; ++i) {
    winrt::com_ptr<IDWriteFontFamily> family;
    winrt::check_hresult(coll->GetFontFamily(i, family.put()));

    winrt::com_ptr<IDWriteLocalizedStrings> familyNames;
    winrt::check_hresult(family->GetFamilyNames(familyNames.put()));

    uint32_t index = 0;
    BOOL exists = false;

    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    int defaultLocaleSuccess =
        GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH);
    if (defaultLocaleSuccess) {
      winrt::check_hresult(
          familyNames->FindLocaleName(localeName, &index, &exists));
    } else {
      winrt::check_hresult(
          familyNames->FindLocaleName(L"en-us", &index, &exists));
    }

    if (!exists) {
      index = 0;
    }

    uint32_t length = 0;
    winrt::check_hresult(familyNames->GetStringLength(index, &length));

    std::wstring name;
    name.resize(length);
    name.reserve(length + 1);
    winrt::check_hresult(familyNames->GetString(index, name.data(), length + 1));

    fontList.push_back(name);
  }

  return fontList;
}

} // namespace dcomp

