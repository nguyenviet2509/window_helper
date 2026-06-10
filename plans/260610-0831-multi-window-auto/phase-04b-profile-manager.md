---
phase: 4b
title: ProfileManager + assignment persist
status: pending
priority: P1
effort: 0.5d
---

# Phase 4b — ProfileManager

## Context
- Mỗi PerWindowContext cần `AppConfig` riêng load từ profile file.
- User assign profile qua UI (Phase 5); app nhớ assignment qua `lastAssignment.json`.
- Migration: lần đầu chạy → tạo `profiles/profile-Default.json` từ `config.json` (hoặc defaults).

## Files to create
- `src/config/profile-manager.h` (~50 LOC)
- `src/config/profile-manager.cpp` (~120 LOC)

## API design

```cpp
// profile-manager.h
#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include "config-loader.h"

class ProfileManager {
public:
    // dir = <exe>/profiles
    explicit ProfileManager(std::filesystem::path dir);

    // List profile names (không bao gồm .json).
    std::vector<std::string> list() const;

    // Load profile theo tên. Trả false nếu file không tồn tại — caller dùng defaults.
    bool load(const std::string& name, AppConfig& out);

    // Save profile (overwrite). Tạo dir nếu chưa có.
    bool save(const std::string& name, const AppConfig& cfg);

    // Create new profile từ template (seed = current cfg hoặc defaults).
    bool create(const std::string& name, const AppConfig& seed);

    // Rename / delete.
    bool rename(const std::string& oldName, const std::string& newName);
    bool remove(const std::string& name);

    bool exists(const std::string& name) const;

    // Assignment persist: { "W0": "MainChar", "W1": "AltChar" }.
    using Assignment = std::unordered_map<std::string, std::string>;
    Assignment loadAssignment() const;       // empty nếu file chưa có
    bool       saveAssignment(const Assignment& a);

    // Migration: nếu profiles/ rỗng, seed profile "Default" từ config.json.
    void ensureDefaultProfile(const AppConfig& seedFromGlobal);

private:
    std::filesystem::path dir_;
    std::filesystem::path assignPath_;
    ConfigLoader loader_;
};
```

## File layout

```
<exe-dir>/
├── config.json                   ← giữ làm fallback/default (existing)
├── lastAssignment.json           ← { "W0": "MainChar", "W1": "AltChar" }
└── profiles/
    ├── profile-Default.json      ← auto-seed nếu chưa có
    ├── profile-MainChar.json
    └── profile-AltChar.json
```

### Profile file format
Y hệt `config.json` hiện tại (cùng schema AppConfig). Reuse `ConfigLoader`.

### lastAssignment.json format
```json
{
  "W0": "MainChar",
  "W1": "AltChar"
}
```

## Wiring in main.cpp (incremental change từ Phase 4)

```cpp
ProfileManager pm(exeDir() / "profiles");
pm.ensureDefaultProfile(cfg);  // cfg = current config.json load
auto assignment = pm.loadAssignment();

for (size_t i = 0; i < targets.size(); ++i) {
    auto ctx = std::make_unique<PerWindowContext>();
    ctx->hwnd = targets[i]; ctx->index = (int)i;
    std::string key = "W" + std::to_string(i);
    auto it = assignment.find(key);
    ctx->profileName = (it != assignment.end()) ? it->second : "Default";
    if (!pm.load(ctx->profileName, ctx->cfg)) {
        ctx->cfg = cfg;  // fallback
        ctx->profileName = "Default";
    }
    ctx->bus.publish(std::make_shared<const AppConfig>(ctx->cfg));
    // ... rest of pipeline construction using ctx->cfg ...
}
```

## Hot-reload contract
- UI edit (Phase 5) → debounced save:
  ```
  pm.save(ctx->profileName, ctx->cfg)
  ctx->bus.publish(...)
  ctx->dispatcher->updateConfig(ctx->cfg)
  ctx->refill->enable(ctx->cfg.refill.enabled)
  // CombatFsm: nếu chưa có updateConfig, thêm setter (xem phase-05).
  ```

## Todo
- [ ] Tạo `profile-manager.h/.cpp`.
- [ ] Unit test thủ công: create/list/load/save/rename/delete.
- [ ] Migration: lần đầu chạy không có `profiles/` → tạo + seed Default.
- [ ] `lastAssignment.json` save sau mỗi lần user đổi profile trong UI.
- [ ] Add to CMakeLists.

## Success criteria
- Lần đầu chạy: tự tạo `profiles/profile-Default.json` từ `config.json`.
- Restart app → load đúng profile từ `lastAssignment.json`.
- Rename profile A → B → file rename + `lastAssignment.json` update tự động.

## Risks
- Tên profile chứa ký tự illegal trên filesystem → validate (alphanum + dash/underscore).
- Concurrent save lúc UI edit nhanh → ConfigLoader save không atomic; rủi ro corrupt file. Mitigation: write-to-temp-then-rename.

## Open
- Có cần backup `profile-{name}.json.bak` trước save không? Tạm bỏ — git/file history đủ.
