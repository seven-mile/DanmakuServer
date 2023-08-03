#include "pch.h"

#include <tuple>
#include <random>
#include <format>
#include <functional>
#include <queue>
#include <map>
#include <unordered_map>
#include <optional>

import dcomp;
import server;

#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "dwmapi")

namespace found = winrt::Windows::Foundation;
namespace ui = winrt::Windows::UI;
namespace comp = ui::Composition;
namespace uicore = ui::Core;
namespace threading = winrt::Windows::System::Threading;

using namespace std::chrono_literals;

template <typename T, typename U>
inline auto ceil_div(T a, U b) {
  return (a + b - 1) / b;
}

class DanmakuManager {
  using lane_id_t = int;
  using danmaku_t = comp::Visual;
  using deleter_t = std::function<void(danmaku_t)>;

  static constexpr float danmaku_height = 32;
  static constexpr auto duration = 10000ms;

  comp::Compositor compositor;
  comp::ContainerVisual root;
  comp::CompositionEasingFunction spring;

  std::mutex mtx;

  float scr_width = 1.f * GetSystemMetrics(SM_CXSCREEN);
  float scr_height = 1.f * GetSystemMetrics(SM_CYSCREEN);

  lane_id_t lane_cnt;
  std::map<lane_id_t, std::queue<std::tuple<danmaku_t, deleter_t>>> lane_state;

  // returns if this lane is ready to issue
  bool AllocLane(danmaku_t danmaku, deleter_t deleter, lane_id_t &id) {
    // lane count is just ~60, no need to optimize
    auto it = std::min_element(lane_state.begin(), lane_state.end(), [](auto &a, auto &b) {
      return a.second.size() < b.second.size();
    });
    bool issuable = it->second.empty();
    it->second.emplace(danmaku, deleter);
    id = it->first;
    return issuable;
  }

  void PopLane(lane_id_t id) {
    lane_state[id].pop();
  }

  float GetLanePosY(lane_id_t id) {
    return id * danmaku_height;
  }

  void IssueLane(lane_id_t lane_id) {
    auto &&[danmaku, deleter] = lane_state[lane_id].front();
    auto width = danmaku.Size().x;
    auto ypos = GetLanePosY(lane_id);

    auto batch = compositor.CreateScopedBatch(comp::CompositionBatchTypes::Animation);
    
    auto duration1 = std::chrono::duration_cast<found::TimeSpan>(
      duration * (width / (width + scr_width)));
    auto duration2 = duration - duration1;

    // Apply screen entry animation
    {
      auto anim = compositor.CreateVector3KeyFrameAnimation();
      anim.InsertKeyFrame(0.F, {scr_width, ypos, 0}, spring);
      anim.InsertKeyFrame(1.F, {scr_width-width, ypos, 0}, spring);
      anim.Duration(duration1);
      anim.IterationBehavior(comp::AnimationIterationBehavior::Count);
      anim.IterationCount(1);
      danmaku.StartAnimation(L"Offset", anim);
    }

    batch.End();

    // When the danmaku is off the screen edge,
    // free the lane and start the remaining animation
    batch.Completed([=, this](auto &, auto &){
      std::lock_guard g{mtx};
      // Pop issuing for other danmaku
      assert(std::get<0>(lane_state[lane_id].front()) == danmaku);
      PopLane(lane_id);
      // Issue right after the vacancy
      if (lane_state[lane_id].size())
        IssueLane(lane_id);

      auto batch = compositor.CreateScopedBatch(comp::CompositionBatchTypes::Animation);
  
      auto ypos = danmaku.Offset().y;
      auto width = danmaku.Size().x;

      // Apply screen entry animation
      {
        // it's a tunable position offset to
        // pretend the animation is connected
        constexpr int CPU_OFFSET = 0;
        auto anim = compositor.CreateVector3KeyFrameAnimation();
        anim.InsertKeyFrame(0.F, {scr_width-width-CPU_OFFSET, ypos, 0}, spring);
        anim.InsertKeyFrame(1.F, {-width, ypos, 0}, spring);
        anim.Duration(duration2);
        anim.IterationBehavior(comp::AnimationIterationBehavior::Count);
        anim.IterationCount(1);
        danmaku.StartAnimation(L"Offset", anim);
      }

      batch.End();

      batch.Completed([deleter, danmaku](auto &, auto &) {
        // free it
        deleter(danmaku);
      });
    });
  }

public:

  DanmakuManager(comp::Compositor compositor, comp::ContainerVisual root)
    : compositor(compositor), root(root),
      spring(compositor.CreateLinearEasingFunction()) {
    lane_cnt = int(floor(scr_height / danmaku_height - 1));
    for (int i = 0; i < lane_cnt; ++i) {
      lane_state[i] = {};
    }
  }

  std::tuple<danmaku_t, lane_id_t> CreateDanmaku(std::wstring const &text, deleter_t deleter) {
    std::lock_guard g{mtx};

    static std::default_random_engine rd{998244353};
    std::uniform_int_distribution rgb_gen{0, 255};

    
    auto danmaku = compositor.CreateContainerVisual();
    lane_id_t lane_id{-1};
    bool issuable = AllocLane(danmaku, deleter, lane_id);
    auto ypos = GetLanePosY(lane_id);

    OutputDebugString(std::format(L"Allocated lane {}, with ypos = {}; "
      "now has {} danmaku(s)\n", lane_id, ypos, lane_state[lane_id].size()).c_str());

    danmaku.Offset({scr_width, ypos, 0});

    // construct danmaku visual
    {
      auto text_visual = dcomp::CreateTextVisual(compositor, text);

      auto drop_shadow = compositor.CreateDropShadow();
      drop_shadow.BlurRadius(5.f);
      drop_shadow.Opacity(1.f);
      drop_shadow.Color(ui::Colors::Black());

      auto text_mask = compositor.CreateMaskBrush();
      {
        auto text_mask_brush = compositor.CreateColorBrush();
        text_mask_brush.Color(ui::Colors::Black());
        text_mask.Source(text_mask_brush);
        text_mask.Mask(text_visual.Brush());
      }
      drop_shadow.Mask(text_mask);

      text_visual.Shadow(drop_shadow);

      danmaku.Children().InsertAtTop(text_visual);
      danmaku.Size({text_visual.Size().x, danmaku_height});
    }

    if (issuable)
      IssueLane(lane_id);

    return {danmaku, lane_id};
  }

  void IssueDanmaku(std::wstring const &text) {
    auto &&[danmaku, lane_id] = CreateDanmaku(text, [this](comp::Visual danmaku){
      danmaku.StopAnimation(L"Offset");
      // remove to free memory
      root.Children().Remove(danmaku);
    });
    root.Children().InsertAtTop(danmaku);
  }

};

class DanmakuWnd : public ATL::CWindowImpl<DanmakuWnd> {

  using Base = ATL::CWindowImpl<DanmakuWnd>;

  comp::Compositor compositor{nullptr};
  comp::ContainerVisual root{nullptr};
  uicore::CoreDispatcher dispatcher{nullptr};

  std::unique_ptr<DanmakuManager> danmaku_mgr;
  std::unique_ptr<server::DanmakuServer> danmaku_srv;

public:

  BEGIN_MSG_MAP(DanmakuWnd)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  END_MSG_MAP()

  DECLARE_WND_CLASS(_T("DanmakuOverlay"));

  static constexpr float OPACITY = 1.F;

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
      for (int i=0; i<2; i++) {
        danmaku_mgr->IssueDanmaku(gen_text());
      }
    }, 300ms);
  }

  LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL &) {
    dispatcher = dcomp::GetOrCreateCoreDispatcher();
    std::tie(compositor, root) = dcomp::CreateCompositorForWindow(m_hWnd);

    danmaku_mgr = std::make_unique<DanmakuManager>(compositor, root);

    root.Opacity(OPACITY);

    //TestDanmakuSingleIssueMode();
    //TestDanmakuCanvasMode();

    danmaku_srv = std::make_unique<server::DanmakuServer>([&](std::wstring const &danmaku){
      danmaku_mgr->IssueDanmaku(danmaku);
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
      | WS_EX_NOREDIRECTIONBITMAP;
  }
};

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {

  DanmakuWnd wnd{};

  auto scr_width = GetSystemMetrics(SM_CXSCREEN);
  auto scr_height = GetSystemMetrics(SM_CYSCREEN);

  RECT rect{0, 0, scr_width, scr_height};
  wnd.Create(NULL, rect);

  wnd.ShowWindow(TRUE);
  wnd.SetActiveWindow();

  auto dispatcher = dcomp::GetOrCreateCoreDispatcher();

  dispatcher.ProcessEvents(uicore::CoreProcessEventsOption::ProcessUntilQuit);

  wnd.DestroyWindow();

  return 0;
}
