# Phase 0 — PostMessage Probe Test

**Est:** 30 phút
**Priority:** P0 — gate cho Phase 2
**Status:** code-ready (awaiting user probe run)

## Implemented
- `tools/postmessage-probe/probe.cpp` — probe binary
- `tools/postmessage-probe/CMakeLists.txt`
- Root `CMakeLists.txt`, `vcpkg.json`, `.gitignore`
- `docs/input-backend-decision.md` — template for user to fill after running probe

## Build (user)
```
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target postmessage_probe
.\build\bin\Release\PostMessageProbe.exe
```


## Mục Tiêu
Xác định PT của user có accept `PostMessage` cho keyboard/mouse không. Quyết định input backend cho toàn bộ tool.

## Context
- Brainstorm Section 37 (PostMessage Input Backend)
- 85% PT classic accept WM_KEYDOWN/WM_RBUTTONDOWN; cần verify cụ thể server user dùng.
- Nếu YES → backend `PostMessage` (user multitask, không chiếm chuột)
- Nếu NO → backend `SendInput` (foreground required, kém UX)

## Implementation

### File: `tools/postmessage-probe/probe.cpp` (~80 LOC)

```cpp
#include <windows.h>
#include <iostream>

int main() {
    HWND h = FindWindow(NULL, L"Priston Tale");
    if (!h) { std::cerr << "Game window not found\n"; return 1; }
    
    std::cout << "Found HWND. Alt-tab sang Notepad trong 3s...\n";
    Sleep(3000);
    
    // Test 1: F2 background
    PostMessage(h, WM_KEYDOWN, VK_F2, 0);
    Sleep(50);
    PostMessage(h, WM_KEYUP, VK_F2, 0);
    Sleep(500);
    
    // Test 2: right-click (400, 300)
    LPARAM lp = MAKELPARAM(400, 300);
    PostMessage(h, WM_RBUTTONDOWN, MK_RBUTTON, lp);
    Sleep(20);
    PostMessage(h, WM_RBUTTONUP, 0, lp);
    Sleep(500);
    
    // Test 3: SHIFT+right-click
    PostMessage(h, WM_KEYDOWN, VK_LSHIFT, 0);
    PostMessage(h, WM_RBUTTONDOWN, MK_RBUTTON | MK_SHIFT, lp);
    Sleep(20);
    PostMessage(h, WM_RBUTTONUP, MK_SHIFT, lp);
    PostMessage(h, WM_KEYUP, VK_LSHIFT, 0);
    
    std::cout << "Probe sent. Check game.\n";
    return 0;
}
```

### CMakeLists.txt nhỏ riêng cho probe
```cmake
add_executable(postmessage_probe tools/postmessage-probe/probe.cpp)
target_link_libraries(postmessage_probe PRIVATE user32)
```

## Steps
1. Mở PT offline (private server không XC3 — KHÔNG dùng official ở Phase 0)
2. Build `postmessage_probe.exe` (Release x64)
3. Chạy probe, alt-tab sang Notepad ngay khi prompt
4. Quan sát PT:
   - F2 skill có active visible? → keys OK
   - Mob ở (400, 300) có bị đánh? → mouse OK
   - Char attack-in-place với SHIFT? → SHIFT modifier OK

## Decision Matrix

| Kết quả | Backend chốt | Ghi chú |
|---|---|---|
| Cả keys + mouse + SHIFT đều works | `PostMessage` | UX tốt nhất — user multitask |
| Chỉ keys works, mouse fail | `Hybrid` (PostMessage keys + SendInput mouse) | Foreground khi attack, background khi pot |
| Tất cả fail | `SendInput` only | Foreground required |

Ghi quyết định vào `docs/input-backend-decision.md` (1 dòng) — Phase 2 đọc.

## Success Criteria
- [ ] Build probe success
- [ ] Test 3 scenarios (F2, right-click, SHIFT+right-click)
- [ ] Ghi quyết định backend vào file
- [ ] Document hành vi cụ thể của PT user

## Risks
- PT version khác nhau, server private custom → kết quả khác nhau. Document chi tiết version client trong note.
- DirectInput exclusive mode chỉ kick in ở fullscreen → probe ở windowed có thể không reflect đúng nếu user dự định fullscreen.

## Next
Phase 2 implementation phụ thuộc backend chốt ở đây.
