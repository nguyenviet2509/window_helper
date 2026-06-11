---
phase: 4
title: Grace Period + Periodic Verify + Revoke Handling
status: pending
priority: high
effort: 0.5d
depends_on: [phase-03]
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
- [ ] LicenseManager skeleton + Bootstrap branching
- [ ] Cache UpdateLastVerified method (atomic write rename)
- [ ] Periodic verify thread + stop mechanism
- [ ] OnLicenseLost callback + main.cpp toast handler
- [ ] Test: activate → restart → no dialog (cache hit)
- [ ] Test: simulate >48h cache age (manually edit) + offline → SHOW_DIALOG
- [ ] Test: revoke trong khi running → toast + shutdown
- [ ] Test: expire token → next start → SHOW_DIALOG with "expired" reason

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

## Next Steps
→ Phase 5 integrate vào main.cpp + polish
