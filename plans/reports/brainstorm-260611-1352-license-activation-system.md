---
type: brainstorm
date: 2026-06-11
slug: license-activation-system
status: approved
---

# License / Activation System — Brainstorm Summary

## Problem
WindowHelper bị share auto linh tinh. Cần gate activation: chỉ user được cấp token mới chạy được, mỗi máy 1 token, quản lý tập trung từ dashboard hiện có.

## Goals
- Token bind 1-1 với máy (HWID)
- Admin quản lý từ dashboard Node/Express hiện tại (tạo, revoke, reset machine, set expiry)
- UX activation giống mockup (modal: Machine ID + Enter code + Activate/Exit)
- Grace offline 48h
- Anti-share "đủ dùng" (không phải DRM enterprise)

## Non-goals
- DRM chống reverse-engineer chuyên nghiệp
- Multi-tier license, trial, payment flow
- Auto-renew, billing
- Heartbeat realtime

---

## Final Architecture

```
[WindowHelper.exe C++]  --HTTPS+Ed25519--> [dashboard/server.js]  --> [SQLite/DB hiện có]
   ImGui modal gate                           Express routes
   WinHTTP client                             Alpine.js admin tab
   AES-GCM cache (key=HKDF(HWID))             license_events audit
```

### Client modules (mới)
- `src/license/hwid-collector.{h,cpp}` — VolumeSerial(C:) + cpuid + MachineGuid + MAC → SHA-256
- `src/license/license-client.{h,cpp}` — WinHTTP POST JSON, verify Ed25519
- `src/license/license-cache.{h,cpp}` — AES-256-GCM encrypt/decrypt %APPDATA%\WindowHelper\license.dat
- `src/license/license-manager.{h,cpp}` — orchestration, grace period, periodic re-verify
- `src/ui/activation-dialog.{h,cpp}` — ImGui modal trước main UI

### Server additions (vào `dashboard/`)
- Routes: `POST /api/license/activate`, `POST /api/license/verify`, `GET/POST /api/admin/licenses`, `POST /api/admin/licenses/:id/revoke|reset-machine`
- Table `licenses` + `license_events`
- Alpine.js tab "Licenses" trong `index.html` (table + create modal)

---

## Key Design Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Activation mode | Online (server verify) | User chọn. Cho phép revoke realtime |
| HWID source | Mix VolumeSerial+cpuid+MachineGuid+MAC, SHA-256, display 8 hex | Khó fake, ổn định, format giống mockup |
| Machine binding | 1 token = 1 máy cứng, admin reset thủ công | Chống share triệt để nhất |
| Offline policy | Grace 48h sau lần verify cuối | Cân bằng UX (mạng VN) vs revoke |
| Server URL | Hardcode + pinned Ed25519 public key | Chống MITM + fake server |
| Cache encryption | AES-GCM, key = HKDF(HWID + binary salt) | Copy file sang máy khác → key sai → reject |
| Verify cadence | Khi start + mỗi 6h khi đang chạy | Revoke lag tối đa 6h+grace |
| Stack server | Express thêm route, integrate dashboard hiện có | Reuse JWT admin auth, không tăng infra |
| Expiry | Admin set khi tạo (per-token) | User decision |
| Tier | 1 loại token | YAGNI |

---

## Data Model

```sql
licenses (
  id INTEGER PRIMARY KEY,
  token TEXT UNIQUE NOT NULL,          -- 32 hex chars
  machine_id TEXT NULL,                -- full SHA-256 hex
  machine_id_short TEXT NULL,          -- 8 hex hiển thị
  user_label TEXT,                     -- ai dùng (note nội bộ)
  created_at INTEGER,
  activated_at INTEGER NULL,
  expires_at INTEGER NULL,             -- NULL = vĩnh viễn
  revoked INTEGER DEFAULT 0,
  last_seen INTEGER NULL,
  last_ip TEXT NULL,
  app_version TEXT NULL,
  note TEXT
)

license_events (
  id INTEGER PRIMARY KEY,
  license_id INTEGER,
  type TEXT,                            -- activate|verify|reject|revoke|reset
  ip TEXT, ua TEXT, ts INTEGER,
  meta_json TEXT
)
```

---

## API Contract

**POST /api/license/activate**
```json
req:  { "token": "ab12...", "machine_id": "<sha256 hex>", "machine_id_short": "91262587", "app_version": "1.x" }
res:  { "ok": true, "expires_at": 1735689600, "grace_hours": 48,
        "signed": "<base64 ed25519 sig over canonical payload>",
        "payload": { "token_hash": "...", "machine_id": "...", "expires_at": ..., "issued_at": ... } }
err:  409 { "ok": false, "code": "MACHINE_MISMATCH" } | 404 INVALID_TOKEN | 410 REVOKED | 410 EXPIRED
```

**POST /api/license/verify** — body same as activate (no app_version optional), response same shape.

Client luôn verify `signed` bằng pinned public key trước khi trust payload.

---

## Client Flow

```
WinMain
  └─ LicenseManager::Bootstrap()
        ├─ HWID = Collect()
        ├─ cache = LoadEncryptedCache(HWID)
        ├─ if cache valid && now < cache.last_verified + 48h && now < cache.expires_at:
        │     spawn async Verify() (non-blocking)
        │     return OK → main UI
        ├─ else: show ActivationDialog (modal blocking)
        │     ├─ display Machine ID short
        │     ├─ on Activate click: POST /activate → verify sig → save cache → return OK
        │     ├─ on Exit: PostQuitMessage
        │     └─ on error: show message, stay in modal
  └─ Periodic: every 6h while running → Verify(); if revoked/expired → show toast + graceful shutdown
```

---

## Implementation Phases (estimate)

| # | Phase | Output | Time |
|---|---|---|---|
| 1 | HWID + skeleton + ImGui modal (no network) | hwid-collector, activation-dialog stub | 0.5d |
| 2 | Server routes + SQLite + admin tab | dashboard endpoints + UI | 1d |
| 3 | WinHTTP client + Ed25519 verify + AES cache | end-to-end activate flow | 1d |
| 4 | Grace + periodic verify + revoke handling | full runtime gate | 0.5d |
| 5 | Integrate main.cpp gate + tray status + UX polish | shippable | 0.5d |

Total: ~3.5 day-equivalent.

---

## Risks & Mitigations

| Risk | Mitigation |
|---|---|
| Reverse-engineer skip gate | Accept — target casual share, not pro cracker. Ed25519 + HWID-bound cache đủ raise bar |
| HWID đổi khi user upgrade phần cứng | Admin "reset machine" button |
| Server downtime block users | Grace 48h offline |
| Token leak qua screenshot/chat | 1 token = 1 máy → kẻ thứ 2 activate → 409 reject, admin thấy event |
| MITM fake server local | Pinned public key Ed25519 verify response |
| Cache file copy sang máy khác | AES key derive từ HWID máy đó → giải mã fail |

---

## Dependencies
- C++: WinHTTP (Windows built-in), bcrypt (AES-GCM Windows CNG), libsodium or Monocypher cho Ed25519 (~30KB)
- Server: `better-sqlite3` (nếu dùng SQLite mới) + `tweetnacl` hoặc `libsodium-wrappers` cho ký Ed25519
- Build: thêm vcpkg port `libsodium` (hoặc embed Monocypher header-only)

---

## Success Criteria
- App từ chối khởi động khi không có token hợp lệ
- Token activate trên máy A → cố activate máy B → reject với event log rõ
- Admin revoke → user kế tiếp restart bị block trong ≤6h online / ≤48h offline
- Cache file copy chéo máy → giải mã fail, force re-activate
- Mất mạng 24h sau activate → app vẫn chạy
- Tampered server response → client reject (Ed25519 sig fail)

---

## Open / Deferred Questions
1. DB hiện tại của dashboard là gì (SQLite/Postgres/MySQL)? — user sẽ cung cấp context sau, ảnh hưởng schema migration
2. Có cần "soft block" (cho phép chạy demo mode đọc-only khi expired) hay hard exit? Mặc định: hard exit
3. Format token: 32 hex (16 bytes random) hay readable (XXXX-XXXX-XXXX-XXXX)? Mặc định: 32 hex, copy/paste dễ
4. Có log Machine ID short ra file `dist/README.txt` hoặc clipboard tự động để user dễ gửi không? Mặc định: hiện trong modal + nút Copy
