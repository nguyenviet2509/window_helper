# Code Review — Phase 4-5 (License Manager + Integration)

**Verdict:** APPROVE_WITH_CHANGES
**Score:** 7/10

Solid structure, clean separation, good Vietnamese UX. Several real thread-safety bugs in `LicenseInfoDialog` reads of `current_` and one logic bug in destructor ordering. Most are small, well-scoped fixes.

---

## CRITICAL (must-fix)

### C1 — Data race: LicenseInfoDialog reads `current_` without lock
**File:** `src/license/license-manager.h:46`, `src/ui/license-info-dialog.cpp:114-150`
`LicenseManager::Current()` returns a `const CachedLicense&` to `current_`. The verify worker thread mutates `current_.last_verified` / `expires_at` under `mtx_` (`license-manager.cpp:184-186`). The UI thread (`LicenseInfoDialog::Render`) reads `lic_.token`, `lic_.machine_id`, `lic_.last_verified`, `lic_.expires_at` with no synchronization. `std::string` reads concurrent with writes are UB; even POD ints have torn-read risk on the int64 fields.

**Fix:** Add a thread-safe snapshot accessor:
```cpp
CachedLicense LicenseManager::Snapshot() const {
    std::lock_guard<std::mutex> lock(mtx_);   // mutable mutex
    return current_;
}
```
Change `LicenseInfoDialog` to hold a `CachedLicense` by value plus a `std::function<CachedLicense()>` provider, and refresh once per `Open()` (or each frame while open).

### C2 — `runOneVerify` calls `LicenseCache::Save(current_, hwid)` while holding `mtx_`
**File:** `src/license/license-manager.cpp:183-186`
Holding the mutex across a disk encrypt + write blocks `AdoptFromDialog` and any future `Snapshot()` for the duration of file I/O (tens of ms). Lower-impact than C1 but worth fixing alongside.

**Fix:** Copy under lock, save outside lock.
```cpp
CachedLicense snap;
{ std::lock_guard<std::mutex> g(mtx_); current_.last_verified = now;
  if (result.expires_at) current_.expires_at = result.expires_at;
  snap = current_; }
LicenseCache::Save(snap, hwid);
```

### C3 — Bootstrap detached thread races cache file with first periodic verify
**File:** `src/license/license-manager.cpp:92-102`
The detached "background verify" started in Bootstrap (within grace) writes to `current_` indirectly only via `LicenseCache::Save` against a freshly-loaded copy `c` — but it does NOT update the manager's in-memory `current_`. So `current_.last_verified` stays stale until the first 6h periodic tick, and `LicenseInfoDialog` will show wrong "last verified" time even after a successful refresh. Additionally if Bootstrap's detached thread is still running when `~LicenseManager` runs, it touches the disk after `LicenseCache::Clear()` could be called from `runOneVerify`.

**Fix:** Either (a) drop the detached refresh (cache will refresh on first periodic tick — KISS), or (b) make it a tracked member thread that calls back into the manager under lock. Recommend (a) — simpler, removes the race entirely. Grace exists precisely to defer this.

---

## HIGH (should-fix)

### H1 — `LicenseLostReason()` data race
**File:** `src/license/license-manager.cpp:141-145`
Comment claims `lost_reason_` is only written before `license_lost_` is set, but `AdoptFromDialog` clears it under `mtx_` (line 116) without setting the atomic. A concurrent read from main-thread `LicenseLost()/LicenseLostReason()` while AdoptFromDialog runs is racy. Also written from `runOneVerify` under lock, read here without lock.

**Fix:** Take `mtx_` for the read, or store reason in an `std::atomic<std::shared_ptr<std::string>>`. Lock is simpler.

### H2 — `mtx_` is not `mutable`, but `Current()`/`LicenseLostReason()` are `const`
**File:** `src/license/license-manager.h:61`
Adding a lock to either method (required for C1/H1) won't compile unless `mtx_` is `mutable`.

**Fix:** `mutable std::mutex mtx_;`

### H3 — `Stop()` deadlock risk if called from inside the verify thread path
Not currently called from worker, but `Stop()` does `cv_.notify_all()` then `join()`. The cv predicate uses `stop_flag_.load()` and the wait holds `mtx_`. Worker also takes `mtx_` inside `runOneVerify`. If `Stop()` is called while `runOneVerify` holds `mtx_` doing a network call, join waits — correct, but the comment in `verifyLoop` claims "Release lock before network call" — verify the unlock actually happens before `LicenseClient::Verify`. Line 158 confirms — OK. Keep as-is, but add a note: never call `Stop()` from inside any callback fired by the verify thread.

### H4 — Bootstrap synchronous Verify with no timeout sentinel
**File:** `src/license/license-manager.cpp:60`
Grace-expired branch calls `LicenseClient::Verify` synchronously before window is shown. If WinHTTP timeout is the library default (≈60s), user sees a frozen black window for up to a minute after splash. Verify that `LicenseClient` uses an aggressive timeout (≤5s) for the bootstrap path, otherwise gate it behind a "Verifying license…" overlay.

**Fix:** Either (a) render activation-frame with a "Đang xác thực…" overlay before the sync call, or (b) confirm `LicenseClient::Verify` enforces ≤5s timeout. Document the timeout in `license-client.h`.

### H5 — Token logged? Verify `LicenseClient::Verify` does not log full token on error
Not visible in this diff. Confirm in `license-client.cpp` that network/parse error paths never `LOG_*` the raw token or HWID. The diff itself is clean — no token printed in `license-manager.cpp` or `main.cpp`.

---

## MEDIUM

### M1 — `LicenseInfoDialog` constructor binds `const&` to manager-owned object
**File:** `src/ui/license-info-dialog.h:12`, `src/main.cpp:286`
Even after C1 is fixed via Snapshot, the reference-binding API is misleading — the dialog appears to be a "live view" but mutating fields are racy. Replace with by-value + refresh-on-Open pattern (see C1 fix).

### M2 — License-lost toast: `licLostCountdown` decrements by `io.DeltaTime` but `PostMessageW(WM_DESTROY)` is fired every frame after 0
**File:** `src/main.cpp:337-340`
Once `licLostCountdown <= 0`, the overlay keeps running and posts `WM_DESTROY` every frame until the loop exits. Cosmetic (DestroyWindow is idempotent-ish) but noisy.

**Fix:** Add a `bool licLostShutdownPosted = false;` guard.

### M3 — `setOnExit` callback registered on tray but old `setOnExit` callback on `MainWindow` is unused
**File:** `src/main.cpp:283`
`tray.setOnExit` posts `WM_DESTROY` directly to `win.hwnd()`. Confirm this bypasses any save-on-exit logic in `MainWindow`. If config has unsaved debounced changes, they'll be lost. Minor — debounce is 500ms — but worth verifying `MainWindow::runLoop` flushes on WM_DESTROY.

### M4 — `current_.token` empty short-circuit in `runOneVerify` silently no-ops
**File:** `src/license/license-manager.cpp:176`
If token is empty (e.g. `AdoptFromDialog` somehow called with empty fresh) the worker spins every 6h doing nothing instead of signaling lost. Acceptable defensively but log a warning the first time.

### M5 — `LicenseInfoDialog::maskToken` length check `< 8` shows `****`
**File:** `src/ui/license-info-dialog.cpp:19-22`
Real tokens are 32+ chars so this branch is unreachable in practice. KISS — fine as-is, but worth a comment that empty/short token means invalid state, not just "redacted."

---

## LOW

- `license-manager.cpp` is 211 LOC — 5% over the 200-LOC guideline. Acceptable; splitting would harm cohesion (YAGNI).
- `LicenseInfoDialog::Render` uses `"Da copy!"` (ASCII) while every other label is properly UTF-8 Vietnamese (`Đã hết hạn`, etc.). Likely intentional for ImGui default font glyph coverage; if so, add a one-line comment. Otherwise normalize to `"Đã copy!"`.
- `main.cpp` license-lost toast strings `"Dong app sau %.0f giay..."` likewise stripped of diacritics — same comment.
- `kVerifyInterval = 6h` is hard-coded; consider sourcing from `cache.grace_hours / 8` or config — defer (YAGNI for now).
- `LicenseManager::Stop()` after `WGC capture failed` early-exit (main.cpp:215): good — explicitly stops before scheduler shutdown.

---

## Praise

- Clean two-phase init: UI before license gate so the dialog has a render target; heavy capture/vision only after activation. Saves resources on the EXIT path.
- Detached worker thread + atomic-flag polling from main loop is the right pattern; zero per-frame cost when license valid.
- Bootstrap error→Vietnamese mapping is exhaustive and matches `ErrorCode` enum 1:1.
- `renderActivationFrame` cleanly reuses DX11 swap chain — no duplicate device code.
- Destructor calls `Stop()` — exception-safe RAII.
- Single-instance mutex check unaffected; cleanup ordering in main.cpp is correct (reverse init order).
- `WM_QUIT` handled inside the activation render loop (main.cpp:170) — graceful early-exit before heavy init.

---

## Open Questions

1. Does `LicenseClient::Verify` enforce a bootstrap-suitable timeout (≤5s)? (H4)
2. Does `MainWindow::runLoop` flush debounced config save on `WM_DESTROY`? (M3)
3. Are the ASCII-only license-lost / "Da copy" strings intentional (font glyph coverage) or oversight? (LOW)
4. Should `kVerifyInterval` be derived from `grace_hours` rather than hard-coded 6h?
