# Phase 02 — Desktop Manager + Worker Thread Split

## Context
- Plan: [plan.md](plan.md)
- Existing: [src/main.cpp](../../src/main.cpp), [src/input/input-scheduler.cpp](../../src/input/input-scheduler.cpp)

## Overview
- Priority: P1
- Status: pending
- Mục tiêu: tách process thành 2 thread — UI trên Default desktop, Worker trên BotDesk; Worker chạy vision + input + combat FSM.

## Key Insights
- `SetThreadDesktop` chỉ thành công nếu thread CHƯA tạo HWND.
- Worker thread không được dùng ImGui hay bất kỳ window API tạo HWND.
- IPC giữa 2 thread: shared atomic state (vision results, command flags) + std::mutex cho UI log.

## Requirements
- F1: `DesktopManager` quản lý lifecycle `CreateDesktop` / `CloseDesktop`.
- F2: Worker thread tự attach BotDesk khi start, log error nếu fail.
- F3: UI thread vẫn responsive, không block bởi vision/combat.
- F4: Shared state thread-safe.

## Architecture
```
WindowHelper.exe
├── main() — UI thread on Default
│   ├── ImGui + tray + config
│   ├── shared: std::atomic<VisionState>, std::atomic<CombatCommand>
│   └── std::thread workerTh(workerEntry, &shared, botDeskHandle)
│
└── workerEntry()
    ├── SetThreadDesktop(botDeskHandle) → assert success
    ├── IFrameSource (PrintWindow) + VisionPipeline
    ├── InputScheduler + SendInputBackend
    ├── Loop @ 20Hz:
    │   ├── frame = capture
    │   ├── visionState = analyze(frame)
    │   ├── shared.vision.store(visionState)
    │   ├── cmd = shared.command.exchange(none)
    │   ├── if (cmd) inputScheduler.send(...)
    │   └── combatFsm.tick(visionState)
    └── On exit: detach desktop, cleanup
```

## Related Code Files
- Create: `src/desktop/desktop-manager.h`
- Create: `src/desktop/desktop-manager.cpp`
- Create: `src/worker/worker-thread.h`
- Create: `src/worker/worker-thread.cpp`
- Create: `src/state/shared-state.h` (atomic VisionState + command queue)
- Update: `src/main.cpp` — split UI vs worker, không khởi tạo InputScheduler/VisionPipeline trên main thread nữa
- Update: `src/ui/main-window.cpp` — đọc shared state để render gauge
- Update: `src/CMakeLists.txt`

## Implementation Steps
1. `desktop-manager.{h,cpp}`:
   ```cpp
   class DesktopManager {
   public:
       bool create(const std::wstring& name);  // creates BotDesk
       HDESK handle() const { return desk_; }
       bool attachCurrentThread();             // SetThreadDesktop
       ~DesktopManager();                       // CloseDesktop
   private:
       HDESK desk_ = nullptr;
       std::wstring name_;
   };
   ```
   - Trong `create`: dùng `CreateDesktopW` với access mask đầy đủ (xem phase 00).
2. `shared-state.h`:
   ```cpp
   struct SharedState {
       std::atomic<VisionState> vision;          // POD, lock-free
       std::atomic<bool> stopRequested{false};
       std::atomic<bool> combatEnabled{false};
       // Log mirror cho UI:
       std::mutex logMu;
       std::deque<std::string> logRing;          // capped, ring buffer
   };
   ```
3. `worker-thread.{h,cpp}`:
   - Entry function nhận `SharedState*` + `HDESK`.
   - Call `SetThreadDesktop(hdesk)` → log + return nếu fail.
   - Init `PrintWindowFrameSource` (target HWND của PT trên BotDesk — Phase 03 sẽ cung cấp).
   - Init VisionPipeline + InputScheduler + SendInputBackend + AttackSweep + PotEvaluator + CombatFSM (giống main.cpp hiện tại).
   - Loop tick: bắt frame → vision → store atomic → FSM → input.
   - Khi `stopRequested` → break + cleanup.
4. Refactor `main.cpp`:
   - Bỏ phần init vision/input/combat khỏi main thread.
   - Tạo `DesktopManager`, `SharedState`, spawn worker thread.
   - UI loop chỉ đọc `SharedState` để hiển thị HP/MP/log.
5. UI changes minimal:
   - `MainWindow::render()` đọc `shared.vision.load()`.
   - Log panel đọc `shared.logRing` (lock briefly).

## Todo
- [ ] DesktopManager class
- [ ] SharedState struct + atomic VisionState (verify is_trivially_copyable cho atomic)
- [ ] worker-thread skeleton + SetThreadDesktop
- [ ] Move vision/input/combat init từ main vào worker
- [ ] UI đọc shared state thay vì callback trực tiếp
- [ ] CMakeLists update
- [ ] Smoke test: build + chạy trên Default desktop (BotDesk == Default tạm thời để test thread split)

## Success Criteria
- Build pass.
- Worker thread chạy độc lập, UI vẫn 60fps.
- VisionState propagate qua atomic store/load, UI hiển thị gauge real-time.
- Không có race condition (test với ThreadSanitizer nếu có, hoặc inspection).

## Risk Assessment
- Risk: `std::atomic<VisionState>` không lock-free nếu struct quá lớn → fallback `std::mutex` + state.
- Risk: SetThreadDesktop fail vì thread tạo HWND ngầm (ImGui hoặc OpenCV) → đảm bảo worker không khởi tạo gì tạo HWND trước call.
- Risk: Memory ordering bug → dùng `memory_order_seq_cst` (default), tối ưu sau.

## Security Considerations
- Shared state pointer truyền vào thread phải sống đủ lâu (lifetime trong main scope).

## Next Steps
- Phase 03: tích hợp game launcher để worker biết HWND của PT trên BotDesk.
