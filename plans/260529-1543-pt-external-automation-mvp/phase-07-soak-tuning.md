# Phase 7 — Soak Test PT Offline + Tuning

**Est:** 1 ngày
**Priority:** P0 — gate trước khi production
**Status:** pending
**Depends:** Phase 6 (integration test pass)

## Mục Tiêu
- Verify tool hoạt động end-to-end trên PT offline / server private không XC3
- Tune thresholds dựa trên hành vi PT thực tế
- 8-hour soak: no crash, no leak, no input misfire
- Document kết quả + known limitations

## Pre-flight Checklist
- [ ] Phase 6 all tests pass
- [ ] PT offline server / private không XC3 setup sẵn
- [ ] Tài khoản phụ throwaway character lv1–20
- [ ] OBS sẵn để record session (debug nếu crash)
- [ ] Backup config + log mỗi 30 phút

## Stages

### Stage 1: Smoke Test (30 phút)
1. PT offline 800×600 windowed.
2. Run WindowHelper.exe.
3. Attach window.
4. Calibrate HP/MP/SP qua UI.
5. Verify vision preview: kéo char qua khu damage để rớt HP → bar preview update đúng.
6. Manually pot HP → verify tool detect HP tăng lại.
7. Tắt tool. Kiểm tra log không có ERROR.

### Stage 2: Tune Vision (1–2 giờ)
Verify trên scene thật:
- HP detection accuracy: hand-record HP% mỗi 5s qua 5 phút, compare tool reading. Mục tiêu MAE < 3%.
- Nếu sai số > 5% → tune `advanced.detection_hue.hp` ranges hoặc re-calibrate region.
- SP/MP tương tự.

Edge case verify:
- Particle bay qua orb → tool false trigger không? Adjust EMA alpha (0.20–0.40) nếu cần.
- Loading screen / town → tool pause đúng? Verify state UNSAFE.

### Stage 3: Combat Behavior (1 giờ)
Bật AUTO, farm khu mob phù hợp class:
- Buff cycle có chạy đúng F2→F3→F4→F5→F1 ban đầu? Verify.
- Attack sweep tỷ lệ hit mob > 50%? Adjust sweep_radius nếu sweep quá rộng/hẹp.
- Smart death detect repick có đúng < 3s sau mob chết? Adjust min_dwell / max_dwell nếu cần.
- Re-buff sau 300s có chạy?
- Click miss (right-click ground) có làm char chạy? Verify SHIFT+right-click attack-in-place hoạt động.

### Stage 4: Pot Stress Test (30 phút)
Chủ động vào khu nhiều damage để test pot priority:
- HP rớt nhanh → P0 trigger trước MP/SP? Verify timing.
- HP critical kéo dài > 3s → P2 Recall trigger? Verify F12 gửi đúng.
- Pot cooldown 600ms có spam đúng không?

### Stage 5: 8-Hour Soak Test
1. Setup farm scene ổn định (lots of mob, no boss).
2. Bật AUTO + log INFO level.
3. Để chạy 8 giờ liên tục.
4. Mỗi giờ check qua RDP/remote: tool còn chạy? CPU/RAM stable?
5. **Kết thúc check:**
   - [ ] Tool còn alive (no crash)
   - [ ] RSS drift < 10 MB (so với start)
   - [ ] CPU steady (< 5%)
   - [ ] Log không có ERROR/CRITICAL
   - [ ] Character vẫn sống (HP > 0)
   - [ ] No stuck input (verify bằng cách bấm phím thủ công sau khi tắt tool)

## Tuning Parameters (sau khi quan sát)

| Param | Default | Tune nếu |
|---|---|---|
| `advanced.hp.confirm_frames` | 2 | False trigger nhiều → tăng 3 |
| `advanced.vision.ema_alpha` | 0.30 | Phản ứng chậm → tăng 0.40 |
| `advanced.combat.death_detection.mp_drain_threshold_pct` | 1.0 | Repick quá sớm → tăng 1.5 |
| `combat.target_pick_interval_ms` | 4000 | Mob nhanh chết → giảm 3000 |
| `combat.sweep_r_min/max` | 50–200 | Hit rate thấp → adjust theo class |
| `humanizer.jitter_sigma_ms` | 45 | Quá đều → tăng 60 |

## Document Output

Tạo `docs/v1-release-notes.md`:
- Thông số tune cuối
- Known limitations (PostMessage scope, foreground, anti-cheat awareness)
- Hướng dẫn user calibrate + first run
- Troubleshooting common issues

## Risks & Stop Conditions

DỪNG soak ngay nếu:
- Tool crash → fix root cause trước khi retry
- Char chết liên tục → P0 logic lỗi → debug
- Tool spam pot không kiểm soát → confirm logic lỗi
- Log có ERROR rate > 1/phút → có issue nghiêm trọng

## Acceptance
- [ ] Stage 1–5 all pass
- [ ] 8-hour soak: no crash, < 10MB RSS drift
- [ ] HP detection MAE < 3% sau tune
- [ ] Mob hit rate > 50% trong farm scene phù hợp
- [ ] Documentation v1 release notes hoàn thành
- [ ] Tool sẵn sàng cho user real use case (tài khoản phụ trên server official)

## Next (Out of Scope cho MVP)
- Server official testing với tài khoản phụ (user tự làm, document risk XC3)
- v2 features: Template matching mob, multi-resolution profile, code signing, etc.
