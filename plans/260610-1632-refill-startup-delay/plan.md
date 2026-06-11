---
title: "Refill Startup Delay (Reuse intervalSec)"
description: "Sau khi bấm F8 ON, reset lastRefillAt timestamps về now() để lần refill đầu fire sau intervalSec (thay vì instant). Zero new field."
status: pending
priority: P2
created: 2026-06-10
updated: 2026-06-10
slug: refill-startup-delay
brainstorm: plans/reports/brainstorm-260610-1632-refill-startup-delay.md
blockedBy: []
blocks: []
---

# Refill Startup Delay

## Goal
F8 ON → refill KHÔNG fire ngay. Lần đầu fire sau `intervalSec` (per-slot, đã có sẵn). Reset timer mỗi lần combat OFF→ON.

## Root Cause
[pot-refill-scheduler.cpp:82](src/combat/pot-refill-scheduler.cpp#L82): `if (last == 0) return true;` — slot chưa refill lần nào ⇒ `isDue=true` ngay tick đầu sau F8.

## Approach (Final — KISS)
Khi `enable(true)` transition từ `false→true`: set `lastHpAt_ = lastSpAt_ = lastMpAt_ = now()`.
→ Logic `isDue` cũ tự nhiên gate đúng: lần refill đầu fire tại `F8_time + intervalSec`.

**Zero new config field, zero UI change, zero serialize change.**

## Phases
| # | Phase | Status |
|---|-------|--------|
| 1 | [Implementation](phase-01-implementation.md) | pending |

## Files Affected
- `src/combat/pot-refill-scheduler.h` — chuyển `enable()` inline → declaration
- `src/combat/pot-refill-scheduler.cpp` — implement `enable()` với transition logic

## Success Criteria
1. F8 ON với HP `intervalSec=60` → HP refill fire sau ~60s, KHÔNG fire ngay
2. F8 OFF rồi F8 ON lại giữa session → timer reset, đợi lại `intervalSec`
3. Hot-reload config khi combat đang ON → KHÔNG reset timer
4. Slot đã refill rồi mà F8 OFF→ON → reset (đây là intent)
5. Compile clean, no regression

## Notes
- Design cũ (thêm `startupDelaySec` field) bị reject vì violate YAGNI — `intervalSec` đã đủ làm setup delay.
