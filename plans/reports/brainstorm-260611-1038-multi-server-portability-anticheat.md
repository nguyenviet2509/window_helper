---
type: brainstorm
date: 2026-06-11
slug: multi-server-portability-anticheat
status: agreed
---

# Brainstorm — Multi-server portability + anti-cheat layered defense

## Problem statement

Tool WindowHelper hiện chạy ổn trên 1 server PT cụ thể. Mục tiêu: dùng lại được trên **nhiều server PT private khác** (general portability), với giả định server đích **có thể có GameGuard mạnh**.

## Current state — gì dùng lại được, gì không

**Đã portable (chỉ cần config):**
- Window detect: fallback chứa "priston" — match được mọi PT private giữ titlebar gốc.
- Keybind, threshold HP/MP/SP, cooldown, radius, dwell — `config.json` + UI.
- Input backend: `SendInput` foreground (PostMessage bị PT reject — note ở [src/main.cpp:124](src/main.cpp#L124)).

**Hard-coded → blocker chính:**
- Vision ROI + hue ranges nhúng cứng trong [src/main.cpp:157-159](src/main.cpp#L157-L159):
  ```cpp
  auto hp = MakeBar(403, 656, 22, 121, { {0,10}, {170,180} });
  auto sp = MakeBar(383, 675, 11, 102, { {40, 80} });
  auto mp = MakeBar(586, 655, 21, 121, { {100,130} });
  ```
- Assume WGC frame 1010x789 → đổi resolution / UI skin → detector chết.

**Risk độc lập:**
- GameGuard có thể: hook `SendInput`, scan window class `WindowHelperUI`, scan exe `WindowHelper.exe`, hook DirectX present.
- Không thể fix một lần bằng software → cần layered defense.

## Evaluated approaches

### Vision portability
| Option | Pros | Cons |
|---|---|---|
| **A. Config preset + UI calibration** (✓ chọn) | Mỗi server 1 JSON; share dễ; YAGNI | Phải build calibration UI |
| B. Auto-calibrate khi startup | Zero-config UX | Phức tạp, false positive, vi phạm KISS |
| C. Hardcode multi-server compile flag | Zero overhead | Rebuild mỗi server, anti-pattern |

### Anti-cheat bypass
| Phase | Approach | Tin cậy | Cost |
|---|---|---|---|
| **2** | Stealth user-mode (rename class/exe, fallback PostMessage) | Thấp-trung (cat-and-mouse) | Thấp |
| **3** | Interception driver (signed kernel driver giả HID) | Trung-cao | Trung; user cài driver |
| **4** | Hardware HID (CH9329/Pico USB) | Rất cao | User mua hardware ~150-300k |

## Final solution — Layered phases

**Phase 1 — Vision portability + calibration UI** (core, independent)
- Move ROI/hue ra `config.json` → section `vision`.
- ImGui tab "Calibrate": user click-drag chọn ROI HP/MP/SP; tool tự sample hue trung bình + variance từ pixel trong ROI khi bar đầy.
- Preset switching: dropdown load `config-{server}.json`.
- Validation: hiển thị overlay debug bar detect realtime để user xác nhận.

**Phase 2 — Stealth user-mode**
- Window class name: random/configurable thay `WindowHelperUI`.
- Exe rename ở build script (`build.ps1` lấy custom name từ arg).
- Backend selector: thử PostMessage trước, fallback SendInput nếu PostMessage không nhận; cho user toggle qua config.
- Hide UI window khỏi Alt-Tab + taskbar khi chạy (tooltray-only mode).

**Phase 3 — Interception driver backend**
- Thêm `InterceptionBackend` implement `IInputBackend` (interface đã có ở [src/input/i-input-backend.h](src/input/i-input-backend.h)).
- Detect driver installed → enable backend; fallback SendInput nếu không có.
- UI: nút "Cài Interception driver" + check status.

**Phase 4 — Hardware HID backend (optional)**
- `SerialHidBackend`: gửi command qua COM port → MCU (CH9329 hoặc Pico W/CDC).
- Document firmware ref + wiring guide trong `docs/hardware-hid-setup.md`.
- UI: chọn COM port, test button.

**Capture**: giữ `WgcCapture`, KHÔNG đụng. Chỉ fix khi gặp vấn đề thực tế.

## Implementation considerations

- **File ownership**: `i-input-backend.h` đã abstract → Phase 3-4 chỉ thêm backend mới, không phá vỡ.
- **Config schema migration**: Phase 1 đổi schema `config.json` → thêm version field, auto-migrate old config.
- **Calibration UX**: phải intuitive — overlay highlight ROI trong khi drag; sample hue range bằng `min-max ± padding`.
- **PostMessage caveat**: bạn đã ghi PT từ chối PostMessage. Trên server khác có thể nhận — cần test, không assume.
- **Anti-cheat testing**: chỉ test trên server có cho phép, hoặc test acc throwaway. **Risk ban acc** — phải warning user rõ.

## Risks

| Risk | Severity | Mitigation |
|---|---|---|
| GG update detect Interception | Cao | Có Phase 4 HW HID làm fallback |
| Calibration UI buggy → user calibrate sai | Trung | Debug overlay realtime + preset share community |
| Driver install fail (user UAC từ chối) | Trung | Fallback SendInput, clear error message |
| User HW không có | Thấp | Phase 4 optional, không block Phase 1-3 |
| Ban acc khi test | Cao | Document throwaway acc, warning UI |

## Success metrics

- Phase 1: calibrate 1 server mới trong < 5 phút, không sửa code.
- Phase 2: tool sống ≥ 1h trên server có anti-cheat nhẹ.
- Phase 3: tool sống ≥ 1h trên server GameGuard nhẹ-trung.
- Phase 4: tool sống ổn định trên server GameGuard mạnh.

## Next steps

1. Tạo plan chi tiết qua `/ck:plan` với 4 phase trên.
2. Phase 1 ưu tiên cao nhất — làm trước cho server hiện tại đã có lợi.
3. Test thực tế trên server đích trước khi build Phase 3/4 (YAGNI check).

## Unresolved questions

- Server đích cụ thể là gì? Cần biết để test Phase 2 và quyết Phase 3/4.
- Có sẵn 1 acc throwaway để test anti-cheat không?
- Phase 4 (HW HID): user có sẵn sàng mua phần cứng không, hay chỉ document để dành?
