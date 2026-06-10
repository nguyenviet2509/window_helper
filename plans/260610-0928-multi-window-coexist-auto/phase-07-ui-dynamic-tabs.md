---
phase: 7
title: MainWindow dynamic tabs + per-tab config + pause indicator
status: pending
priority: P1
effort: 2d
---

# Phase 7 — UI dynamic tabs

## Context
Reuse Phase 5 plan cũ (`260610-0831/phase-05-ui-tabs.md`) + thêm:
- Tab động (add/remove theo lifecycle events).
- Pause indicator (global + per-window).
- F9 manual pause button.
- Profile assignment dropdown per tab.

## Files to modify
- `src/ui/main-window.h/.cpp` (lớn — ~400-500 LOC).

## Layout

```
┌─ Header ──────────────────────────────────────────┐
│ [●] User: ACTIVE / IDLE   [Manual Pause: OFF] [F9]│
│ Windows detected: 2/3                             │
├─ Tabs ────────────────────────────────────────────┤
│ [ W0: PristonTale ] [ W1: PristonTale ] [ + ]     │
├─ Tab content (W0) ────────────────────────────────┤
│ Status: AUTO ON   Paused: NO                      │
│ Profile: [MainChar ▼] [New] [Rename] [Delete]     │
│                                                   │
│ ┌─ Combat ───┐ ┌─ Refill ───┐ ┌─ Buffs ───┐       │
│ │ ...        │ │ ...        │ │ ...        │       │
│ └────────────┘ └────────────┘ └────────────┘       │
│                                                   │
│ ┌─ Pause settings (global) ─────────────┐         │
│ │ Idle threshold: [3000ms]              │         │
│ │ Mouse idle threshold: [5000ms]        │         │
│ └───────────────────────────────────────┘         │
├─ Footer ──────────────────────────────────────────┤
│ Logs (filtered for W0): ...                       │
└───────────────────────────────────────────────────┘
```

## API additions

```cpp
class MainWindow {
public:
    // Lifecycle integration.
    void onWindowAdded(PerWindowContext* ctx);     // thread-safe; enqueue UI event
    void onWindowRemoved(HWND hwnd, int index);
    void setPauseGate(PauseGate* gate);
    void setUserActivityMonitor(UserActivityMonitor* mon);
    void setProfileManager(ProfileManager* pm);

private:
    void renderHeader();
    void renderTabs();
    void renderTabContent(PerWindowContext& ctx);
    void renderProfileSelector(PerWindowContext& ctx);
    void renderConfigEditor(PerWindowContext& ctx);
    void renderPauseSettings();
    void renderEmptyState();   // khi N=0

    std::mutex eventMu_;
    std::vector<std::function<void()>> pendingUiEvents_;  // applied on next frame

    std::vector<PerWindowContext*> contexts_;
    int activeTabIdx_ = 0;
    PauseGate* pause_ = nullptr;
    UserActivityMonitor* monitor_ = nullptr;
    ProfileManager* pm_ = nullptr;
};
```

## Dynamic tab behavior
- `onWindowAdded`/`onWindowRemoved` không touch ImGui directly (callbacks chạy trên lifecycle thread). Push lambda vào `pendingUiEvents_`; UI thread drain mỗi frame.
- Khi tab bị remove + đang active → switch về tab 0 hoặc empty state.
- Ghost tab: nếu user muốn giữ cấu hình khi PT đóng → optional "pin tab" feature — defer Phase 8.

## Per-tab config editor
- Tất cả field AppConfig (combat/refill/buffs/pause). Group bằng `ImGui::CollapsingHeader`.
- Save: button "Save profile" → `pm.save(ctx.profileName, ctx.cfg)`.
- Auto-apply trên thay đổi: ctx.bus.publish(new cfg) → hot-reload pipeline.

## Profile dropdown
- Combo box list profiles từ `pm.list()`.
- Change selection → load profile vào ctx.cfg + publish bus + save assignment via `pm.bind(title, rect, name)`.

## Pause indicator
- Header dot:
  - Green = idle (auto running)
  - Yellow = user active (paused)
  - Red = manual pause
- Per-tab badge "PAUSED" overlay khi `pause->isPaused(ctx.hwnd)`.

## Empty state
- 0 windows: hiển thị "Waiting for PristonTale window... (poll every 2.5s)" + button "Rescan now" (force lifecycle poll).

## Hot reload
- Khi ctx.cfg đổi via UI → `ctx.bus.publish(cfg)`. Combat/Refill/Vision subscribe → update internal state.

## Todo
- [ ] Refactor MainWindow → tabs API.
- [ ] Implement `pendingUiEvents_` queue.
- [ ] Profile dropdown + CRUD.
- [ ] Config editor sections (group bằng collapsing headers).
- [ ] Pause indicator + manual toggle.
- [ ] Empty state + Rescan button.
- [ ] Log filter per window (parse tag `[w{i}]`).

## Success criteria
- Mở/đóng PT runtime → tabs update real-time không crash.
- Profile switch → config hot-reload, không restart.
- Pause indicator phản ánh state đúng < 250ms.
- F9 toggle manual pause: UI cập nhật + tất cả windows pause/resume.

## Risks
- ImGui state race: render thread vs lifecycle thread → strict eventMu_ guard.
- Config editor lớn (~30 field) → grouping + scroll. Defer rare-use fields vào "Advanced" header.
- Tab remove khi đang edit field → user mất unsaved changes. Mitigation: prompt save trước remove (defer; chấp nhận data loss đợt đầu).
