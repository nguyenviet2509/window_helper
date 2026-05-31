# Phase 03 — Anti Static-Scan

## Context Links
- Brainstorm §#3
- Files: `src/main.cpp`, `CMakeLists.txt`

## Overview
- Priority: P2
- Status: pending
- Random window title runtime + random OUTPUT_NAME build-time để né Xingcode3 string scan.

## Key Insights
- Xingcode3 scan dựa trên process name + window title fixed strings.
- Runtime random title đủ né behavioral scan; build-time random tên exe né static signature.
- UX: title nhìn lạ — tray tooltip giữ tên thật để user nhận diện.

## Requirements
**Functional**
- Window title format: pool of plausible names, e.g. `["Notepad - {n}.txt", "Document{n} - Word", "Untitled - Paint"]`, n=random int.
- CMake: `set(OUTPUT_NAME "helper_${BUILD_SUFFIX}")` với suffix random qua `string(RANDOM ...)` hoặc env var.
- Tray tooltip: "WindowHelper (real)" — giữ identify được.

**Non-functional**
- Title set 1 lần lúc CreateWindow; không thay đổi runtime (tránh redraw).
- CMake change không phá clean build.

## Architecture
```
main.cpp:
  string title = pickRandomTitle(rng);  // chọn từ pool
  CreateWindowEx(..., title.c_str(), ...);

CMakeLists.txt:
  string(RANDOM LENGTH 6 ALPHABET "abcdefghijklmnopqrstuvwxyz" BUILD_SUFFIX)
  set_target_properties(WindowHelper PROPERTIES OUTPUT_NAME "svc_${BUILD_SUFFIX}")
```

## Related Code Files
**Modify**
- `src/main.cpp` — pickRandomTitle + dùng cho CreateWindow
- `CMakeLists.txt` — OUTPUT_NAME random
- `src/ui/tray-icon.cpp` — tooltip set "WindowHelper" hoặc giữ identifier

## Implementation Steps
1. Trong `main.cpp`, thêm static array `kFakeTitles[]` (3-5 entries) + helper `chooseTitle()` dùng `std::mt19937` seed `std::random_device`.
2. Thay literal "WindowHelper" trong CreateWindowEx bằng `chooseTitle()` result (lưu vào string biến local).
3. Cập nhật tray-icon tooltip giữ tên user-friendly.
4. CMake: thêm `string(RANDOM ...)` trên top, set OUTPUT_NAME property.
5. Build clean, verify exe có suffix.
6. Run, verify Task Manager / Window title hiển thị fake.

## Todo List
- [ ] Thêm title pool + chooser ở main.cpp
- [ ] Replace CreateWindow title literal
- [ ] Update tray tooltip
- [ ] CMake random OUTPUT_NAME
- [ ] Clean build → exe có suffix khác nhau 2 build liên tiếp
- [ ] Run + check title qua Spy++ hoặc tool list windows

## Success Criteria
- 2 clean build liền nhau → exe khác tên.
- Window title runtime KHÔNG chứa "WindowHelper" / "Helper".
- Tray vẫn cho user biết tool nào.

## Risk Assessment
| Risk | L | I | Mitigation |
|---|---|---|---|
| Title trùng app thật (Notepad) làm Spy++ confused | L | L | Acceptable; có thể thêm hidden marker class name |
| OUTPUT_NAME random làm CI/script find exe khó | M | L | Log suffix to `build/.last-name.txt` |
| `string(RANDOM)` cùng seed CMake → cùng kết quả | M | M | Dùng `${CMAKE_CURRENT_BINARY_DIR}` + time fallback, hoặc env var |

## Next Steps
- Phase 05 verify title qua external tool.
- Tier 2: random class name + icon resource.
