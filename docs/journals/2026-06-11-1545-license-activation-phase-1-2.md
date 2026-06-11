# License Activation System: Phases 1-2 Complete

**Date**: 2026-06-11 15:45  
**Severity**: Medium (cross-repo coordination, rate limit + race condition fixes)  
**Component**: License activation (C++ client + Node backend)  
**Status**: Resolved (phases 3-6 pending)

## What Happened

Implemented full license activation pipeline across two repositories in a single cook session using parallel fullstack-developer agents:
- **window_helper (C++)**: HWID collector + ImGui activation dialog (commit ae0f9cb)
- **bot_discord_app (Node)**: SQLite schema + activation/verification endpoints + admin dashboard (commit 1416324)

Tester ran 11 curl tests + HWID stability check — all PASS. Code reviewer approved with 2 HIGH + 3 MEDIUM fixes applied.

## The Brutal Truth

Cross-repo orchestration felt brittle initially. Plans live in window_helper, but bot_discord_app source needed absolute paths (D:/Vietnt/Project/...) in phase docs for agents to edit the right files. Risk: path typos silently edit wrong locations. We dodged this through explicit reviewer scrutiny, not process safeguards. Need to bake this into templates.

Rate limiting + first-bind race condition slipped through initial design. Both caught during code review—felt like close calls that shouldn't have gotten to review. Tester's curl harness proved its worth here.

## Technical Details

**Phase 1 (C++ client):**
- HWID = SHA-256(VolumeSerial + cpuid + MachineGuid + MAC)
- Generated via bcrypt CNG, cached in mutex-protected singleton
- ImGui dialog matches mockup; build clean (bcrypt + iphlpapi linked)

**Phase 2 (Node backend):**
- Dual SQLite schema: `licenses` (machine_id_short, expires_at, created_at) + `license_events` (activation/verification audit)
- db-licenses.js: atomic conditional bindMachine via UPDATE...WHERE
- Ed25519 signature verification (tweetnacl)
- Rate-limited public endpoints: `/api/license/activate` + `/api/license/verify`
- Admin gated routes: `/api/admin/licenses` (JWT required)
- Monorepo workspace setup lets bot/ import shared/db-licenses.js directly

**Fixes Applied:**
1. **IP spoof bypass (HIGH)**: Rate limit now trusts proxy headers explicitly + validates IP range
2. **First-bind race (HIGH)**: activate + verify now idempotent; second request returns existing license_event
3. **Expires_at validation (MEDIUM)**: Reject activate if already expired at issue time
4. **machine_id_short cross-check (MEDIUM)**: Re-verify during verify endpoint call
5. **Dedupe helper (MEDIUM)**: Extracted license_event lookup to shared utility

## What We Tried

Initial design skipped IP validation in rate limiting (assumed reverse proxy trustworthy). Tester's curl tests caught this immediately—simple oversight, expensive lesson.

First-bind activate endpoint originally returned success without idempotency guard. Retry during user's ImGui dialog caused duplicate entries. Fixed with UPDATE...WHERE machine_id IS NULL before insert.

## Root Cause Analysis

**Cross-repo path issue:** No standard for absolute vs. relative paths in multi-repo plans. We improvised per-agent, creating a coordination tax.

**Rate limit + race condition:** Pressure to parallelize agent work meant less time for end-to-end scenario review. Phase 2 agent focused on endpoint isolation, not cross-endpoint timing guarantees.

**Code reviewer as safety net:** Small MEDIUM fixes were acceptable precisely because reviewer ran the suite end-to-end. Early design review (pre-implementation) would have caught these cheaper.

## Lessons Learned

1. **Plan templates for monorepos:** Document absolute path convention upfront. Add validation script: "phase docs reference files that exist."

2. **Rate limit + idempotency checklist:** Any endpoint accepting user-triggered retries needs both safeguards. Review these in pairs, not separately.

3. **Tester validation scope:** 11 curl tests + HWID stability check proved lightweight but effective. Worth adding similar quick-hit suites for phases 3-6 early (don't wait until final integration).

4. **Pre-binding machine_id design:** Admin issuing license with machine_id_short from Discord /license command before user activates is clean—gating happens at point of license creation, not activation verification. Keeps Discord bot authority clear.

5. **Monorepo workspace imports:** bot_discord_app's ability to `require('../shared/db-licenses.js')` eliminates internal HTTP marshalling. Architectural win for tight binding between bot commands and license queries.

## Next Steps

- **Phases 3-6 pending:** WinHTTP client (phase 3), grace 48h + periodic verify (phase 4), main.cpp integration (phase 5), Discord bot /license slash commands (phase 6)
- **Production server URL:** Hardcode into C++ client—currently unresolved (blocking phase 3)
- **Path validation script:** Before next phase, add check: "referenced files exist at documented paths"
- **Discord bot command design:** Pre-bind machine_id in /issue-license command before ImGui activation dialog on client side (reduces activation failure surface)

**Unresolved Q:** Production server URL for C++ client hardcode?
