---
phase: 6
title: ProfileManager + assignment persist (match by title+rect)
status: pending
priority: P1
effort: 0.75d
---

# Phase 6 — ProfileManager

## Context
Reuse Phase 4b plan cũ (`260610-0831/phase-04b-profile-manager.md`) + thêm matching logic cho hot-plug: khi PT đóng và mở lại, HWND mới ≠ HWND cũ → cần match profile assignment qua title + last known rect.

## Files to create / modify
- `src/config/profile-manager.h` (~80 LOC)
- `src/config/profile-manager.cpp` (~200 LOC)
- `src/config/app-config.h` — chia đôi: shared part vs per-profile part (Phase 4b plan cũ đã design; reuse).

## Storage layout
```
profiles/
  ├─ profile-Default.json
  ├─ profile-MainChar.json
  ├─ profile-Alt1.json
  └─ ...
lastAssignment.json   # key → profileName
```

### lastAssignment.json schema
```json
{
  "assignments": [
    {
      "key": "PristonTale|960x540|0,0",
      "title": "PristonTale",
      "rectW": 960,
      "rectH": 540,
      "rectX": 0,
      "rectY": 0,
      "profileName": "MainChar",
      "lastSeenIso": "2026-06-10T09:30:00Z"
    },
    ...
  ]
}
```

## ProfileManager API

```cpp
class ProfileManager {
public:
    explicit ProfileManager(std::filesystem::path dir);

    bool load(const std::string& name, AppConfig& out);
    bool save(const std::string& name, const AppConfig& cfg);
    std::vector<std::string> list() const;
    bool create(const std::string& name, const AppConfig& templ);
    bool rename(const std::string& oldName, const std::string& newName);
    bool remove(const std::string& name);

    void ensureDefaultProfile(const AppConfig& templ);

    // Assignment.
    using AssignmentMap = std::map<std::string, std::string>;  // key → profile
    AssignmentMap loadAssignment();
    bool saveAssignment(const AssignmentMap& m);

    // Match HWND to assignment key.
    // Strategy: exact match key first; fallback to title-only; fallback "Default".
    std::string matchKey(const std::wstring& title, const RECT& rect) const;
    std::string buildKey(const std::wstring& title, const RECT& rect) const;

    // Bind: gọi khi user assign profile cho 1 window.
    void bind(const std::wstring& title, const RECT& rect, const std::string& profileName);

private:
    std::filesystem::path dir_;
    mutable std::mutex mu_;
    AssignmentMap assignment_;
};
```

### Key format
```
{title-trimmed}|{rectW}x{rectH}|{rectX},{rectY}
```
- Title trim leading/trailing spaces, lowercase.
- Rect rounded to nearest 10px (forgive small repos move).

### Match algorithm
1. Build key từ current title + rect → exact match in `assignment_`.
2. Nếu miss → match by title (ignore rect) → return first.
3. Nếu vẫn miss → return "Default".

## AppConfig split (reuse plan cũ design)
```cpp
// Per-profile (1 file per profile)
struct AppConfig {
    BarConfig bar;          // HP/MP/SP region — shared assumption same resolution
    CombatConfig combat;
    RefillConfig refill;
    BuffConfig buffs;
    PauseConfig pause;
    // Shared fields cloned to each profile for simplicity (KISS).
};
```

## Migration
- Khi `profile-Default.json` không tồn tại + `config.json` cũ tồn tại → copy config → profile-Default.json. Giữ config.json làm legacy fallback.

## Todo
- [ ] `profile-manager.h/.cpp`.
- [ ] JSON load/save (reuse existing config json util).
- [ ] `matchKey` logic + unit test.
- [ ] Migration on first run.
- [ ] CMakeLists.

## Success criteria
- 2 PT mở cùng resolution, profile khác nhau, đóng cả 2, mở lại theo thứ tự ngược → mỗi PT vẫn nhận đúng profile (qua title+rect match).
- Profile rename trong UI → assignment cập nhật (Phase 7).
- Migration: config.json → profile-Default.json không mất field.

## Risks
- Title trùng cả 2 PT, rect khác nhau → rect-based match phân biệt. OK.
- Title trùng + rect trùng (cùng resolution, cùng vị trí) → ambiguous. Trường hợp này hiếm trong thực tế (user thường arrange windows side-by-side). Fallback first match — acceptable.
- Profile file corrupt → load fail → use Default + log warn.
