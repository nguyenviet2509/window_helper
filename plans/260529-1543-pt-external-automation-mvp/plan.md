---
name: PT External Automation MVP
slug: pt-external-automation-mvp
created: 2026-05-29
status: pending
type: implementation
estimatedDays: 12.5
totalLoc: ~3000
brainstorm: ../reports/brainstorm-260529-1404-pt-external-automation.md
blockedBy: []
blocks: []
---

# PT External Automation Assistant — MVP Plan

External-only Win32 automation cho Priston Tale (auto-pot HP/MP/SP + buff cycle + SHIFT+right-click attack).

## Stack
C++17, MSVC v143, vcpkg (OpenCV4, nlohmann_json, imgui[dx11-binding,win32-binding]), CMake, WinRT (WGC), Dear ImGui + D3D11.

## Output
`WindowHelper.exe` — single Win32 GUI exe ~5–8 MB, portable, không cần Admin, VersionInfo benign.

## Phases

| # | Phase | File | Est | Status |
|---|---|---|---|---|
| 0 | PostMessage Probe | [phase-00](phase-00-postmessage-probe.md) | 30m | code-ready |
| 1 | Capture WGC + Vision Detector + Mock skeleton | [phase-01](phase-01-capture-and-vision.md) | 2d | code-ready |
| 2 | Input Backend + Humanizer + OutputGate | [phase-02](phase-02-input-humanizer-gate.md) | 2d | code-ready |
| 3 | Action Dispatcher + Combat FSM + Mob Death Detect | [phase-03](phase-03-dispatcher-combat-fsm.md) | 2d | code-ready |
| 4 | Config + ImGui UI + Calibrate | [phase-04](phase-04-config-ui-calibrate.md) | 2d | code-ready (Calibrate overlay deferred) |
| 5 | Logger + Tray + Hotkey + Naming/VersionInfo | [phase-05](phase-05-logger-tray-naming.md) | 1d | code-ready (app.ico binary deferred) |
| 6 | Integration Test Mock + Replay | [phase-06](phase-06-integration-test.md) | 1d | manual-only (scenarios in build-deploy-test-guide §5) |
| 7 | Soak Test PT Offline + Tuning | [phase-07](phase-07-soak-tuning.md) | 1d | manual-only |
| 8 | Build & Deploy & Test Simulation Guide (docs) | [phase-08](phase-08-build-deploy-test-guide.md) | 0.5d | done — [docs/build-deploy-test-guide.md](../../docs/build-deploy-test-guide.md) |

## Key Decisions (MVP — bỏ khỏi v1)
- ❌ DXGI Duplication fallback (chỉ WGC)
- ❌ Template matching mob (chỉ Stationary Sweep + SHIFT+right-click)
- ❌ Profile system multi-config
- ❌ Auto-detect input backend runtime (probe 1 lần, hardcode)
- ❌ Multi-resolution profile (chỉ 800×600)
- ❌ Code signing, random tên build
- ❌ GTest framework (test qua Mock + Replay)

## Critical Features (GIỮ)
- ✅ HP Priority P0 preempt (cứu char)
- ✅ Combat FSM: BUFFING (F2→F3→F4→F5 + right-click) → ARMING (F1) → ATTACKING (SHIFT+right-click sweep) → re-buff sau 300s
- ✅ Smart mob death detect (MP+HP sliding window 2s)
- ✅ OutputGate + Safe Mode (capture health FSM)
- ✅ Humanizer (Gaussian jitter, break, session pause)
- ✅ Vision waterline detection bar đứng (HP đỏ, SP vàng, MP xanh dương)

## Dependencies Between Phases
Phase 0 → quyết định input backend cho Phase 2.
Phase 1 + 2 chạy độc lập sau Phase 0.
Phase 3 cần Phase 1 (vision) + Phase 2 (input).
Phase 4 cần Phase 3 (UI bind Config + FSM).
Phase 5 cosmetic, sau Phase 4.
Phase 6 cần tất cả.
Phase 7 sau Phase 6.
Phase 8 sau Phase 7 (docs cho user/dev).

## Success Criteria
- HP/MP/SP detect accuracy > 96% sau calibrate
- HP emergency P0 latency < 150ms từ vision → key send
- Smart death detect repick đúng < 2s sau mob chết
- Soak test 8h offline: no crash, no leak (< 10 MB RSS drift), no input misfire
- Mock test: all scenarios Section 33.2 pass
