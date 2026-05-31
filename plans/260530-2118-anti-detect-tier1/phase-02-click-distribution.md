# Phase 02 — Click Distribution Gaussian

## Context Links
- Brainstorm §#2
- File: `src/combat/attack-sweep.h`

## Overview
- Priority: P1
- Status: pending
- Đổi annulus uniform → radius Gaussian; resample khi out-of-range.

## Key Insights
- ML detector flag uniform pattern; cluster Gaussian giống người thật.
- σ = (rMax-rMin)/3, mean = (rMax+rMin)/2 → 99% nằm trong [rMin,rMax].
- Angle giữ uniform [0,2π) (mob có thể ở mọi hướng).

## Requirements
**Functional**
- Replace `uniform_real_distribution<double> radius_` → `normal_distribution<double>`.
- Resample loop max 5 lần nếu out-of-range; fallback clamp ở lần cuối.

**Non-functional**
- File vẫn header-only, <60 LOC.
- API `pickAttackPosition(HWND)`, `setRange(rMin,rMax)` không đổi.

## Architecture
```
pickAttackPosition:
  a = uniform[0, 2π)
  for i in 0..4:
    r = normal(mean, sigma)
    if rMin <= r <= rMax: break
  else: r = clamp(r, rMin, rMax)
  return center + (cos(a)*r, sin(a)*r)
```

## Related Code Files
**Modify**
- `src/combat/attack-sweep.h`

## Implementation Steps
1. Đổi member `radius_` type → `std::normal_distribution<double>`.
2. Init với mean=(rMin+rMax)/2, sigma=(rMax-rMin)/6 (vì ±3σ cover full range → /6).
3. Update `setRange` rebuild distribution.
4. Trong `pickAttackPosition`: resample loop.
5. Build + check.

## Todo List
- [ ] Update distribution type + init
- [ ] Update setRange
- [ ] Add resample loop
- [ ] Build pass
- [ ] Sanity test: in 1000 sample radii, histogram peak ở mean

## Success Criteria
- Sample 1000 lần, histogram có peak rõ ở center, tail mỏng dần (không uniform).
- Tất cả sample nằm trong [rMin, rMax].
- No regression: tool vẫn đánh quái như cũ.

## Risk Assessment
| Risk | L | I | Mitigation |
|---|---|---|---|
| σ sai làm cluster quá hẹp → boring | L | L | Tune σ=(range)/6, có thể expose config sau |
| Resample loop vô hạn (σ huge) | L | M | Cap 5 iter + clamp fallback |

## Next Steps
- Phase 05 log click coord, vẽ scatter để verify cluster.
