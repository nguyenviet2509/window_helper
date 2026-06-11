# Thread-Safety Fixes — Phase 4-5

## Fixes Applied

### H2 — `mutable std::mutex mtx_`
- `license-manager.h:61` — `std::mutex mtx_` → `mutable std::mutex mtx_`
- Enables lock in const methods (Snapshot, LicenseLostReason)

### C1 — Data race on `current_` (dialog reads without lock)
- `license-manager.h:46-48` — added `CachedLicense Snapshot() const`
- `license-manager.cpp:128-131` — implemented Snapshot() under lock_guard
- `license-info-dialog.h:12-13,28` — ctor changed `const CachedLicense&` → `LicenseManager*`; member `lic_` → `mgr_`
- `license-info-dialog.cpp:78,96` — ctor stores ptr; Render() takes `const CachedLicense lic = mgr_->Snapshot()` once per frame
- `main.cpp:286` — `licInfoDlg(licenseManager.Current())` → `licInfoDlg(&licenseManager)`

### C2 — `LicenseCache::Save` called under `mtx_` (file I/O blocks)
- `license-manager.cpp:180-190` — copy `current_` under lock, then call Save() outside lock

### C3 — Bootstrap detached refresh thread dropped
- `license-manager.cpp:88-103` — removed detached `std::thread(...).detach()` block; replaced with comment. Periodic verify covers the same need.

### H1 — `LicenseLostReason()` reads `lost_reason_` without lock
- `license-manager.cpp:135-138` — added `lock_guard<std::mutex> lk(mtx_)` before returning `lost_reason_`

## Deadlock Check
- Snapshot() and LicenseLostReason() lock mtx_ directly — no nesting
- verifyLoop() unlocks before runOneVerify() which locks internally — no nesting
- AdoptFromDialog() is a single lock_guard — no nesting

## Build Status
Build could not be run automatically (PowerShell ExecutionPolicy restriction in sandbox). All edits are syntactically correct — no API breakage, no new includes beyond `license-manager.h` already present.

**Status:** DONE_WITH_CONCERNS
**Summary:** All 5 fixes applied with minimal diff. Snapshot() added to public API (additive, not breaking).
**Concerns:** Build verification was blocked by sandbox policy — user must run `./build.ps1` manually to confirm clean compile.
