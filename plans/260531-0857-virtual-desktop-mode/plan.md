---
title: "Virtual Desktop Mode — Auto không chiếm chuột"
description: "Chạy PT trên separate Windows Desktop object để cursor #2 hoàn toàn cô lập, user vẫn dùng cursor #1 trên Default desktop."
status: pending
priority: P1
effort: 1-2w + 1w soak
branch: master
tags: [architecture, input, vision, desktop-isolation, anti-hijack]
created: 2026-05-31
brainstorm: ../reports/brainstorm-260531-0846-virtual-desktop-anti-hijack.md
blockedBy: []
blocks: []
---

# Plan — Virtual Desktop Mode

## Context
- Brainstorm: [brainstorm-260531-0846-virtual-desktop-anti-hijack.md](../reports/brainstorm-260531-0846-virtual-desktop-anti-hijack.md)
- Vấn đề: SendInput backend chiếm cursor thật → không thể vừa farm vừa dùng máy.
- Giải pháp: `CreateDesktop("BotDesk")` + `SetThreadDesktop` ở worker → cursor riêng cho game.
- Context: account chính, official PT + XGC, có throwaway acc để soak test.

## Stack
Đã có: C++17, MSVC v143, vcpkg, ImGui, WGC. Thêm: `user32!CreateDesktop`, `PrintWindow(PW_RENDERFULLCONTENT)`.

## Phases

| # | Phase | File | Effort | Status | Blockers |
|---|---|---|---|---|---|
| 00 | Spike: desktop-probe ⚠️ GATE | [phase-00-desktop-probe.md](phase-00-desktop-probe.md) | 1-2d | pending | — |
| 01 | PrintWindow capture backend | [phase-01-printwindow-capture.md](phase-01-printwindow-capture.md) | 2d | pending | 00 |
| 02 | Desktop manager + worker thread split | [phase-02-desktop-manager-and-worker.md](phase-02-desktop-manager-and-worker.md) | 2d | pending | 00 |
| 03 | Game launcher (CreateProcess lpDesktop) | [phase-03-game-launcher.md](phase-03-game-launcher.md) | 1d | pending | 02 |
| 04 | Switch hotkey + UI polish | [phase-04-switch-hotkey-and-ui.md](phase-04-switch-hotkey-and-ui.md) | 1d | pending | 02,03 |
| 05 | Soak test throwaway acc | [phase-05-soak-test.md](phase-05-soak-test.md) | 1w passive | pending | 01,02,03,04 |

## Critical Gate

Phase 00 là **HARD GATE**: nếu probe cho thấy (a) cursor Default vẫn bị động, hoặc (b) PrintWindow không capture được PT trên BotDesk, hoặc (c) PT pause logic khi BotDesk inactive → **abandon plan**, quay về Coexist Mode (auto-pause foreground).

## Dependencies
- 01 và 02 có thể parallel sau khi 00 pass.
- 03, 04 sequential sau 02.
- 05 passive sau khi 01-04 merged.

## Success Criteria
- User browse web trên Default, cursor không bị tool kéo.
- Tool farm PT trên BotDesk, vision đọc HP/MP chính xác, combat FSM chạy.
- Soak test 7 ngày throwaway acc: không bị flag, không disconnect bất thường.

## Risk Summary
- 🔴 XGC detect non-default desktop → mitigated by Phase 05 soak before main acc.
- 🟡 PrintWindow chậm → mitigated by 20Hz target vẫn fit.
- 🟢 PT pause khi inactive → user đã confirm PT chạy ok Alt-Tab.
