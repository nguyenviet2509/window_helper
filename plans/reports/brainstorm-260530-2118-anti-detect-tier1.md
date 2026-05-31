# Brainstorm — Anti-detect Tier 1 (chống Xingcode3 + GM observation)

**Ngày:** 2026-05-30 21:18
**Mục tiêu:** Nâng cấp tool tránh bị Xingcode3 (kernel anti-cheat) + GM phát hiện
**Phạm vi:** Tier 1 — 4 hạng mục ROI cao nhất

## Vấn đề

Tool hiện đã có Humanizer (jitter timing, miss-click, break window) và random click trong annulus. Nhưng còn lộ ở:

1. **Click "teleport"** — không có chuyển động chuột giữa các click → behavioral monitor catch ngay
2. **Phân phối uniform** trong annulus → ML detector dễ flag (người thật cụm click)
3. **Process name `WindowHelper.exe`** + window title cố định → Xingcode3 static scan
4. **SendInput backend** → bị driver hook ở kernel layer

## Giải pháp Tier 1 (đã chốt)

### #1 Mouse path (Bezier / minimum-jerk)
- Trước mỗi `sendShiftRightClick`/`sendRightClick`/`sendLeftClick`, sinh path từ vị trí chuột hiện tại → toạ độ đích
- Path: Bezier 3-điểm với control point lệch ngẫu nhiên, tổng thời gian 80-250ms tuỳ khoảng cách
- Gửi `WM_MOUSEMOVE` (PostMessage) hoặc `SendInput MOUSE_MOVE` từng waypoint, ~16-30ms/bước
- Module mới: `src/input/mouse-path.h` + `.cpp`

### #2 Click distribution bias
- Đổi annulus uniform → Gaussian quanh điểm "tâm cụm mob" (giả định trùng tâm cửa sổ)
- Đổi `attack-sweep.h`: `uniform_real_distribution` → `normal_distribution` với σ ≈ (rMax-rMin)/3, mean = (rMax+rMin)/2
- Clamp vào [rMin, rMax] để khỏi văng quá xa

### #3 Anti static-scan
- Build-time: random suffix cho `OUTPUT_NAME` (CMake variable hoặc build script)
- Runtime: random window title (e.g., `"Notepad - {random}.txt"`) thay vì `"WindowHelper"`
- Optional: tray icon dùng icon system thay icon custom

### #4 PostMessage-only path
- Đã có `postmessage-backend.cpp`. Cần:
  - Test xem game (Xingcode3) có chấp nhận click PostMessage không
  - Nếu có → đặt mặc định backend = PostMessage, ẩn SendInput option
  - Nếu không → giữ SendInput, đầu tư mouse path để giảm rủi ro

## Files dự kiến đụng đến

| File | Thay đổi |
|---|---|
| `src/input/mouse-path.h/.cpp` | **TẠO MỚI** — sinh Bezier path |
| `src/input/i-input-backend.h` | Thêm `sendMouseMove(x, y)` |
| `src/input/send-input-backend.cpp` | Implement `sendMouseMove` + tích hợp path vào click |
| `src/input/postmessage-backend.cpp` | Tương tự |
| `src/combat/attack-sweep.h` | Đổi distribution sang Gaussian clamped |
| `src/main.cpp` | Random window title runtime |
| `CMakeLists.txt` | Random output name (optional, có thể skip) |
| `src/config/config-loader.cpp` | Thêm config `defaultBackend` |

## Rủi ro

| Rủi ro | Mức | Mitigation |
|---|---|---|
| Mouse path chậm → mất nhịp đánh quái | Vừa | Path 80-250ms tuỳ distance, không quá 300ms |
| PostMessage không nhận click ở game | Cao | Test trước; fallback SendInput nếu fail |
| Gaussian clamp tạo cụm ở biên annulus | Thấp | Resample khi out-of-range thay vì clamp |
| Random window title gây UX khó tìm | Thấp | Vẫn show tên thật trong tray tooltip |

## Success criteria

- Mouse path: nhìn thấy cursor di chuyển mượt khi tool đánh quái (không teleport)
- Distribution: histogram click sau 1h chạy phải có cluster, không uniform
- Anti-scan: process khi build có suffix khác nhau mỗi build; window title không chứa "WindowHelper"
- PostMessage-only: game nhận đầy đủ click + key sau khi switch backend

## Out-of-scope (Tier 2-3)

- Idle/varied actions (mở inventory, xoay camera) — Tier 2
- Vary attack cooldown — Tier 2
- Mob detector / vision targeting — Tier 3
- Hardware HID (Arduino) — Tier 3, ngoài scope code

## Unresolved questions

1. **Game có nhận click qua PostMessage không?** Cần probe trước khi đặt mặc định.
2. **Có cần ẩn cả tray icon** không? (Tier 1 chưa làm vì UX impact).
3. **Mouse path khi click pot (HP/MP)** — có nên skip path để bơm bình tức thì? (Đề xuất: skip để giữ priority P0).
