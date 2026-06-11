# Phase 4+5 Implementation Report

## Phase 4 — Files Created

| File | LOC | Notes |
|---|---|---|
| `src/license/license-manager.h` | 64 | API: BootstrapResult, LicenseManager class |
| `src/license/license-manager.cpp` | 211 | Bootstrap, verifyLoop, runOneVerify |

## Phase 5 — Files Created / Modified

| File | LOC | Change |
|---|---|---|
| `src/ui/license-info-dialog.h` | 33 | NEW — read-only popup |
| `src/ui/license-info-dialog.cpp` | 182 | NEW — token mask, vi-VN date, grace timer, Copy button |
| `src/main.cpp` | 361 | MODIFIED — license gate + overlay hook |
| `src/ui/main-window.h` | 100 | MODIFIED — +renderActivationFrame, +setOnFrameOverlay |
| `src/ui/main-window.cpp` | 599 | MODIFIED — renderActivationFrame impl, overlay call in renderFrame |
| `src/ui/tray-icon.h` | 27 | MODIFIED — +setOnLicenseInfo callback |
| `src/ui/tray-icon.cpp` | 50 | MODIFIED — +kMenuLicenseInfo item, handler |
| `src/CMakeLists.txt` | 68 | MODIFIED — added license-manager.cpp, license-info-dialog.cpp |

## Build Status

PASS — `build.ps1` clean. Only pre-existing C++20 `atomic_store/load` deprecation warnings in `config-bus.h` (not our code). No errors.

Output: `build/bin/Release/svc_dkpa7.exe` (0.88 MB)

## main.cpp Insertion Point

License gate inserted at **lines 114–155** (after `win.init()`, before input/combat/capture init). Frame overlay hook wired at lines 278–316. Old code: UI constructed last; new code: UI constructed first to enable dialog rendering.

**Structural change from original:** `MainWindow win` moved before capture/input init. Order is now:
1. Logger + config + FindTarget
2. `win.init()` (ImGui+DX11 up)
3. LICENSE GATE (bootstrap → dialog loop if needed)
4. Input/combat/capture/vision init (as before)

This is the correct order per Phase 5 spec.

## Deviations from Plan

| Plan spec | Actual | Reason |
|---|---|---|
| `LicenseManager(LicenseClient&)` constructor | Default ctor — calls static methods directly | LicenseClient uses static methods only; no instance needed |
| `SetOnLicenseLost` callback on manager | Polling `LicenseLost()` atomic per frame | KISS — avoids thread-safety issues with callback firing from worker; poll is free |
| `revoked` field on `CachedLicense` | Not present in existing `CachedLicense` struct (Phase 1) | Skipped — server signals revoke via Verify() returning Revoked error; no local flag needed |
| Bootstrap calls `StartPeriodicVerify()` internally (phase 4 spec) | Caller (main.cpp) calls it after dialog | Cleaner separation; matches Phase 5 wiring spec |

## Tray Menu Integration

Used `kMenuLicenseInfo = 1` (renumbered existing items up). Menu item "License Info..." inserted before separator at top of context menu. Callback: `tray.setOnLicenseInfo([&]{ licInfoDlg.Open(); })`.

`LicenseInfoDialog` is stack-allocated in main.cpp, holds `const CachedLicense&` reference to `licenseManager.Current()` — live data.

## LicenseInfoDialog

Included and complete. Fields shown: masked token (first4+****+last4), machine_id short (8 hex), expiry (DD/MM/YYYY HH:MM or "Vĩnh viễn"), last verified datetime, grace remaining (Xh Ym or "Hết grace — cần online"). Copy button for machine_id with 2s "Da copy!" flash.

## Status Indicator

Not implemented as separate badge — license-lost state is shown as a prominent red overlay toast instead, which is more actionable. Per plan: "optional, time-permitting".

## Manual Test Plan

- **No cache → dialog** — Bootstrap returns SHOW_DIALOG; render loop shows ActivationDialog only
- **Invalid token** — dialog stays open with error from existing ActivationDialog error handling
- **Valid token** — OnActivated fires → AdoptFromDialog → StartPeriodicVerify → main pipeline starts
- **Restart with valid cache within 48h** — Bootstrap loads cache, grace not expired → async verify spawned → ENTER_MAIN, no dialog
- **Restart with cache >48h old (offline)** — grace expired → sync Verify → NetworkError → SHOW_DIALOG with "Không kết nối được server"
- **Tray "License Info..."** — opens modal with token/machine/expiry/grace data
- **Periodic verify: server returns Revoked** — `license_lost_` set → next frame overlay shows red toast "Mã đã bị thu hồi" + 30s countdown → PostQuitMessage

## Open Questions

None — all spec items resolved. `CachedLicense.revoked` field absent from Phase 1 struct; handled by treating Revoked error from Verify() as the signal (no local flag needed, server is authoritative).

---
**Status:** DONE
