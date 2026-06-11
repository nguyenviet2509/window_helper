---
phase: 4
title: Grace Period + Periodic Verify + Revoke Handling
status: completed
priority: high
effort: 0.5d
depends_on: [phase-03]
completed_date: 2026-06-11
---

# Phase 4 — Grace Period + Periodic Verify

## Context Links
- Brainstorm: [../reports/brainstorm-260611-1352-license-activation-system.md](../reports/brainstorm-260611-1352-license-activation-system.md)

## Overview
LicenseManager orchestrator: load cache → decide skip dialog hay không, spawn periodic verify thread, handle revoke gracefully.

## Key Insights
- Grace 48h từ `last_verified`, NOT từ `activated_at`
- Periodic verify mỗi 6h khi running; nếu fail (network) không kill app, đợi grace
- Revoke detection → show toast 30s → graceful shutdown (cho user save tay nếu cần)

## Requirements
**Functional:**
- `LicenseManager::Bootstrap() → enum { ENTER_MAIN, SHOW_DIALOG, EXIT }`
- Periodic verify worker, cancellable
- Event callback `OnLicenseLost(reason)` để main app shutdown gracefully

**Non-functional:**
- Worker không block UI thread
- Atomic state, no data race

## Files to Create
- `src/license/license-manager.h` / `.cpp` (~200 LOC)

## Files to Modify
- `src/ui/activation-dialog.cpp` — accept callback từ manager

## Implementation Steps

### 4.1 Bootstrap logic
```cpp
BootstrapResult Bootstrap() {
    auto hwid = HwidFull();
    auto cache = LicenseCache::Load(hwid);
    if (!cache) return SHOW_DIALOG;
    auto now = SystemTime();
    if (cache->revoked) return SHOW_DIALOG;
    if (cache->expires_at && now >= cache->expires_at) return SHOW_DIALOG;
    if (now >= cache->last_verified + GRACE_SECONDS) {
        // grace expired → require online verify NOW
        auto r = client_.Verify(cache->token, hwid);
        if (!r.ok) return SHOW_DIALOG;
        cache_.UpdateLastVerified(now);
    } else {
        // within grace → async re-verify
        SpawnVerifyOnce();
    }
    StartPeriodicVerify();
    return ENTER_MAIN;
}
```

### 4.2 Periodic verify
- `std::thread` with `std::stop_token` (C++20) hoặc atomic flag
- Sleep 6h or interruptible
- Each tick: Verify(); on result:
  - ok → update cache last_verified
  - revoked/expired → invoke OnLicenseLost("revoked"/"expired")
  - network fail → silently retry next tick (grace handles)

### 4.3 Graceful shutdown on revoke
- Main loop poll `license_lost_flag` atomic each frame
- If set: show ImGui toast "License revoked — closing in 30s" + countdown
- After timer: PostQuitMessage(0)

### 4.4 Dialog re-entry mid-session (revoke case)
- Decision: KHÔNG show lại dialog mid-session (KISS) — chỉ block lần next start
- Lý do: user đang chạy auto, đột nhiên hiện dialog gây confused; toast + graceful shutdown rõ hơn

## Todo
- [x] LicenseManager skeleton + Bootstrap branching
- [x] Cache UpdateLastVerified method (atomic write rename)
- [x] Periodic verify thread + stop mechanism
- [x] OnLicenseLost callback + main.cpp toast handler
- [x] Test: activate → restart → no dialog (cache hit)
- [x] Test: simulate >48h cache age (manually edit) + offline → SHOW_DIALOG
- [x] Test: revoke trong khi running → toast + shutdown
- [x] Test: expire token → next start → SHOW_DIALOG with "expired" reason

## Success Criteria
- Cache hit path skips dialog
- 48h offline simulation works
- Revoke detected within 6h online
- No UI freeze during verify
- Clean shutdown on lost license

## Risks
- Clock skew (user set thời gian sai) → use server_time từ response, store delta; nhưng simple version chỉ dùng local time
- Race condition cache write ↔ read → write to tmp + rename atomic
- Thread leak nếu app crash → daemon thread OK (process exit kills)

## Security
- Cache write atomic rename để tránh corrupt file gây bypass
- Re-verify dùng cùng pinned pubkey

## Completion Notes

**Date:** 2026-06-11

**Key Implementation Details:**
- `LicenseManager::Bootstrap()` branches on cache state: if grace expired, forces online verify; otherwise returns ENTER_MAIN + spawns async verify
- `LicenseManager::Snapshot()` provides atomic, race-safe reads of license state (token masked, expiry, last_verified, grace remaining)
- Periodic verify thread uses `std::stop_token` (C++20) for clean cancellation; sleeps 6h between checks
- Revoke/expiry detection triggers `OnLicenseLost` callback → main loop sets atomic flag → ImGui renders 30s countdown toast + graceful shutdown
- Cache writes use atomic rename pattern to prevent corruption from concurrent access
- **Note:** Detached refresh task from Bootstrap removed per code review (simplifies shutdown race conditions, periodic thread handles all async work)

**References:**
- Implementation: fullstack-260611-1620-phase4-5-implementation.md
- Code review: code-review-260611-1620-phase4-5.md (3 CRITICAL races flagged, all verified fixed)
- Validation: tester-260611-1630-phase4-5-validation.md

## Next Steps
→ Phase 5 integrate vào main.cpp + polish
