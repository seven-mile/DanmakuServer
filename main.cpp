#include "pch.h"

#include "config.hpp"

#include "resource.h"
#include <CommCtrl.h>

#include <Mile.Xaml.h>

//#include "Xaml/App.h"
#include "Xaml/SettingsPage.h"

#include "BooleanToVisibilityConverter.g.h"

import dcomp;
import danmaku;
import server;

#pragma comment(lib, "Comctl32.lib")

class SettingsWnd : public ATL::CWindowImpl<SettingsWnd> {

public:
  BEGIN_MSG_MAP(SettingsWnd)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  MESSAGE_HANDLER(WM_GETMINMAXINFO, OnMinMaxInfo)
  END_MSG_MAP()

  DECLARE_WND_SUPERCLASS(_T("Danmaku.SettingsWindow"), _T("Mile.Xaml.ContentWindow"));

  LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL &handled) {
    handled = false;
    SetIcon(LoadIcon(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_SETTINGS)), TRUE);
    return S_OK;
  }

  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL &) {
    // overrides default, no quit message, just hide it
    ShowWindow(false);
    return S_OK;
  }

  LRESULT OnMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL &) {
    LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
    auto dpi = GetDpiForWindow(m_hWnd);
    lpMMI->ptMinTrackSize.x = 760 * dpi / 96;
    lpMMI->ptMinTrackSize.y = 400 * dpi / 96;
    return S_OK;
  }

  static SettingsWnd &singleton() {
    static SettingsWnd wnd;
    return wnd;
  }

  static LPCTSTR GetWndCaption() {
    return _T("Danmaku Server Settings");
  }

  static DWORD GetWndStyle(DWORD dwStyle) {
    return dwStyle | WS_OVERLAPPEDWINDOW;
  }

  static DWORD GetWndExStyle(DWORD dwExStyle) {
    return dwExStyle | WS_EX_TOPMOST;
  }

};

class ShellIconWnd
    : public ATL::CWindowImpl<ShellIconWnd> {
public:
  DECLARE_WND_CLASS(_T("ShellIconHiddenWindow"));

private:
  BEGIN_MSG_MAP(ShellIconWnd)
  MESSAGE_HANDLER(WM_USER, OnNIProc)
  END_MSG_MAP()

  static constexpr // {87810064-5698-45EB-B53D-B6CBD1924EB1}
  const GUID GUID_TRAY_ICON = 
  { 0x87810064, 0x5698, 0x45eb, { 0xb5, 0x3d, 0xb6, 0xcb, 0xd1, 0x92, 0x4e, 0xb1 } };

  void ShowAbout() {
    TASKDIALOGCONFIG td{};
    td.cbSize = sizeof(td);
    td.hwndParent = m_hWnd;
    td.hInstance = GetModuleHandle(NULL);
    td.dwFlags = TDF_ENABLE_HYPERLINKS;
    td.pszMainIcon = MAKEINTRESOURCE(m_CurrentIconResource);
    td.pszWindowTitle = L"About";
    td.pszMainInstruction = L"Danmaku Server 1.5.3";
    td.pszContent = 
LR"(Please check our <a href="https://github.com/seven-mile/DanmakuServer">GitHub Repo</a> for more information.
The danmaku server is hosted on :7654 port, access <a href="http://localhost:7654/danmaku?name=danmaku%20server&content=hello,%20world%F0%9F%92%98">this url</a> for sample danmaku effect.)";
    td.nDefaultButton = TDCBF_OK_BUTTON;
    td.pfCallback = [](HWND hwnd, UINT msg, WPARAM, LPARAM lParam, LONG_PTR) {
      if (msg == TDN_HYPERLINK_CLICKED) {
        ShellExecute(hwnd, L"open", (LPCWSTR)lParam, NULL, NULL, SW_SHOW);
      }
      return S_OK;
    };

    winrt::check_hresult(TaskDialogIndirect(&td, NULL, NULL, NULL));
  }

  void ShowSettings() {
    SettingsWnd::singleton().ShowWindow(true);
    SettingsWnd::singleton().SetActiveWindow();
  }

  HRESULT CreateShell() {

    RECT rect{};

    // Create a hidden window (using CWindowImpl)
    HWND hWnd = Create(NULL, rect, L"ShellIconHiddenWindow", WS_POPUP);

    if (hWnd != 0) // Was created?
    {
      // Add the icon into the shell
      ShellNotify(NIM_ADD);
      ShellNotify(NIM_SETVERSION);
      return S_OK;
    } else {
      return HRESULT_FROM_WIN32(GetLastError());
    }
  }

  void DestroyShell() {
    ShellNotify(NIM_DELETE); // Remove the icon
    if (m_hWnd != NULL) {
      // Get rid of the hidden window
      DestroyWindow();
    }
  }

  void ShellNotify(DWORD msg) {

    NOTIFYICONDATA notifyIconData{};
    notifyIconData.cbSize = sizeof(notifyIconData);
    notifyIconData.guidItem = GUID_TRAY_ICON;
    notifyIconData.uVersion = NOTIFYICON_VERSION_4;
    notifyIconData.hWnd = m_hWnd;
    notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP | NIF_GUID;
    notifyIconData.uCallbackMessage = WM_USER;
    notifyIconData.uID = 0;

    notifyIconData.hIcon = ::LoadIcon(GetModuleHandle(NULL),
                                      MAKEINTRESOURCE(m_CurrentIconResource));
    ::lstrcpyn(notifyIconData.szTip, m_CurrentText.c_str(),
               ARRAYSIZE(notifyIconData.szTip)); // Limit to 64 chars
    ::Shell_NotifyIcon(msg, &notifyIconData);
  }

  LRESULT OnNIProc(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam,
                   BOOL & /*bHandled*/) {

    switch (LOWORD(lParam)) {
      case NIN_SELECT:
        // for NOTIFYICON_VERSION_4 clients, NIN_SELECT is prerable to listening
        // to mouse clicks and key presses directly.
        break;

      case NIN_BALLOONTIMEOUT:
        break;

      case NIN_BALLOONUSERCLICK:
        break;

      case WM_CONTEXTMENU: {
        POINT const pt = {LOWORD(wParam), HIWORD(wParam)};
        int cmd = ShowPopupMenu(IDM_TRAY, pt);
        if (cmd == IDMI_EXIT) {
          PostQuitMessage(0);
        } else if (cmd == IDMI_ABOUT) {
          ShowAbout();
        } else if (cmd == IDMI_SETTINGS) {
          ShowSettings();
        }
      } break;
    }

    return 0;
  }

public:

  static ShellIconWnd &singleton() {
    static ShellIconWnd wnd{};
    return wnd;
  }

  void SetShellTipText(std::wstring const &TipText) {
    // Save this text for when we update
    m_CurrentText = TipText;
    ShellNotify(NIM_MODIFY);
  }

  void SetShellIcon(WORD IconResource) {
    // Save this icon resource for when we update
    m_CurrentIconResource = IconResource;
    ShellNotify(NIM_MODIFY);
  }

  void SetShellVisible(bool bVisible = true) {
    if (bVisible == true) // User wants to show the icon in the shell
    {
      if (m_bVisible == false) // Doesn't already exist?
      {
        // Create the shell, and timer (if applicable)
        CreateShell();
      } // Otherwise, well you already have icon in the shell. :-)
    } else // User wants rid of the icon
    {
      if (m_bVisible == true) // Is it there already?
      {
        DestroyShell(); // Get rid
      }
    }

    m_bVisible = bVisible;
  }

  int ShowPopupMenu(WORD PopupMenuResource, POINT pt) {
    HMENU hMenu, hPopup = 0;

    hMenu = ::LoadMenu(GetModuleHandle(NULL),
                       MAKEINTRESOURCE(PopupMenuResource));

    if (hMenu != 0) {
      //POINT pt;
      //::GetCursorPos(&pt);

      // TrackPopupMenu cannot display the menu bar so get
      // a handle to the first shortcut menu.
      hPopup = ::GetSubMenu(hMenu, 0);

      // To display a context menu for a notification icon, the
      // current window must be the foreground window before the
      // application calls TrackPopupMenu or TrackPopupMenuEx. Otherwise,
      // the menu will not disappear when the user clicks outside of the menu
      // or the window that created the menu (if it is visible).
      ::SetForegroundWindow(m_hWnd);

      int cmd = ::TrackPopupMenu(hPopup, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x,
                                  pt.y, 0, m_hWnd, NULL);

      // See MS KB article Q135788
      ::PostMessage(m_hWnd, WM_NULL, 0, 0);

      // Clear up the menu, we're not longer using it.
      ::DestroyMenu(hMenu);
      return cmd;
    }
    return 0;
  }

private:
  bool m_bVisible;

  int m_CurrentIconResource;
  std::wstring m_CurrentText;
};

class DanmakuWnd : public ATL::CWindowImpl<DanmakuWnd> {

  using Base = ATL::CWindowImpl<DanmakuWnd>;

  comp::Compositor compositor{nullptr};
  comp::ContainerVisual root{nullptr};
  uicore::CoreDispatcher dispatcher{nullptr};

  danmaku::DanmakuManager &danmaku_mgr{danmaku::DanmakuManager::singleton()};

  std::unique_ptr<server::DanmakuServer> danmaku_srv;

public:

  BEGIN_MSG_MAP(DanmakuWnd)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  END_MSG_MAP()

  DECLARE_WND_CLASS(_T("DanmakuOverlay"));

  static DanmakuWnd &singleton() {
    static DanmakuWnd wnd;
    return wnd;
  }

  void TestDanmakuSingleIssueMode() {
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

    threading::ThreadPoolTimer::CreatePeriodicTimer([this](auto const &){
      for (int i=0; i<20; i++) {
        danmaku_mgr.IssueDanmaku(gen_text());
      }
    }, 300ms);
  }

  LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL &) {
    // Refer to https://github.com/dechamps/RudeWindowFixer
    winrt::check_bool(SetPropW(m_hWnd, L"NonRudeHWND", INVALID_HANDLE_VALUE));
    SetIcon(LoadIcon(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_DANMAKU)), TRUE);

    dispatcher = dcomp::GetOrCreateCoreDispatcher();
    std::tie(compositor, root) = dcomp::CreateCompositorForWindow(m_hWnd);

    danmaku_mgr.Initialize(compositor, root);

    //TestDanmakuSingleIssueMode();
    //TestDanmakuCanvasMode();

    danmaku_srv = std::make_unique<server::DanmakuServer>([&](std::wstring const &danmaku){
      danmaku_mgr.IssueDanmaku(danmaku);
    });

    return 0;
  }

  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL &) {
    PostQuitMessage(0);
    return 0;
  }

  static LPCTSTR GetWndCaption() {
    return _T("Danmaku Overlay");
  }

  static DWORD GetWndStyle(DWORD dwStyle) {
    return dwStyle | WS_POPUPWINDOW;
  }

  static DWORD GetWndExStyle(DWORD dwStyle) {
    return dwStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST
      | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW;
  }
};

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {

  auto &wnd = DanmakuWnd::singleton();
  auto &tray_icon = ShellIconWnd::singleton();
  auto &settings = SettingsWnd::singleton();

  winrt::check_hresult(::MileXamlGlobalInitialize());
  //auto xaml_app = winrt::make<winrt::DanmakuServer::implementation::App>();
  auto app = winrt::Windows::UI::Xaml::Application::Current();
  {
    winrt::Windows::UI::Xaml::ResourceDictionary dict;
    dict.Source(found::Uri(L"ms-appx:///Mile.Xaml.Styles.SunValley.xbf"));
    app.Resources().MergedDictionaries().Append(
      dict
    );

    //<local:BooleanToVisibilityConverter x:Key="TrueToVisibleConverter" Positive="True" />
    //<local:BooleanToVisibilityConverter x:Key="FalseToVisibleConverter" Positive="False" />

    winrt::DanmakuServer::BooleanToVisibilityConverter posi_conv, nega_conv;
    posi_conv.Positive(true);
    nega_conv.Positive(false);

    app.Resources().Insert(winrt::box_value(L"TrueToVisibleConverter"), posi_conv);
    app.Resources().Insert(winrt::box_value(L"FalseToVisibleConverter"), nega_conv);
  }

  tray_icon.SetShellVisible();

  tray_icon.SetShellIcon(IDI_DANMAKU);
  tray_icon.SetShellTipText(L"DanmakuServer listening on :7654");

  auto scr_width = GetSystemMetrics(SM_CXSCREEN);
  auto scr_height = GetSystemMetrics(SM_CYSCREEN);

  {
    RECT rect_full{0, 0, scr_width, scr_height};
    wnd.Create(NULL, rect_full);
  }

  {
    constexpr auto SETTINGS_WIDTH = 1200, SETTINGS_HEIGHT = 900;
    RECT rect_settings{
      (scr_width - SETTINGS_WIDTH) / 2,
      (scr_height - SETTINGS_HEIGHT) / 2,
      (scr_width + SETTINGS_WIDTH) / 2,
      (scr_height + SETTINGS_HEIGHT) / 2};
    auto page = winrt::make<winrt::DanmakuServer::implementation::SettingsPage>();
    settings.Create(NULL, rect_settings, NULL, 0, 0, 0U, winrt::get_abi(page));
  }

  wnd.ShowWindow(true);
  wnd.SetActiveWindow();

  auto dispatcher = dcomp::GetOrCreateCoreDispatcher();
  dispatcher.ProcessEvents(uicore::CoreProcessEventsOption::ProcessUntilQuit);

  wnd.DestroyWindow();
  settings.DestroyWindow();

  // destroy icon for window release
  tray_icon.SetShellVisible(false);

  winrt::check_hresult(::MileXamlGlobalUninitialize());

  return 0;
}
