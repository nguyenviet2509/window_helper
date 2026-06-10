---
phase: 7
title: UI reuse from coexist plan (strip pause/cursor controls)
status: pending
priority: P2
effort: 1d
---

# Phase 7 — UI

## Context
Reuse `main-window-coexist.cpp` từ plan 0928 (dynamic tabs + per-profile config). Strip controls không cần:
- Pause threshold sliders (3s/5s/30s) → bỏ
- Cursor park x/y → bỏ
- Manual pause F9 → giữ (vẫn dùng cho convenience)
- User activity indicator → bỏ

Add:
- Injection status per tab: "INJECTED" / "FAILED" / "PENDING"
- "Re-inject" button per tab (manual retry)

## Files
- `src/ui/main-window-injection.cpp` (~400 LOC, derived from coexist version)
- Reuse: profile-manager-ui, calibration-panel, audit-log

## Layout

```
┌─ Header ─────────────────────────────────────────┐
│ Processes detected: 2/3   Injected: 2/2          │
├─ Tabs ───────────────────────────────────────────┤
│ [ W0: PT (PID 1234) ✓ ] [ W1: PT (PID 5678) ✓ ]  │
├─ Tab content ────────────────────────────────────┤
│ Status: AUTO ON   Inject: ✓ INJECTED             │
│ Profile: [MainChar ▼] [...] [Re-inject]          │
│                                                  │
│ ┌─ Combat / Refill / Buffs (per-profile cfg) ──┐ │
│ │ ...                                          │ │
│ └──────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────┘
```

## Empty state
"Waiting for PristonTale.exe process..." + Rescan button.

## Inject status indicator
- ✓ INJECTED (green)
- ⌛ INJECTING (yellow, transient)
- ✗ FAILED (red, with retry button)
- ⚠ XC3 DETECT (red flashing, DLL self-unloaded)

## Todo
- [ ] Copy main-window-coexist → main-window-injection.
- [ ] Strip pause/cursor controls.
- [ ] Add inject status indicator.
- [ ] Re-inject button → calls main-side injector.reInject(pid).

## Success criteria
- 3 PT mở → 3 tab xuất hiện với inject status đúng.
- Re-inject button work sau khi DLL self-unload.

## Risks
- Tối thiểu — copy paste + adjust.
