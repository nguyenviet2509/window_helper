#include "pot-refill-scheduler.h"
#include "../core/logger.h"
#include "../dispatch/priority.h"

#include <opencv2/imgproc.hpp>

using ms = std::chrono::milliseconds;
using sec = std::chrono::seconds;

// Tọa độ pot trong kho — đo trong Paint từ screenshot toàn cửa sổ PT (window-relative,
// bao gồm title bar + border). Tại runtime sẽ convert sang client coords qua
// windowToClient(). KHÔNG được sửa các giá trị này trừ khi PT thay UI kho.
namespace {
struct Bbox { int l, t, r, b; };
constexpr Bbox HP_BBOX{268, 462, 290, 484};
constexpr Bbox SP_BBOX{268, 484, 291, 501};
constexpr Bbox MP_BBOX{268, 506, 290, 528};
// Tọa độ slot CORE (cuộn tp về làng) trên thanh hotbar dưới — window-relative (Paint).
constexpr Bbox CORE_BBOX{310, 717, 362, 744};
constexpr int CORE_CX = (CORE_BBOX.l + CORE_BBOX.r) / 2;
constexpr int CORE_CY = (CORE_BBOX.t + CORE_BBOX.b) / 2;
constexpr int HP_CX = (HP_BBOX.l + HP_BBOX.r) / 2;
constexpr int HP_CY = (HP_BBOX.t + HP_BBOX.b) / 2;
constexpr int SP_CX = (SP_BBOX.l + SP_BBOX.r) / 2;
constexpr int SP_CY = (SP_BBOX.t + SP_BBOX.b) / 2;
constexpr int MP_CX = (MP_BBOX.l + MP_BBOX.r) / 2;
constexpr int MP_CY = (MP_BBOX.t + MP_BBOX.b) / 2;

// Kích thước window khi user đo trong Paint (logical pixels). Code chuyển đổi
// sang physical pixels tại runtime bằng tỷ lệ với window thật. Nếu user thay
// đổi DPI hoặc kích thước window, scale auto-adjust.
constexpr int PAINT_REF_W = 808;
constexpr int PAINT_REF_H = 630;

// Convert tọa độ window-relative (đo trong Paint từ screenshot toàn cửa sổ) sang
// client-relative (cái mà backend.sendMouseMove dùng qua ClientToScreen).
// Trừ đi offset của title bar + border trên/trái.
void windowToClient(HWND h, int wxRef, int wyRef, int& cx, int& cy) {
    RECT wr{}, cr{};
    POINT clientTL{0, 0};
    if (!h || !GetWindowRect(h, &wr) || !ClientToScreen(h, &clientTL) || !GetClientRect(h, &cr)) {
        cx = wxRef; cy = wyRef;
        return;
    }
    int winW = wr.right - wr.left;
    int winH = wr.bottom - wr.top;
    // Scale từ Paint reference (logical) sang physical window size hiện tại.
    float sx = (float)winW / (float)PAINT_REF_W;
    float sy = (float)winH / (float)PAINT_REF_H;
    int wxPhys = (int)((float)wxRef * sx + 0.5f);
    int wyPhys = (int)((float)wyRef * sy + 0.5f);
    int dx = clientTL.x - wr.left;
    int dy = clientTL.y - wr.top;
    cx = wxPhys - dx;
    cy = wyPhys - dy;
    Logger::instance().logf(LogLevel::Info,
        "[refill.dbg] win=%dx%d ref=%dx%d scale=(%.3f,%.3f) | "
        "input ref=(%d,%d) phys=(%d,%d) offset=(%d,%d) -> client=(%d,%d)",
        winW, winH, PAINT_REF_W, PAINT_REF_H, sx, sy,
        wxRef, wyRef, wxPhys, wyPhys, dx, dy, cx, cy);
}

// Sample 1 vùng ROI nhỏ trong frame và quyết định slot rỗng:
//   - Đổi BGRA -> HSV.
//   - Slot rỗng có background xám tối: saturation rất thấp + value thấp.
//   - Slot chứa pot có icon bão hòa cao (đỏ/xanh) -> meanSat lớn.
// Threshold dựa trên quan sát PT: empty avgSat thường <15, có pot >40.
bool sampleSlotEmpty(const cv::Mat& bgra, int cxClient, int cyClient) {
    if (bgra.empty()) return false;
    // ROI 10x10 quanh center (tránh số stack ở góc trên-phải slot).
    const int half = 5;
    int x0 = std::max(0, cxClient - half);
    int y0 = std::max(0, cyClient - half);
    int x1 = std::min(bgra.cols - 1, cxClient + half);
    int y1 = std::min(bgra.rows - 1, cyClient + half);
    if (x1 <= x0 || y1 <= y0) return false;
    cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
    cv::Mat patch = bgra(roi);
    cv::Mat bgr, hsv;
    cv::cvtColor(patch, bgr, cv::COLOR_BGRA2BGR);
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    cv::Scalar meanHsv = cv::mean(hsv);
    double avgSat = meanHsv[1];
    double avgVal = meanHsv[2];
    bool empty = (avgSat < 25.0) && (avgVal < 90.0);
    Logger::instance().logf(LogLevel::Info,
        "[refill.probe] slot center=(%d,%d) avgSat=%.1f avgVal=%.1f -> %s",
        cxClient, cyClient, avgSat, avgVal, empty ? "EMPTY" : "HAS_POT");
    return empty;
}
}   // namespace

PotRefillScheduler::PotRefillScheduler(InputScheduler& s, OutputGate& g, HWND t,
                                       const PotRefillConfig& cfg)
    : sched_(s), gate_(g), target_(t), cfg_(cfg) {}

void PotRefillScheduler::scheduleStep(int prio, TP fireAt,
                                      std::function<void(IInputBackend&)> action) {
    InputCmd c;
    c.priority = prio;
    c.bypassRefillGate = true;   // tránh self-block khi refillActive_=true
    c.fireAt = fireAt;
    c.action = std::move(action);
    sched_.schedule(std::move(c));
}

bool PotRefillScheduler::isDue(Slot s, TP now) const {
    const auto& sc = slotCfg(s);
    if (sc.intervalSec <= 0) return false;
    TP last{};
    switch (s) {
        case Slot::Hp: last = lastHpAt_; break;
        case Slot::Sp: last = lastSpAt_; break;
        case Slot::Mp: last = lastMpAt_; break;
        default: return false;
    }
    if (last.time_since_epoch().count() == 0) return true;   // chưa từng refill
    return (now - last) >= sec(sc.intervalSec);
}

bool PotRefillScheduler::anyDue(TP now) const {
    return isDue(Slot::Hp, now) || isDue(Slot::Sp, now) || isDue(Slot::Mp, now);
}

const PotRefillSlot& PotRefillScheduler::slotCfg(Slot s) const {
    switch (s) {
        case Slot::Hp: return cfg_.hp;
        case Slot::Sp: return cfg_.sp;
        case Slot::Mp: return cfg_.mp;
        default:       return cfg_.hp;
    }
}

WORD PotRefillScheduler::slotVk(Slot s) const {
    switch (s) {
        case Slot::Hp: return '1';
        case Slot::Sp: return '2';
        case Slot::Mp: return '3';
        default:       return 0;
    }
}

PotRefillScheduler::TP& PotRefillScheduler::lastRefillAt(Slot s) {
    switch (s) {
        case Slot::Hp: return lastHpAt_;
        case Slot::Sp: return lastSpAt_;
        case Slot::Mp: return lastMpAt_;
        default:       return lastHpAt_;
    }
}

PotRefillScheduler::Slot PotRefillScheduler::pickNextSlot() {
    // Order: HP -> MP -> SP
    if (slotsPlanned_[0]) { slotsPlanned_[0] = false; return Slot::Hp; }
    if (slotsPlanned_[2]) { slotsPlanned_[2] = false; return Slot::Mp; }
    if (slotsPlanned_[1]) { slotsPlanned_[1] = false; return Slot::Sp; }
    return Slot::None;
}

const char* PotRefillScheduler::stateName() const {
    switch (state_) {
        case State::Idle:           return "IDLE";
        case State::OpenInv:        return "OPEN_INV";
        case State::MoveSlot:       return "MOVE_SLOT";
        case State::FireSlot:       return "FIRE_SLOT";
        case State::CloseInv:       return "CLOSE_INV";
        case State::Cleanup:        return "CLEANUP";
        case State::AbortCloseInv:  return "ABORT_CLOSE";
        case State::AbortCleanup:   return "ABORT_CLEANUP";
        case State::CoreMove:       return "CORE_MOVE";
        case State::CoreClick:      return "CORE_CLICK";
    }
    return "?";
}

int PotRefillScheduler::secondsUntilNext(char which, TP now) const {
    Slot s = Slot::None;
    const PotRefillSlot* sc = nullptr;
    TP last{};
    switch (which) {
        case 'h': s = Slot::Hp; sc = &cfg_.hp; last = lastHpAt_; break;
        case 's': s = Slot::Sp; sc = &cfg_.sp; last = lastSpAt_; break;
        case 'm': s = Slot::Mp; sc = &cfg_.mp; last = lastMpAt_; break;
        default: return -1;
    }
    if (!sc || sc->intervalSec <= 0) return -1;
    if (last.time_since_epoch().count() == 0) return 0;
    auto target = last + sec(sc->intervalSec);
    if (target <= now) return 0;
    return (int)std::chrono::duration_cast<sec>(target - now).count();
}

void PotRefillScheduler::beginRefill(TP now) {
    // Snapshot cursor (screen coords).
    if (!GetCursorPos(&savedCursorScreen_)) {
        savedCursorScreen_ = { 0, 0 };
    }
    // Plan due slots.
    slotsPlanned_[0] = isDue(Slot::Hp, now);
    slotsPlanned_[1] = isDue(Slot::Sp, now);
    slotsPlanned_[2] = isDue(Slot::Mp, now);

    gate_.setRefillActive(true);
    refillStartedAt_ = now;

    // Tap V (open inventory).
    WORD vk = cfg_.inventoryToggleKey;
    scheduleStep(P0_HpEmergency, now, [vk](IInputBackend& b) {
        b.sendKeyTap(vk, 30);
    });
    state_ = State::OpenInv;
    nextStepAt_ = now + ms(cfg_.inventoryOpenDelayMs);

    Logger::instance().logf(LogLevel::Info,
        "[refill] BEGIN slots=%s%s%s cursor=(%ld,%ld)",
        slotsPlanned_[0] ? "H" : "",
        slotsPlanned_[2] ? "M" : "",
        slotsPlanned_[1] ? "S" : "",
        savedCursorScreen_.x, savedCursorScreen_.y);
}

void PotRefillScheduler::enterAbort(TP now) {
    // Defensive: release shift trước khi đóng kho.
    scheduleStep(P0_HpEmergency, now, [](IInputBackend& b) { b.sendKeyUp(VK_LSHIFT); });
    // Đóng kho.
    WORD vk = cfg_.inventoryToggleKey;
    scheduleStep(P0_HpEmergency, now + ms(20), [vk](IInputBackend& b) {
        b.sendKeyTap(vk, 30);
    });
    state_ = State::AbortCloseInv;
    nextStepAt_ = now + ms(cfg_.inventoryCloseDelayMs);
    Logger::instance().log(LogLevel::Warn, "[refill] ABORT — closing inventory");
}

void PotRefillScheduler::doCleanup(TP now, bool aborted) {
    // Defensive shift release (đề phòng atomic lambda lỗi).
    scheduleStep(P0_HpEmergency, now, [](IInputBackend& b) { b.sendKeyUp(VK_LSHIFT); });

    // Restore cursor: screen -> client -> sendMouseMove.
    POINT pt = savedCursorScreen_;
    HWND tgt = target_;
    if (ScreenToClient(tgt, &pt)) {
        int cx = pt.x, cy = pt.y;
        scheduleStep(P0_HpEmergency, now + ms(20),
            [cx, cy](IInputBackend& b) { b.sendMouseMove(cx, cy); });
    }

    gate_.setRefillActive(false);
    state_ = State::Idle;
    currentSlot_ = Slot::None;
    slotsPlanned_[0] = slotsPlanned_[1] = slotsPlanned_[2] = false;
    corePending_ = false;

    auto elapsedMs = std::chrono::duration_cast<ms>(now - refillStartedAt_).count();
    if (aborted) {
        abortBackoffUntil_ = now + ms(cfg_.abortBackoffMs);
        Logger::instance().logf(LogLevel::Warn,
            "[refill] ABORTED elapsedMs=%lld backoffMs=%d",
            (long long)elapsedMs, cfg_.abortBackoffMs);
    } else {
        Logger::instance().logf(LogLevel::Info,
            "[refill] DONE elapsedMs=%lld", (long long)elapsedMs);
    }
}

void PotRefillScheduler::tick(const VisionState& v, TP now) {
    if (!enabled_ || !cfg_.enabled) return;

    // Idle: check trigger.
    if (state_ == State::Idle) {
        if (now < abortBackoffUntil_) return;
        if (!anyDue(now)) return;
        // Pre-check HP critical: hoãn nếu HP đã thấp sẵn.
        if (v.valid && v.hpPct < cfg_.hpCriticalAbortThreshold) {
            return;
        }
        beginRefill(now);
        return;
    }

    // Global timeout: nếu refill chạy quá refillTimeoutMs mà chưa về Idle -> force cleanup.
    if ((now - refillStartedAt_) > ms(cfg_.refillTimeoutMs)) {
        Logger::instance().logf(LogLevel::Warn,
            "[refill] TIMEOUT (>%dms) — force cleanup state=%s",
            cfg_.refillTimeoutMs, stateName());
        doCleanup(now, true);
        return;
    }

    // Abort check: HP critical giữa chừng (chỉ khi đang ở giữa flow, không phải lúc đang cleanup).
    if (v.valid && v.hpPct < cfg_.hpCriticalAbortThreshold
            && state_ != State::AbortCloseInv && state_ != State::AbortCleanup) {
        Logger::instance().logf(LogLevel::Warn,
            "[refill] ABORT hpPct=%.3f < critical=%.3f",
            v.hpPct, cfg_.hpCriticalAbortThreshold);
        enterAbort(now);
        return;
    }

    // Wait for nextStepAt_.
    if (now < nextStepAt_) return;

    switch (state_) {
        case State::OpenInv: {
            // Pick first due slot.
            Slot picked = pickNextSlot();
            // HP-only: probe slot pixels; nếu rỗng -> teleport core NGAY (kho phải còn mở
            // mới click được core). Skip luôn MP/SP vì teleport đổi scene.
            if (picked == Slot::Hp && snapshot_) {
                int pcx, pcy;
                windowToClient(target_, HP_CX, HP_CY, pcx, pcy);
                cv::Mat frame;
                if (snapshot_(frame) && sampleSlotEmpty(frame, pcx, pcy)) {
                    Logger::instance().log(LogLevel::Warn,
                        "[refill] HP slot EMPTY -> core teleport (kho vẫn mở)");
                    corePending_ = true;
                    lastHpAt_ = now;
                    slotsPlanned_[0] = slotsPlanned_[1] = slotsPlanned_[2] = false;
                    int cx, cy;
                    windowToClient(target_, CORE_CX, CORE_CY, cx, cy);
                    scheduleStep(P0_HpEmergency, now,
                        [cx, cy](IInputBackend& b) { b.sendMouseMove(cx, cy); });
                    state_ = State::CoreMove;
                    nextStepAt_ = now + ms(cfg_.mouseMoveDelayMs);
                    Logger::instance().logf(LogLevel::Warn,
                        "[refill] CORE_MOVE client=(%d,%d)", cx, cy);
                    break;
                }
            }
            currentSlot_ = picked;
            if (currentSlot_ == Slot::None) {
                // No slots — close inventory.
                WORD vk = cfg_.inventoryToggleKey;
                scheduleStep(P0_HpEmergency, now, [vk](IInputBackend& b) {
                    b.sendKeyTap(vk, 30);
                });
                state_ = State::CloseInv;
                nextStepAt_ = now + ms(cfg_.inventoryCloseDelayMs);
                break;
            }
            int wx, wy;
            switch (currentSlot_) {
                case Slot::Hp: wx = HP_CX; wy = HP_CY; break;
                case Slot::Sp: wx = SP_CX; wy = SP_CY; break;
                case Slot::Mp: wx = MP_CX; wy = MP_CY; break;
                default: wx = wy = 0;
            }
            int cx, cy;
            windowToClient(target_, wx, wy, cx, cy);
            scheduleStep(P0_HpEmergency, now,
                [cx, cy](IInputBackend& b) { b.sendMouseMove(cx, cy); });
            state_ = State::MoveSlot;
            nextStepAt_ = now + ms(cfg_.mouseMoveDelayMs);
            Logger::instance().logf(LogLevel::Info,
                "[refill] MOVE_%s win=(%d,%d) client=(%d,%d)",
                currentSlot_ == Slot::Hp ? "HP" : (currentSlot_ == Slot::Sp ? "SP" : "MP"),
                wx, wy, cx, cy);
            break;
        }
        case State::MoveSlot: {
            // Atomic Shift+N.
            WORD vk = slotVk(currentSlot_);
            scheduleStep(P0_HpEmergency, now, [vk](IInputBackend& b) {
                b.sendKeyDown(VK_LSHIFT);
                b.sendKeyTap(vk, 30);
                b.sendKeyUp(VK_LSHIFT);
            });
            state_ = State::FireSlot;
            nextStepAt_ = now + ms(cfg_.postHotkeyDelayMs);
            Logger::instance().logf(LogLevel::Info,
                "[refill] FIRE_%s shift+%c",
                currentSlot_ == Slot::Hp ? "HP" : (currentSlot_ == Slot::Sp ? "SP" : "MP"),
                (char)vk);
            break;
        }
        case State::FireSlot: {
            // Mark this slot refilled.
            lastRefillAt(currentSlot_) = now;
            currentSlot_ = Slot::None;
            // Pick next.
            Slot next = pickNextSlot();
            if (next != Slot::None) {
                currentSlot_ = next;
                int wx, wy;
                switch (currentSlot_) {
                    case Slot::Hp: wx = HP_CX; wy = HP_CY; break;
                    case Slot::Sp: wx = SP_CX; wy = SP_CY; break;
                    case Slot::Mp: wx = MP_CX; wy = MP_CY; break;
                    default: wx = wy = 0;
                }
                int cx, cy;
                windowToClient(target_, wx, wy, cx, cy);
                scheduleStep(P0_HpEmergency, now,
                    [cx, cy](IInputBackend& b) { b.sendMouseMove(cx, cy); });
                state_ = State::MoveSlot;
                nextStepAt_ = now + ms(cfg_.mouseMoveDelayMs);
                Logger::instance().logf(LogLevel::Info,
                    "[refill] MOVE_%s win=(%d,%d) client=(%d,%d)",
                    next == Slot::Hp ? "HP" : (next == Slot::Sp ? "SP" : "MP"),
                    wx, wy, cx, cy);
            } else {
                // Done — close inventory.
                WORD vk = cfg_.inventoryToggleKey;
                scheduleStep(P0_HpEmergency, now, [vk](IInputBackend& b) {
                    b.sendKeyTap(vk, 30);
                });
                state_ = State::CloseInv;
                nextStepAt_ = now + ms(cfg_.inventoryCloseDelayMs);
                Logger::instance().log(LogLevel::Info, "[refill] CLOSE_INV tap V");
            }
            break;
        }
        case State::CloseInv:
            doCleanup(now, /*aborted=*/false);
            break;
        case State::CoreMove: {
            int cx, cy;
            windowToClient(target_, CORE_CX, CORE_CY, cx, cy);
            scheduleStep(P0_HpEmergency, now,
                [cx, cy](IInputBackend& b) { b.sendRightClick(cx, cy); });
            state_ = State::CoreClick;
            nextStepAt_ = now + ms(cfg_.postHotkeyDelayMs);
            Logger::instance().log(LogLevel::Warn, "[refill] CORE_CLICK right-click (kho mở)");
            break;
        }
        case State::CoreClick: {
            // Đóng kho sau khi đã right-click core.
            WORD vk = cfg_.inventoryToggleKey;
            scheduleStep(P0_HpEmergency, now, [vk](IInputBackend& b) {
                b.sendKeyTap(vk, 30);
            });
            state_ = State::CloseInv;
            nextStepAt_ = now + ms(cfg_.inventoryCloseDelayMs);
            break;
        }
        case State::AbortCloseInv:
            state_ = State::AbortCleanup;
            nextStepAt_ = now;
            break;
        case State::AbortCleanup:
            doCleanup(now, /*aborted=*/true);
            break;
        case State::Idle:
        case State::Cleanup:
            break;
    }
}
