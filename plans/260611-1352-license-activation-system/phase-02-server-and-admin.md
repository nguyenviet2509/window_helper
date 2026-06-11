---
phase: 2
title: Server Endpoints + DB Schema + Admin UI Tab (bot_discord_app)
status: completed
priority: high
effort: 1d
depends_on: []
---

# Phase 2 — Server Endpoints + Admin UI

## Context Links
- Brainstorm: [../reports/brainstorm-260611-1352-license-activation-system.md](../reports/brainstorm-260611-1352-license-activation-system.md)
- Repo: `D:/Vietnt/Project/bot_discord_app/`
- Key files: `dashboard/server.js`, `shared/db.js`, `dashboard/routes/managed-bots.js` (reference pattern), `dashboard/public/index.html`, `dashboard/public/js/app.js`

## Overview
Thêm license CRUD vào dashboard hiện có. SQLite tables vào `shared/db.js initDb()`. Public endpoints (`/api/license/activate|verify`) mount TRƯỚC auth chain. Admin endpoints sau auth. Alpine tab mới.

## Key Insights
- Project dùng `shared/db.js` cho schema (no migration files) — inline `CREATE TABLE IF NOT EXISTS`
- Pattern feature: `shared/db-{feature}.js` + `dashboard/routes/{feature}.js` (xem `managed-bots`)
- `server.js` hiện mount tất cả `/api/*` SAU `auth` → cần thêm 2 mount BEFORE auth cho public license routes
- JWT secret = `DASHBOARD_SECRET` env (đã có)
- `tweetnacl` pure JS, không cần native build — tránh xung đột với better-sqlite3 native

## Requirements
**Functional:**
- Admin CRUD: list, create, edit note/expiry, revoke, reset machine, view events
- Public: activate (bind machine first time, reject mismatch), verify (signed payload)
- Log mọi event vào `license_events`

**Non-functional:**
- Response <100ms
- `LICENSE_ED25519_PRIVATE_KEY` chỉ trong env, không log
- Rate limit `/api/license/*` 10 req/min/IP

## Files to Create
- `D:/Vietnt/Project/bot_discord_app/shared/db-licenses.js` — CRUD: `createToken`, `findByToken`, `bindMachine`, `revoke`, `resetMachine`, `recordEvent`, `list`, `update`, `listEvents` (~150 LOC)
- `D:/Vietnt/Project/bot_discord_app/shared/license-crypto.js` — `signPayload(obj) → base64`, key loaded from env (~50 LOC)
- `D:/Vietnt/Project/bot_discord_app/dashboard/routes/license-public.js` — `/activate` + `/verify` (~120 LOC)
- `D:/Vietnt/Project/bot_discord_app/dashboard/routes/licenses-admin.js` — admin CRUD (~150 LOC)
- `D:/Vietnt/Project/bot_discord_app/dashboard/public/js/licenses.js` — Alpine `licensesSection` component
- `D:/Vietnt/Project/bot_discord_app/tools/license-keygen.js` — one-off: `nacl.sign.keyPair()` → print env + C++ header hex array

## Files to Modify
- `D:/Vietnt/Project/bot_discord_app/shared/db.js` — append 2 `CREATE TABLE` vào `initDb()`
- `D:/Vietnt/Project/bot_discord_app/dashboard/server.js` — mount routes + rate limit middleware
- `D:/Vietnt/Project/bot_discord_app/dashboard/package.json` — `tweetnacl`, `express-rate-limit`
- `D:/Vietnt/Project/bot_discord_app/dashboard/public/index.html` — thêm nav item + section
- `D:/Vietnt/Project/bot_discord_app/.env.example` (or .env) — thêm `LICENSE_ED25519_PRIVATE_KEY`, `LICENSE_GRACE_HOURS`

## DB Schema (append to `shared/db.js initDb()`)
```sql
CREATE TABLE IF NOT EXISTS licenses (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  token TEXT UNIQUE NOT NULL,
  machine_id TEXT,                    -- pre-bound via /license issue, hoặc bind khi activate
  machine_id_short TEXT,
  user_label TEXT,
  discord_user_id TEXT,               -- recipient (Discord snowflake)
  issued_by_discord_id TEXT,          -- admin who ran /license issue
  created_at INTEGER DEFAULT (unixepoch()),
  activated_at INTEGER,
  expires_at INTEGER,
  revoked INTEGER NOT NULL DEFAULT 0,
  last_seen INTEGER,
  last_ip TEXT,
  app_version TEXT,
  note TEXT
);
CREATE INDEX IF NOT EXISTS idx_licenses_discord_user ON licenses(discord_user_id);
CREATE INDEX IF NOT EXISTS idx_licenses_token ON licenses(token);

CREATE TABLE IF NOT EXISTS license_events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  license_id INTEGER,
  type TEXT NOT NULL,
  ip TEXT, ua TEXT,
  ts INTEGER DEFAULT (unixepoch()),
  meta_json TEXT,
  FOREIGN KEY(license_id) REFERENCES licenses(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_events_license ON license_events(license_id, ts);
```

## API Spec
```
POST /api/license/activate        [PUBLIC + rate-limit]
  body: { token, machine_id, machine_id_short, app_version }
  200: { ok:true, expires_at, grace_hours, signed (base64), payload }
  400 INVALID_INPUT | 404 INVALID_TOKEN | 409 MACHINE_MISMATCH | 410 REVOKED | 410 EXPIRED

POST /api/license/verify          [PUBLIC + rate-limit]
  body: { token, machine_id, app_version? }
  same response shape

GET    /api/admin/licenses                       [JWT]
POST   /api/admin/licenses                       [JWT]  body: { user_label, expires_at?, note? }
PATCH  /api/admin/licenses/:id                   [JWT]  body: { user_label?, expires_at?, note? }
POST   /api/admin/licenses/:id/revoke            [JWT]
POST   /api/admin/licenses/:id/reset-machine     [JWT]
GET    /api/admin/licenses/:id/events            [JWT]
```

Canonical payload bytes (chuỗi để ký + verify ở client):
`token_hash|machine_id|expires_at|issued_at`
- `token_hash` = SHA-256(token) hex (không gửi token plaintext qua signed payload)

## Implementation Steps

### 2.1 Dependencies
```bash
cd D:/Vietnt/Project/bot_discord_app/dashboard
npm install tweetnacl express-rate-limit
```

### 2.2 Generate Ed25519 key
```bash
node D:/Vietnt/Project/bot_discord_app/tools/license-keygen.js
# Output:
#   LICENSE_ED25519_PRIVATE_KEY=<base64>
#   Public key (C++ header):
#     constexpr unsigned char kServerPubKey[32] = { 0xAB, 0xCD, ... };
```
- Copy private key → `.env`
- Copy C++ header → save cho phase 3 (paste vào `src/license/server-public-key.h`)

### 2.3 Schema
- Append 2 `CREATE TABLE` blocks vào `shared/db.js` `initDb()` (chạy 1 lần khi server start)

### 2.4 `shared/db-licenses.js`
```js
const { getDb } = require('./db')
const crypto = require('crypto')

function createToken({ user_label, expires_at, note, machine_id, machine_id_short, discord_user_id, issued_by_discord_id }) {
  const token = crypto.randomBytes(16).toString('hex')  // 32 hex chars
  const info = getDb().prepare(
    `INSERT INTO licenses (token, user_label, expires_at, note, machine_id, machine_id_short, discord_user_id, issued_by_discord_id)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
  ).run(token, user_label || null, expires_at || null, note || null,
        machine_id || null, machine_id_short || null,
        discord_user_id || null, issued_by_discord_id || null)
  return { id: info.lastInsertRowid, token }
}

// Khi /license issue pre-bind machine_id, activate request phải khớp.
// Khi createToken không pre-bind, lần activate đầu tự bind.

function findByToken(token) { ... }
function bindMachine(id, machine_id, machine_id_short, ip, ua, app_version) { ... }
function recordEvent(license_id, type, ip, ua, meta) { ... }
// ... etc
```

### 2.5 `shared/license-crypto.js`
```js
const nacl = require('tweetnacl')
const sk = Buffer.from(process.env.LICENSE_ED25519_PRIVATE_KEY, 'base64')
if (sk.length !== 64) throw new Error('LICENSE_ED25519_PRIVATE_KEY invalid')

function sign(canonicalMsg) {
  const sig = nacl.sign.detached(Buffer.from(canonicalMsg, 'utf8'), sk)
  return Buffer.from(sig).toString('base64')
}
function buildCanonical({ token, machine_id, expires_at, issued_at }) {
  const tokenHash = crypto.createHash('sha256').update(token).digest('hex')
  return `${tokenHash}|${machine_id}|${expires_at || 0}|${issued_at}`
}
module.exports = { sign, buildCanonical }
```

### 2.6 `dashboard/routes/license-public.js`
- `/activate`: validate body → findByToken → check revoked/expired → if machine_id null bind + event `activate` → if mismatch event `reject` + 409 → build canonical + sign → return
- `/verify`: similar, không bind mới, mismatch/revoked/expired = error
- Update `last_seen`, `last_ip`, `app_version` mỗi lần

### 2.7 `dashboard/routes/licenses-admin.js`
- GET `/`: list (return mask token: first4...last4)
- POST `/`: createToken, return PLAINTEXT token 1 lần
- PATCH `/:id`: update label/expires/note
- POST `/:id/revoke`: set revoked=1, event
- POST `/:id/reset-machine`: clear machine_id, event
- GET `/:id/events`: last 50 events

### 2.8 `server.js` mounting
```js
const rateLimit = require('express-rate-limit')
const licenseLimiter = rateLimit({ windowMs: 60_000, max: 10 })

// PUBLIC — mount TRƯỚC auth routes
app.use('/api/license', licenseLimiter, require('./routes/license-public'))

// ADMIN — sau auth
app.use('/api/admin/licenses', auth, require('./routes/licenses-admin'))
```

### 2.9 Frontend tab
**`dashboard/public/index.html`:**
```html
<!-- sidebar, sau managed-bots: -->
<div @click="tab='licenses'; sidebarOpen=false"
     :class="tab==='licenses' ? 'active' : ''" class="nav-item">
  🔐 Licenses
</div>

<!-- main, sau các section khác: -->
<div x-show="tab === 'licenses'" x-data="licensesSection" x-init="init()">
  <!-- header + create button + table + create modal + events drawer -->
</div>

<!-- before </body>: -->
<script src="/js/licenses.js"></script>
```

**`dashboard/public/js/licenses.js`:** Alpine component theo mẫu các file khác — `init()` fetch list, `createToken()`, `revoke(id)`, `resetMachine(id)`, `editNote(id)`, `viewEvents(id)`.

Quan trọng: response `create` chứa `token` plaintext → hiện trong modal với nút Copy + warning "chỉ hiện 1 lần".

### 2.10 Testing
```bash
# 1. Server starts
npm run dashboard

# 2. Admin login + create token (via UI hoặc curl)
TOKEN=$(curl -X POST localhost:3001/api/auth/login -H 'content-type: application/json' \
  -d '{"username":"admin","password":"..."}' | jq -r .token)

curl -X POST localhost:3001/api/admin/licenses -H "Authorization: Bearer $TOKEN" \
  -H 'content-type: application/json' \
  -d '{"user_label":"test user","note":"first test"}'
# → { id, token: "abc..." }

# 3. Activate (no auth)
curl -X POST localhost:3001/api/license/activate -H 'content-type: application/json' \
  -d '{"token":"abc...","machine_id":"ffff...","machine_id_short":"12345678","app_version":"1.0"}'
# → 200 ok, signed payload

# 4. Activate khác machine_id
# → 409 MACHINE_MISMATCH

# 5. Verify
curl ... /api/license/verify ... → 200 ok

# 6. Admin revoke → verify → 410 REVOKED
# 7. Admin reset-machine → activate machine mới → 200
```

## Todo
- [x] `npm install tweetnacl express-rate-limit` trong dashboard
- [x] Tạo `tools/license-keygen.js`, generate keypair, lưu private vào `.env`, header copy ra cho phase 3
- [x] Append schema vào `shared/db.js initDb()`, restart → table tạo OK
- [x] `shared/db-licenses.js` + manual test trong node REPL
- [x] `shared/license-crypto.js` + roundtrip test (sign → verify với tweetnacl)
- [x] `routes/license-public.js` + mount với rate-limit
- [x] `routes/licenses-admin.js` + mount sau auth
- [x] curl tests toàn bộ flow (10 steps trên)
- [x] Alpine tab UI: list/create/edit/revoke/reset/events
- [x] Test UI: create → copy token → activate sim → revoke → verify → status update

## Success Criteria
- Create token qua UI → token hiện trong table (mask)
- Plaintext token chỉ hiện 1 lần trong create modal
- Activate cùng token 2 máy khác nhau → máy 2 nhận 409 + event log
- Revoke → verify trả 410
- Reset-machine → activate máy mới OK
- Events drawer log đầy đủ activate/verify/reject
- Rate limit ngăn brute (curl 20 lần /activate → 429 sau req 10)

## Risks
- Token plaintext không lưu lại được sau create → user phải copy ngay; mitigate bằng modal rõ ràng
- Private key leak → app bypass-able → strict env-only, gitignore .env
- Rate limit có thể chặn user thật khi mạng retry → 10/min đủ thoải mái

## Security
- `LICENSE_ED25519_PRIVATE_KEY` env-only, never logged
- Token plaintext: hiện 1 lần khi tạo, sau đó luôn mask
- Rate limit chống brute
- JWT bắt buộc cho admin routes (reuse middleware có sẵn)
- Public routes whitelist body fields, validate string length

## Completion Notes
**Date:** 2026-06-11
**Implementation Report:** [fullstack-260611-1509-phase02-implementation.md](../reports/fullstack-260611-1509-phase02-implementation.md)
**Test Report:** [tester-260611-1523-phase1-2-validation.md](../reports/tester-260611-1523-phase1-2-validation.md) — 11/11 curl tests pass
**Code Review:** [code-review-260611-1509-phase1-2.md](../reports/code-review-260611-1509-phase1-2.md) — APPROVE_WITH_CHANGES 7.5/10
**Review Fixes Applied:** [fullstack-260611-1530-review-fixes.md](../reports/fullstack-260611-1530-review-fixes.md) — trust proxy IP, atomic bind, expires_at validation, dedupe, machine_id_short cross-check
**Deviations:** None. All endpoints pass curl tests. Admin UI functional. DB schema clean. Issues from code review addressed.

## Next Steps
→ Phase 3 (client): copy public key header sang `src/license/server-public-key.h`, wire WinHTTP
