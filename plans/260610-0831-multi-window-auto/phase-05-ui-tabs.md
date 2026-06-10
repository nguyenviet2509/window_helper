---
phase: 5
title: MainWindow tabs + per-tab config editor
status: pending
priority: P1
effort: 2d
---

# Phase 5 — UI Tabs + per-tab config editor

## Context
- UI dùng `ImGui::BeginTabBar` — mỗi PerWindowContext = 1 tab.
- Mỗi tab có: profile selector (dropdown + CRUD), status panel, AUTO/BUFF toggle, collapsible config editor cho full AppConfig.
- Edit field → debounced 300ms → `ProfileManager.save` → publish per-window cfg → live update CombatFsm / Dispatcher / Refill.

## Files to modify
- `src/ui/main-window.h` / `.cpp` — refactor render thành tab-based.
- Có thể tách `src/ui/config-editor.{h,cpp}` (~250 LOC) để giữ main-window < 200 LOC.

## API design

```cpp
// main-window.h
class MainWindow {
public:
    using ContextList = std::vector<std::unique_ptr<PerWindowContext>>;

    void setContexts(ContextList* ctxs);
    void setProfileManager(ProfileManager* pm);
    void setArbiterStats(const ForegroundArbiter* a);

    // Callbacks now take window index.
    void setOnCombatToggle(std::function<void(int idx, bool on)>);
    void setOnBuffToggle(std::function<void(int idx, bool on)>);
    void setOnConfigChanged(std::function<void(int idx, const AppConfig&)>);
    void setOnProfileBindChanged(std::function<void(int idx, const std::string& name)>);

    void toggleCombatRequested(int idx = -1);  // -1 = all (F8)
    void toggleBuffRequested(int idx = -1);

private:
    ContextList* ctxs_ = nullptr;
    ProfileManager* pm_ = nullptr;
    const ForegroundArbiter* arbiter_ = nullptr;

    // Per-tab debounce timer cho config edit.
    std::vector<std::chrono::steady_clock::time_point> dirtyAt_;
    std::vector<bool> dirty_;
    // ... existing fields
};
```

## Render structure (ImGui pseudocode)

```cpp
ImGui::Begin("Window Helper");

if (ImGui::BeginTabBar("windows")) {
    for (size_t i = 0; i < ctxs_->size(); ++i) {
        auto& c = (*ctxs_)[i];
        std::string label = "W" + std::to_string(i) + ": " + c->profileName + "###tab" + std::to_string(i);
        if (ImGui::BeginTabItem(label.c_str())) {
            renderTab(i, *c);
            ImGui::EndTabItem();
        }
    }
    ImGui::EndTabBar();
}

ImGui::Separator();
ImGui::Text("FG slots: %llu | fail: %llu",
            (unsigned long long)arbiter_->slotsServed(),
            (unsigned long long)arbiter_->fgFailures());

ImGui::End();
```

### `renderTab(i, ctx)` outline

```cpp
// 1. Profile selector
ImGui::Text("HWND 0x%p | %S", ctx.hwnd, ctx.title.c_str());
auto profiles = pm_->list();
const char* current = ctx.profileName.c_str();
if (ImGui::BeginCombo("Profile", current)) {
    for (auto& p : profiles) {
        bool sel = (p == ctx.profileName);
        if (ImGui::Selectable(p.c_str(), sel)) {
            if (p != ctx.profileName) onProfileBindChanged_(i, p);
        }
    }
    ImGui::EndCombo();
}
ImGui::SameLine(); if (ImGui::Button("New"))    openNewProfileModal(i);
ImGui::SameLine(); if (ImGui::Button("Save"))   pm_->save(ctx.profileName, ctx.cfg);
ImGui::SameLine(); if (ImGui::Button("Rename")) openRenameModal(i);
ImGui::SameLine(); if (ImGui::Button("Delete")) confirmDelete(i);

// 2. Status
auto s = ctx.vision->lastState();
ImGui::Text("HP %.0f%%  MP %.0f%%  SP %.0f%%  seq=%llu",
            s.hpPct*100, s.mpPct*100, s.spPct*100, (unsigned long long)s.seq);

// 3. Toggles
bool autoOn = ctx.combat->isEnabled();
if (ImGui::Checkbox("AUTO", &autoOn)) onCombatToggle_(i, autoOn);
ImGui::SameLine();
bool buffOn = ctx.combat->isBuffEnabled();
if (ImGui::Checkbox("BUFF", &buffOn)) onBuffToggle_(i, buffOn);

// 4. Config editor (collapsing headers)
bool changed = false;
if (ImGui::CollapsingHeader("Pot thresholds")) {
    changed |= ImGui::SliderFloat("HP <", &ctx.cfg.pot.hpThreshold, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::SliderFloat("MP <", &ctx.cfg.pot.mpThreshold, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::SliderFloat("SP <", &ctx.cfg.pot.spThreshold, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::SliderFloat("Recall HP <", &ctx.cfg.pot.hpRecallThreshold, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::InputInt("Cooldown ms", &ctx.cfg.pot.cooldownMs);
}
if (ImGui::CollapsingHeader("Combat")) {
    changed |= ImGui::InputInt("Attack cooldown ms", &ctx.cfg.combat.attackCooldownMs);
    changed |= ImGui::InputInt("Engagement lock ms", &ctx.cfg.combat.engagementLockMs);
    changed |= ImGui::InputInt("Engagement jitter ms", &ctx.cfg.combat.engagementLockJitterMs);
    changed |= ImGui::Checkbox("Mouse path",        &ctx.cfg.combat.enableMousePath);
    changed |= ImGui::Checkbox("Wait MP gate",      &ctx.cfg.combat.waitMpGate);
    changed |= ImGui::SliderFloat("MP gate >=",     &ctx.cfg.combat.waitMpGateThreshold, 0.0f, 1.0f, "%.2f");
}
if (ImGui::CollapsingHeader("Refill")) {
    changed |= ImGui::Checkbox("Enabled", &ctx.cfg.refill.enabled);
    changed |= ImGui::InputInt("HP interval s", &ctx.cfg.refill.hp.intervalSec);
    changed |= ImGui::InputInt("SP interval s", &ctx.cfg.refill.sp.intervalSec);
    changed |= ImGui::InputInt("MP interval s", &ctx.cfg.refill.mp.intervalSec);
}
if (ImGui::CollapsingHeader("Buffs")) {
    for (size_t b = 0; b < ctx.cfg.combat.buffs.size(); ++b) {
        ImGui::PushID((int)b);
        changed |= ImGui::Checkbox("##en",  &ctx.cfg.combat.buffs[b].enabled); ImGui::SameLine();
        ImGui::Text("F%zu", b+2); ImGui::SameLine();
        changed |= ImGui::InputInt("Rebuff s", &ctx.cfg.combat.buffs[b].rebuffIntervalSec);
        ImGui::PopID();
    }
}

if (changed) {
    dirty_[i] = true;
    dirtyAt_[i] = std::chrono::steady_clock::now();
}

// 5. Debounce → persist + publish
auto now = std::chrono::steady_clock::now();
if (dirty_[i] && (now - dirtyAt_[i]) > std::chrono::milliseconds(300)) {
    pm_->save(ctx.profileName, ctx.cfg);
    if (onConfigChanged_) onConfigChanged_(i, ctx.cfg);
    dirty_[i] = false;
}
```

## Modal dialogs

- **New profile**: text input tên + button. Validate alphanum/dash/underscore. `pm.create(name, ctx.cfg)` (copy current). Rebind tab → save assignment.
- **Rename**: input tên mới. `pm.rename(old, new)`; nếu trùng tên → reject. Update assignment.
- **Delete confirm**: nếu profile đang được tab khác dùng → cảnh báo + rebind tab về Default.

## Hot-reload callbacks (main.cpp wiring)

```cpp
win.setOnConfigChanged([&](int i, const AppConfig& c) {
    auto& ctx = *ctxs[i];
    ctx.bus.publish(std::make_shared<const AppConfig>(c));
    ctx.dispatcher->updateConfig(c);
    ctx.refill->enable(c.refill.enabled);
    // CombatFsm: thêm updateConfig(cfg.combat) nếu chưa có; áp các field runtime-tweak.
});

win.setOnProfileBindChanged([&](int i, const std::string& name) {
    auto& ctx = *ctxs[i];
    AppConfig loaded;
    if (pm.load(name, loaded)) {
        ctx.profileName = name;
        ctx.cfg = loaded;
        ctx.bus.publish(std::make_shared<const AppConfig>(ctx.cfg));
        ctx.dispatcher->updateConfig(ctx.cfg);
        ctx.refill->enable(ctx.cfg.refill.enabled);
        // Persist assignment.
        ProfileManager::Assignment a = pm.loadAssignment();
        a["W" + std::to_string(i)] = name;
        pm.saveAssignment(a);
    }
});
```

## CombatFsm needs `updateConfig`
- Hiện CombatFsm nhận `cfg.combat` qua ctor. Thêm:
  ```cpp
  void CombatFsm::updateConfig(const CombatConfig& c);  // copy-into với mutex/atomic swap
  ```
- Buff slots changes: rebuild internal buff schedule.

## Modularization
- Tách `config-editor.{h,cpp}` hold các collapsing-header render fn → main-window <200 LOC.
- `profile-modal.{h,cpp}` cho 3 modal (new/rename/delete).

## Todo
- [ ] Add `VisionPipeline::lastState()` snapshot.
- [ ] Add `CombatFsm::updateConfig`.
- [ ] Refactor MainWindow → tab-based.
- [ ] Implement config-editor (collapsing sections).
- [ ] Profile selector + 3 modals.
- [ ] Wire callbacks in main.cpp.
- [ ] Tray group toggle giữ F8 (-1 = all).
- [ ] Save initial window size đủ rộng (UI ~700px).

## Success criteria
- 2 tab hiển thị; switch tab nhanh, không crash.
- Edit threshold trong tab W0 → 300ms sau profile-W0.json updated; W1 không ảnh hưởng.
- Đổi profile binding W0 → load đúng cfg, AUTO toggle ON vẫn giữ behavior.
- F8 toggle cả 2 cùng lúc.
- Rename profile khi in-use: tab cập nhật tên + assignment file updated.

## Risks
- ImGui::BeginCombo + dynamic list rebuild mỗi frame → OK với <50 profile.
- Race khi user đang edit field mà `ctx.cfg` bị publish update từ luồng khác → giữ ownership cfg trong UI thread; publish 1-way từ UI ra.
- Delete profile đang dùng cả 2 tab → rebind cả 2 về Default.

## Open
- Có cần "Apply to all tabs" button (copy current cfg sang tab khác)? Tạm bỏ — user manual.
- Field render cho `hpKey/mpKey/spKey/mainAttackKey` (VK code) → cần widget pick key; tạm dùng InputInt VK hex (phase sau làm picker đẹp hơn).
