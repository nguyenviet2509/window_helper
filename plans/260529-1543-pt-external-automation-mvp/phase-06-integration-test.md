# Phase 6 — Integration Test (Mock + Replay)

**Est:** 1 ngày
**Priority:** P0 — gate trước khi đụng PT live
**Status:** pending
**Depends:** Phase 1–5 (toàn bộ pipeline)

## Mục Tiêu
Verify end-to-end tool hoạt động đúng trên:
1. PtMockGame.exe (simulation controlled)
2. FileReplaySource từ video record PT thật

## Mock Game Enhancements (Section 33.2)

Bổ sung vào `mock/` từ Phase 1 skeleton:

### 6.1 Auto-Simulation Modes
```cpp
class MockSimulator {
public:
    void enableDamageTick(bool on, int dmgPctPerTick, int intervalMs);
    void enableMpDrainOnKey(WORD vk, int dropPct);
    void enableMobAlive(bool alive);  // toggle: alive → HP/MP biến động ngẫu nhiên; dead → stable
    void enableHpRegen(double pctPerSec);
    void update(TimePoint now);
};
```

### 6.2 Input Logger
- Hook `WndProc` của mock window: log mọi `WM_KEYDOWN/UP`, `WM_RBUTTONDOWN/UP`, `WM_KEYDOWN(VK_LSHIFT)` etc với timestamp.
- Display log panel trong mock UI.

### 6.3 Click Logger
- Log tọa độ mỗi `WM_RBUTTONDOWN` → verify Attack Sweep nằm trong annulus.

## Test Scenarios (Section 33.2 + 22.9)

### TC-01: Hồi HP cơ bản
1. Chạy mock + tool, attach.
2. UI: HP threshold = 60, key = `1`.
3. Bật AUTO.
4. Kéo HP slider xuống 50%.
5. **Expected:** mock log key `1` xuất hiện < 200ms.

### TC-02: HP Emergency P0 Preempt
1. Tool đang BUFFING (F2 đang gửi).
2. Kéo HP slider xuống 35% (dưới emergency 40%).
3. **Expected:** key `1` ngay với jitter < 8ms. Buff sequence resume sau pot.

### TC-03: Buff Cycle Sequential
1. Bật AUTO, Buff 1/2/3/4 đều enabled.
2. **Expected sequence in log:** F2 → RBUTTON(no shift) → wait 500ms → F3 → RBUTTON → ... → F5 → RBUTTON → F1.

### TC-04: Attack Sweep
1. Sau ARMING, mob_alive=ON.
2. **Expected:** SHIFT+RBUTTON tại positions, x ∈ [center_x-200, center_x+200], y tương tự. Random distribution.

### TC-05: Smart Mob Death Detect
1. Tool đang ATTACK (mob_alive=ON, MP đang drain).
2. Toggle mob_alive=OFF (HP/MP stable).
3. **Expected:** sau ~2s tool gửi click mới (repick).

### TC-06: Recall Safety
1. HP slider 12%, giữ 4 giây.
2. **Expected:** key `F12` gửi đi.

### TC-07: OutputGate Foreground
1. Tool đang AUTO trong mock foreground.
2. Alt-tab sang Notepad.
3. **Expected:** input dropped log, mock không nhận key. Quay lại mock → resume.

### TC-08: OutputGate Minimize
1. AUTO running.
2. Minimize mock.
3. **Expected:** capture pause + input dropped + state UNSAFE.

### TC-09: Session Lock
1. AUTO running.
2. Win+L lock máy. Unlock.
3. **Expected:** tool pause hoàn toàn → resume sau unlock.

### TC-10: Cycle Re-buff
1. AUTO running.
2. Set cycle_duration = 30s (testing).
3. **Expected:** sau 30s ATTACKING, FSM quay lại BUFFING (F2→F3→...).

### TC-11: Humanizer Break
1. AUTO running 30 phút.
2. **Expected:** ít nhất 1 break 3–8s xen kẽ (log timing gap).

## Replay Video Tests

### Setup
1. Record OBS từ PT offline (private server) 10 phút farming → MP4.
2. Save vào `assets/replays/pt-farm-001.mp4`.
3. Config `advanced.capture.backend = "replay"`.

### TC-RP-01: Detect Accuracy
Run tool with replay. Hand-annotate 50 frame với HP/MP/SP ground truth (eyeball % từ bar). Compare tool output:
- **Expected:** ±3% MAE cho mỗi bar.

### TC-RP-02: Particle Robustness
Chọn segment có effect/spell che orb. Verify EMA + 2-frame confirm không trigger false pot.

### TC-RP-03: Loading Screen
Segment có cutscene/loading đen.
- **Expected:** state đổi UNSAFE → input dropped, không send key vô nghĩa.

## Bug Tracking
Tạo `docs/test-results.md` ghi kết quả từng TC. Bug nào fail → log + fix → re-run.

## Acceptance
- [ ] All 11 mock test cases pass
- [ ] 3 replay test cases pass
- [ ] Tool log không có ERROR/CRITICAL
- [ ] CPU < 5% steady, RAM < 120MB steady
- [ ] No memory leak qua 1h soak trên mock

## Risks
- Mock không hoàn toàn giống PT thật — replay phase quan trọng để bridge gap
- Replay frame timing fixed 20Hz có thể không match game FPS thật → adjust nếu cần

## Next
Phase 7 soak test real PT offline.
