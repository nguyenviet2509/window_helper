---
type: brainstorm-report
date: 2026-06-10
slug: calibration-and-window-pin
status: approved
amendsPlan: ../plan.md
---

# Amendment: Calibration UI + Window Pin + Audit

## Problem
User report: di chuyển cửa sổ PT → vision HP/MP/SP sai, click attack/pot/refill sai.

## Root cause (audit)
- WGC capture `CreateForWindow(HWND)` → frame là window-relative, position-independent.
- Refill, SendInput đã dùng ClientToScreen runtime → position-independent.
- **Bar region coord (403, 656,...) trong [main.cpp:157](src/main.cpp#L157) hardcoded — nếu user đo từ Paint screenshot ở screen-space (không phải frame-space) → trùng hợp đúng khi window ở vị trí calib, sai khi move.** Đây là vector lỗi chính.
- Phụ: Windows snap edge có thể auto-resize → frame size đổi → bar coord lệch.

## Decision: A+B+C combined

### A — Live calibration UI (chính)
- Hiển thị WGC frame realtime trong UI (D3D texture → ImGui::Image).
- User click trên preview để set rect HP/MP/SP regions + inventory slots.
- Overlay rect lên preview để visual verify.
- Save coord vào AppConfig per profile.

### B — Pin window khi AUTO ON
- Profile lưu `anchorX, anchorY` (screen coord vị trí canonical).
- AUTO toggle ON → `SetWindowPos(hwnd, NULL, anchorX, anchorY, 0, 0, SWP_NOSIZE|SWP_NOZORDER)` đưa window về vị trí calib.
- Optional: "Capture current position as anchor" button trong UI.

### C — Audit log
- Log window rect mỗi 5s khi AUTO ON.
- Warn nếu rect thay đổi giữa các tick (move/resize bất ngờ).

## Architecture changes
- **AppConfig mới field:**
  - `BarConfig hpBar, mpBar, spBar;` (move từ hardcoded main.cpp).
  - `std::vector<Point> refillSlotsHp, refillSlotsSp, refillSlotsMp;` (move từ hardcoded pot-refill-scheduler.cpp).
  - `int anchorX = -1, anchorY = -1;` (-1 = chưa set, không pin).
- New: `src/ui/calibration-panel.{h,cpp}` (~250 LOC) — render preview + region picker.
- `main.cpp` đọc bar config từ `ctx->cfg` thay vì hardcoded.
- `pot-refill-scheduler.cpp` nhận slot coords từ cfg (truyền qua ctor hoặc updateConfig).
- New: `src/core/window-pin.{h,cpp}` (~40 LOC) — SetWindowPos + anchor capture util.

## Effort
+2d (A=1.5d, B=0.25d, C=0.25d).

## Migration
- Lần đầu chạy với plan mới → seed bar config + refill slot vào `profile-Default.json` từ hardcoded values hiện tại.
- User mở calibration UI để chỉnh nếu PT thay UI / patch.

## Risks
- ImGui::Image với D3D texture từ WGC frame → cần SRV shared giữa capture worker và UI thread. Đã có texture trong WgcCapture → chỉ cần thêm getter SRV.
- SetWindowPos có thể bị PT block / un-set ngay (DirectInput exclusive) → test thực tế; fallback: chỉ warn user.

## Unresolved
- Calibration multi-resolution: lưu profile riêng cho mỗi resolution PT (e.g., `Default-1024x768`, `Default-1280x1024`)? Tạm bỏ — user copy profile + recalibrate manual.
- N≥3 windows → tile auto: bỏ scope.
