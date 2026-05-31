# Brainstorm: Pot Refill from Inventory

**Date**: 2026-05-31
**Status**: Design approved — pending plan creation
**Scope**: Add periodic pot-refill feature that opens inventory, hovers slots, and uses Shift+1/2/3 to refill HP/SP/MP stocks.

---

## 1. Problem Statement

Nhân vật chỉ có lượng pot HP/SP/MP nhất định trong hotbar. Khi cạn → combat dừng. Cần tự động nạp lại pot từ kho theo interval cấu hình, không cần user can thiệp.

## 2. Game Action Map (Authoritative)

| Action | Key/Mouse |
|--------|-----------|
| Pot HP ingame | `1` |
| Pot SP ingame | `2` |
| Pot MP ingame | `3` |
| Attack | Shift + Right Mouse |
| Toggle inventory | `V` |
| Refill HP (kho) | mouse → slot HP coords + Shift+`1` |
| Refill SP (kho) | mouse → slot SP coords + Shift+`2` |
| Refill MP (kho) | mouse → slot MP coords + Shift+`3` |

→ `PotConfig` default hiện tại (`VK_F1/F2/F3`) cần đổi thành `'1'/'2'/'3'`. Combat attack đã dùng shift+right-click → khớp.

## 3. Requirements

**Functional**
- Interval riêng cho HP/SP/MP (sec, 0 = disabled).
- Tọa độ slot HP/SP/MP (client coords) nhập trong `config.json`.
- Khi tới interval: pause combat → mở kho → move mouse → Shift+N atomic → đóng kho → restore cursor → resume.
- Nhiều slot tới hạn cùng lúc → gộp 1 lần mở kho, thứ tự **HP → MP → SP**.
- HP critical (`< hpCriticalAbortThreshold`) mid-refill → abort an toàn → pot evaluator fire HP ingame.

**Non-functional**
- KISS: state machine driven bằng `nextStepAt_`, không thread riêng.
- Anti-pattern: tận dụng `mouse-path` Bezier có sẵn cho move; jitter các delay (option).
- Defense-in-depth pause: source skip + gate flag.

## 4. Evaluated Approaches (Pause Mechanism)

| Approach | Pros | Cons |
|----------|------|------|
| A. Source-only flag | Đơn giản | Queue cũ vẫn fire sau khi flag set |
| B. Gate-only | Centralized | Queue dày lên trong refill → burst khi resume |
| **C. Hybrid (CHOSEN)** | Defense-in-depth, không pollute queue, không burst | Cần sửa 2 chỗ (source + gate) |

## 5. Final Design

### 5.1 Config (`game-state.h` + `config-loader.cpp`)
```cpp
struct PotRefillSlot {
    int x = 0, y = 0;          // client coords
    int intervalSec = 0;       // 0 = disabled
};
struct PotRefillConfig {
    bool enabled = false;
    WORD inventoryToggleKey = 'V';
    int inventoryOpenDelayMs  = 400;
    int inventoryCloseDelayMs = 200;
    int mouseMoveDelayMs   = 150;
    int postHotkeyDelayMs  = 200;
    int refillTimeoutMs    = 10000;       // force CLEANUP nếu treo
    double hpCriticalAbortThreshold = 0.30;
    int abortBackoffMs = 30000;
    PotRefillSlot hp;   // Shift+1
    PotRefillSlot sp;   // Shift+2
    PotRefillSlot mp;   // Shift+3
};
// AppConfig::refill = PotRefillConfig{};
```
PotConfig defaults đổi: `hpKey='1'`, `mpKey='3'`, `spKey='2'`.

### 5.2 Module mới: `src/combat/pot-refill-scheduler.{h,cpp}`

**API**
```cpp
class PotRefillScheduler {
public:
    PotRefillScheduler(InputScheduler&, OutputGate&, HWND, const PotRefillConfig&);
    void tick(const VisionState& v, std::chrono::steady_clock::time_point now);
    bool busy() const;                                  // for combat/pot skip
    void updateConfig(const PotRefillConfig&);
    void setEnabled(bool);
};
```

**State machine**
```
IDLE
 → (any slot due && hpPct >= critical) SAVE_CURSOR
 → OPEN_INV       (tap V) wait inventoryOpenDelayMs
 → for each due slot in {HP, MP, SP}:
       MOVE_<R>   (mouse → slot.x,y via Bezier) wait mouseMoveDelayMs
       FIRE_<R>   (atomic: KeyDown LSHIFT; KeyTap '1'/'2'/'3'; KeyUp LSHIFT) wait postHotkeyDelayMs
       update lastRefillAt[R] = now
 → CLOSE_INV      (tap V) wait inventoryCloseDelayMs
 → CLEANUP        (sendKeyUp LSHIFT defensive; restore cursor; clear refillActive_)
 → IDLE

[abort branch — kiểm tra mỗi tick từ OPEN_INV trở đi]
 → if v.valid && hpPct < hpCriticalAbortThreshold:
      ABORT → sendKeyUp LSHIFT → tap V (close) → restore cursor → backoff abortBackoffMs → IDLE

[timeout — refillTimeoutMs từ SAVE_CURSOR]
 → force CLEANUP (best-effort: tap V để đóng kho, release shift, restore cursor)
```

### 5.3 Pause integration (Hybrid)

**`OutputGate`** — thêm `std::atomic<bool> refillActive_{false}` + setter; `allowInput()` đối với commands priority ≥ `P2_Pot` trả `false` khi `refillActive_`.

**Priority bổ sung** trong `dispatch/priority.h`:
```
P0_Critical, P1_Refill (NEW), P2_Pot, P3_Combat
```

**Source skip**: `CombatFsm::tick` và `PotEvaluator::eval*` check `refill.busy()` → return early (không sinh cmd mới).

### 5.4 Atomic Shift+N

```cpp
[vk](IInputBackend& b) {
    b.sendKeyDown(VK_LSHIFT);
    b.sendKeyTap(vk, 30);
    b.sendKeyUp(VK_LSHIFT);
}
```
Tận dụng API có sẵn (pattern giống `SendInputBackend::sendShiftRightClick`). Không sửa `IInputBackend`.

### 5.5 Cursor save/restore

- Trước SAVE_CURSOR: `GetCursorPos` → `ScreenToClient(target_)` → lưu.
- CLEANUP: `ClientToScreen` → `SetCursorPos`.
- Backend hiện có `sendMouseMove(x,y)` (client coords) — dùng cho restore qua scheduler để đồng bộ.

### 5.6 File changes

| File | Loại | Mô tả |
|------|------|------|
| `src/state/game-state.h` | M | + `PotRefillSlot`, `PotRefillConfig`; `AppConfig::refill`; đổi PotConfig defaults |
| `src/config/config-loader.cpp` | M | to/fromJson cho `PotRefillConfig` |
| `src/dispatch/priority.h` | M | + `P1_Refill` |
| `src/core/output-gate.{h,cpp}` | M | + `refillActive_` flag + getter dùng trong `allowInput` |
| `src/combat/pot-refill-scheduler.{h,cpp}` | A | **Module mới** — state machine |
| `src/combat/combat-fsm.cpp` | M | Skip tick khi `refill.busy()` |
| `src/combat/pot-evaluator.cpp` | M | Skip khi `refill.busy()` |
| `src/main.cpp` | M | Wire scheduler vào main tick loop |
| `src/ui/main-window.cpp` | M | Hiển thị config + countdown read-only |
| `config.json` | M | Schema mới (`refill: {...}`) |

## 6. Risks & Mitigation

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Kho đã mở thủ công → V đóng thay vì mở | Refill fail silently | Document; user không can thiệp khi bot chạy |
| Gate denied giữa refill (foreground mất) | State machine treo | Timeout 10s → force CLEANUP |
| Stuck shift modifier sau crash | Mọi click thành shift-click | CLEANUP luôn `sendKeyUp(LSHIFT)`; atomic lambda gói chặt down/up |
| HP cycle quanh critical → abort loop vô tận | Pot không nạp được | Backoff 30s sau abort |
| Buff cycle restart trùng refill | Buff skip 1 tick | Không reset `cycleStart_` → buff timer giữ nguyên |
| Vision invalid lúc refill | Pot evaluator vốn đã skip; refill không cần vision | Refill vẫn chạy bình thường |
| Mouse path Bezier chậm (>delays) | Race với hotkey | `mouseMoveDelayMs` đủ lớn (default 150ms) sau khi path xong |

## 7. Success Criteria

- Khi `enabled=false`: hành vi cũ giữ nguyên 100%, không regression.
- Khi `enabled=true` + interval=60s: sau ~60s thấy log `[refill] BEGIN ... CLEANUP elapsedMs=<2000`.
- HP drop < 0.30 mid-refill: log `[refill] ABORT` → kho đóng → `[pot.hp] FIRE vk=49` (mã '1') trong vòng <500ms.
- Cursor trở lại vị trí farm sau refill.
- Combat resume tự nhiên (state Buffing/Attacking giữ nguyên).

## 8. Next Steps

1. Tạo plan chi tiết qua `/ck:plan` (multi-phase: config → module mới → tích hợp gate → wire main → UI → test).
2. Test thủ công với interval ngắn (5s) trước khi tăng dần.
3. Test edge case: HP critical, foreground loss, manual inventory toggle.

## 9. Unresolved Questions

1. Có cần countdown UI hiển thị "HP refill in: 4m 32s" không? (Recommend: **có**, read-only, debug-friendly. Không phải nút trigger.)
2. Có muốn UI hiện status hiện tại của refill (IDLE/OPENING/REFILLING HP/...) không? (Recommend: **có**, 1 dòng text.)
3. Sau abort do HP critical, có muốn force trigger pot ingame ngay (không chờ pot-evaluator confirm frames) không? (Recommend: **không** — pot-evaluator confirm để chống false positive vẫn cần thiết.)
