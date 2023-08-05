module;
#include "pch.h"

#include "config.hpp"

#include <limits>
export module danmaku;

import dcomp;

export namespace danmaku {

class DanmakuManager {
  using lane_id_t = int;
  using danmaku_t = comp::Visual;
  using deleter_t = std::function<void(danmaku_t)>;


  comp::Compositor compositor;
  comp::ContainerVisual root;
  comp::CompositionEasingFunction spring;

  std::recursive_mutex mtx;

  // TODO: screen size change awareness
  float scr_width = 1.f * GetSystemMetrics(SM_CXSCREEN);
  float scr_height = 1.f * GetSystemMetrics(SM_CYSCREEN);
  
  float danmaku_height = 32;
  found::TimeSpan duration = 10000ms;

  lane_id_t lane_cnt;
  std::map<lane_id_t, std::queue<std::tuple<danmaku_t, deleter_t>>> lane_state;

  // returns if this lane is ready to issue
  bool AllocLane(danmaku_t danmaku, deleter_t deleter, lane_id_t &id) {
    auto idmin = std::numeric_limits<size_t>::max();
    
    for (int jd = 0; jd < lane_cnt; jd++) {
      auto &lane = lane_state[jd];

      if (lane.size() < idmin)
        std::tie(id, idmin) = std::tuple(jd, lane.size());
    }

    lane_state[id].emplace(danmaku, deleter);

    return idmin == 0;
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
    
    auto update_lane_cnt = [this](){
      std::lock_guard g{mtx};
      lane_cnt = std::min(
        config::DanmakuConfig::singleton().RequestedLaneCount(),
        int(floor(scr_height / danmaku_height - 1))
      );
    };

    auto &cfg = config::DanmakuConfig::singleton();

    cfg.ListenRequestedLaneCount([=](auto&){
      update_lane_cnt();
    }, true); // immediate init

    cfg.ListenOpacity([this](auto &value){
      std::lock_guard g{mtx};
      this->root.Opacity(value);
    }, true);

    cfg.ListenDuration([this](auto &value){
      std::lock_guard g{mtx};
      duration = value;
    }, true);

    cfg.ListenDanmakuHeight([=, this](auto &value){
      std::lock_guard g{mtx};
      danmaku_height = value;
      update_lane_cnt();
    }, true);

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

}