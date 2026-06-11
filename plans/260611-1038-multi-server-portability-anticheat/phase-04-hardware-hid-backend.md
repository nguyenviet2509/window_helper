# Phase 4 — Hardware HID backend

## Context
- Plan: [plan.md](plan.md)
- Reference HW: CH9329 USB-to-HID module, hoặc Raspberry Pi Pico (RP2040) với TinyUSB CDC+HID composite.

## Overview
- **Priority**: P3 (optional — only if Phase 3 die trên server AC mạnh)
- **Status**: DEFERRED — spec giữ làm reference, KHÔNG implement cho tới khi Phase 3 ship + vẫn bị detect. YAGNI.
- Tool gửi command qua COM/Serial → microcontroller giả USB HID thật. OS nhận input như chuột/bàn phím vật lý → GG **không thể** distinguish.

## Key insights
- Đây là cách bypass anti-cheat tin cậy nhất software-only không làm được.
- Cost: user phải mua + flash MCU. ~150k VND (CH9329) hoặc ~120k VND (Pico).
- Phải có write-up firmware để user tự setup hoặc bán pre-flashed.
- Khó debug — nếu input không tới, phải kiểm tra cả COM, firmware, driver, USB enumeration.

## Requirements

**Functional:**
- `SerialHidBackend` implement `IInputBackend`:
  - Open COM port (configurable), 115200 8N1 default.
  - Wire protocol simple binary: `[CMD][LEN][PAYLOAD]` (key down/up, mouse move abs, mouse click).
  - Sync: read ACK byte sau mỗi command (timeout 100ms).
- UI: dropdown COM port (enum via `SetupDiGetClassDevs`), baud rate, "Test" button (gửi NumLock toggle).
- Mouse abs coord: client → screen → normalized [0, 32767] (HID absolute).
- Fallback nếu COM port disconnect → notify user, switch SendInput emergency.

**Non-functional:**
- Firmware reference (CH9329 + Pico) trong `docs/`, không build firmware trong repo này.
- Protocol document để 3rd-party tự implement firmware.

## Architecture
```
PC (WindowHelper)              MCU (CH9329 or Pico)
┌──────────────────┐  USB-CDC ┌────────────────┐  USB-HID  ┌──────────┐
│ SerialHidBackend │ ──────► │ Firmware proxy │ ────────► │ OS sees  │
│ COM3, 115200     │         │ parse [CMD]    │           │ real HID │
└──────────────────┘         └────────────────┘           └──────────┘
```

Wire protocol (concise):
```
0x01 KEY_TAP    [vk_low][vk_high][hold_ms]
0x02 KEY_DOWN   [vk_low][vk_high]
0x03 KEY_UP     [vk_low][vk_high]
0x04 MOUSE_MOVE [x_lo][x_hi][y_lo][y_hi]    // absolute, 0..32767
0x05 MOUSE_DOWN [button]                     // 1=L 2=R 4=M
0x06 MOUSE_UP   [button]
0x07 PING       []                          // health check, ACK = 0xAA
```

ACK byte: `0xAA` = OK, `0xEE` = error.

## Related files

**Create:**
- `src/input/serial-hid-backend.h` / `.cpp` (~250 LOC) — COM IO + protocol encode.
- `src/input/com-port-enumerator.h` / `.cpp` (~60 LOC) — Win32 enumerate available COM ports.
- `docs/hardware-hid-setup.md` — wiring guide, firmware flash steps, troubleshooting.
- `firmware/pico-hid-proxy/README.md` — link reference firmware (separate repo hoặc embed sketch).
- `firmware/ch9329-config.md` — CH9329 config (baud, descriptor) hướng dẫn.

**Modify:**
- [src/state/game-state.h](../../src/state/game-state.h) — backend enum thêm `SerialHid`; config field `serial.comPort`, `serial.baud`.
- [src/input/backend-selector.cpp](../../src/input/backend-selector.cpp) — Serial highest priority nếu configured.
- [src/ui/main-window.cpp](../../src/ui/main-window.cpp) — COM port UI + test button.
- [src/config/config-loader.cpp](../../src/config/config-loader.cpp) — parse `serial` section.

## Implementation steps

1. **Wire protocol spec** — finalize byte format trên (document `docs/hardware-hid-setup.md`).
2. **COM enumerator** — `SetupDiGetClassDevs(GUID_DEVCLASS_PORTS)` → list `COMx` + friendly name.
3. **`SerialHidBackend` IO layer** — `CreateFileW("\\\\.\\COMx")`, set DCB (115200/8N1), `WriteFile` + `ReadFile` with timeout.
4. **Protocol encoder** — convert `IInputBackend` calls → byte sequence + send + wait ACK.
5. **Coord normalization** — client (x,y) → screen → normalized abs (0..32767 per HID spec).
6. **Health monitoring** — periodic PING (every 5s); on timeout/NACK → mark backend dead, log + UI alert.
7. **UI panel** — Settings:
   - COM dropdown (enumerate on open).
   - Baud rate input (default 115200).
   - "Test" button: send NumLock toggle, observe NumLock LED + win32 `GetKeyState(VK_NUMLOCK)`.
   - Status indicator (green/red).
8. **Firmware reference** —
   - **CH9329**: config baud + descriptor qua AT command. Document mode + DIP switches.
   - **Pico**: TinyUSB composite (CDC + HID); flash UF2 build từ separate `pico-hid-proxy` repo. Provide source skeleton (~150 LOC C++).
9. **Field test** — full pipeline trên server GG mạnh, đo TTL + reliability.

## Todo
- [ ] Finalize wire protocol doc
- [ ] COM port enumerator
- [ ] `SerialHidBackend` IO + encoder
- [ ] HID coord normalization
- [ ] Backend health monitor (PING)
- [ ] UI COM picker + test button
- [ ] CH9329 setup doc
- [ ] Pico firmware reference (skeleton sketch + UF2 instruction)
- [ ] Backend-selector Serial priority
- [ ] Field test GG mạnh

## Success criteria
- Cắm MCU vào USB → tool enumerate được COM, test button gửi NumLock thành công.
- Tool gửi attack sequence → mob bị đánh đúng như input vật lý.
- Server GG mạnh không detect input bất thường ≥ 2h chạy liên tục.
- Disconnect MCU → tool detect trong < 5s, fallback hoặc cảnh báo.

## Risks
| Risk | Mitigation |
|---|---|
| User không có/không mua MCU | Phase 4 optional; Phase 3 đã cover phần lớn case |
| Firmware buggy → input lag/lost | PING timeout detect; document known-good firmware version |
| COM port permission (admin yêu cầu trên 1 số setup) | Document; tool detect ACCESS_DENIED → error rõ |
| Mouse abs coord trên multi-monitor scale sai | Test; document single-monitor recommended |
| MCU rate limit (USB HID polling 1ms = 1000Hz max) | Throttle send rate; document max attack rate |
| GG phát hiện qua timing pattern (input quá đều) | Reuse `Humanizer` jitter đã có ở pipeline |

## Security
- HW device → user phải tin firmware. Document checksums.
- Wire protocol unauthenticated — bất kỳ app nào trên máy ghi COM cũng inject input được. Acceptable trade-off (local-only attack surface).

## Open questions
- CH9329 vs Pico: chọn primary support nào? CH9329 plug-and-play hơn, Pico flexible/rẻ hơn.
- Có ship firmware pre-built không, hay user tự flash?
- Có cần encryption/auth layer cho wire protocol không? (Mostly không cần — local-only).

## Next steps
→ Phase này là phase cuối. Sau đó: stabilize, document multi-server preset library, journal kết quả test thực tế.
