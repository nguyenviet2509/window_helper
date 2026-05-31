---
title: "Anti-detect Tier 1 (Xingcode3 + GM)"
description: "Mouse Bezier path, Gaussian click distribution, anti static-scan, PostMessage-only feasibility."
status: pending
priority: P1
effort: 10h
branch: master
tags: [anti-detect, input, combat, build]
created: 2026-05-30
---

# Plan — Anti-detect Tier 1

## Context
- Brainstorm: `plans/reports/brainstorm-260530-2118-anti-detect-tier1.md`
- Mục tiêu: chống Xingcode3 static scan + GM behavioral monitoring.
- Nguyên tắc: KISS/YAGNI/DRY, file <200 LOC, không mock.

## Phases

| # | Phase | File | Status | Effort | Blockers |
|---|---|---|---|---|---|
| 01 | Mouse path Bezier | `phase-01-mouse-path.md` | done (code) | 3h | — |
| 02 | Click distribution Gaussian | `phase-02-click-distribution.md` | done (code) | 1h | — |
| 03 | Anti static-scan (title+CMake) | `phase-03-anti-static-scan.md` | done (code) | 1.5h | — |
| 04 | PostMessage-only feasibility | `phase-04-postmessage-only.md` | partial — probe expanded, config added; flip default deferred (PT từ chối PostMessage) | 2.5h | 01 |
| 05 | Integration test E2E | `phase-05-integration-test.md` | pending — needs user gameplay | 2h | 01,02,03,04 |

## Dependencies
- 02, 03 độc lập → có thể parallel với 01.
- 04 cần `sendMouseMove` từ 01 (PostMessage path).
- 05 chạy cuối, verify success criteria từng phase.

## File Ownership (no overlap)
- Phase 01: `src/input/mouse-path.{h,cpp}` (new), `src/input/i-input-backend.h`, `src/input/send-input-backend.{h,cpp}`, `src/input/postmessage-backend.{h,cpp}`, `CMakeLists.txt` (add sources)
- Phase 02: `src/combat/attack-sweep.h`
- Phase 03: `src/main.cpp` (title), `CMakeLists.txt` (OUTPUT_NAME)
- Phase 04: `src/config/config-loader.{h,cpp}`, `src/ui/main-window.cpp` (backend selector), `tools/postmessage-probe/probe.cpp`
- Phase 05: không edit code, chỉ run + report.

> CMake bị 2 phase đụng (01 add sources, 03 OUTPUT_NAME) → 03 chạy SAU 01 hoặc gộp 1 commit.

## Backwards compat
- Mouse path bật mặc định, có flag `enableMousePath` (default true) trong config để rollback.
- `defaultBackend` config mặc định "SendInput" cho đến khi phase 04 xác nhận PostMessage OK.

## Rollback strategy
- Per-phase: mỗi phase 1 commit riêng → `git revert <sha>` tách bạch.
- Mouse path: tắt qua config thay vì revert code.

## Success (toàn cục)
- Cursor di chuyển mượt khi đánh quái (không teleport).
- Histogram click có cluster (không uniform).
- Window title không chứa "WindowHelper"; build sinh exe khác tên.
- PostMessage backend test verdict ghi rõ trong report phase 04.
- All compile OK (MSVC), no warnings new.
