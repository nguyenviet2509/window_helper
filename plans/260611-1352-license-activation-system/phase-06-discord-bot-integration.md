---
phase: 6
title: Discord Bot Integration (/license slash commands + DM delivery + audit log)
status: pending
priority: high
effort: 0.5d
depends_on: [phase-02]
---

# Phase 6 — Discord Bot Integration

## Context Links
- Brainstorm: [../reports/brainstorm-260611-1352-license-activation-system.md](../reports/brainstorm-260611-1352-license-activation-system.md)
- Bot dir: `D:/Vietnt/Project/bot_discord_app/bot/`
- Shared DB: `D:/Vietnt/Project/bot_discord_app/shared/db-licenses.js` (phase 2)

## Overview
Slash command `/license` cho admin issue/revoke/reset/info/list. Bot DM token cho user. Audit channel embed log mọi event.

## Key Insights
- Bot monorepo workspace → import trực tiếp `shared/db-licenses.js` (no HTTP call cần)
- User cần copy FULL HWID (64 hex) từ activation dialog → paste vào Discord cho admin
- Pre-bind machine_id lúc issue chặn share ngay từ đầu
- Audit channel embed giúp detect abuse pattern (vd nhiều reject event = ai đó thử share token)

## Requirements
**Functional:**
- `/license issue user machine_id label [expires_days] [note]` — tạo + pre-bind + DM user + log
- `/license revoke target` (target = token mask hoặc @user) — revoke + DM user thông báo + log
- `/license reset-machine target` — clear machine_id (user đổi PC) + DM user + log
- `/license info user` — show status (token mask, machine, expires, last_seen)
- `/license list [user]` — list tokens (mask only)
- Audit channel embed cho: issue, revoke, reset, activate (success), reject (mismatch), expired use attempt

**Non-functional:**
- Admin-only (env `LICENSE_ADMIN_ROLE_ID` hoặc Discord Administrator perm)
- DM fallback: nếu user khóa DM, reply ephemeral cho admin với token để forward thủ công
- Audit log không leak token plaintext (chỉ mask)

## Files to Create
- `D:/Vietnt/Project/bot_discord_app/bot/commands/license.js` — slash command handler với subcommands (~250 LOC)
- `D:/Vietnt/Project/bot_discord_app/shared/license-notifier.js` — helpers `sendTokenDM(client, userId, token, payload)`, `logAuditEvent(client, channelId, event)` (~120 LOC)

## Files to Modify
- `D:/Vietnt/Project/bot_discord_app/bot/deploy-commands.js` — register `/license` command
- `D:/Vietnt/Project/bot_discord_app/dashboard/routes/license-public.js` (từ phase 2) — sau khi activate/reject/revoke event recorded, fire audit log qua notifier (cần share bot client instance, hoặc emit event qua simple event bus / write to events table và bot poll)
- `D:/Vietnt/Project/bot_discord_app/.env.example` — thêm `LICENSE_ADMIN_ROLE_ID`, `LICENSE_AUDIT_CHANNEL_ID`

## Audit Wiring Decision (đơn giản)
Public routes (server-side) không có Discord client. 2 lựa chọn:
- **A. Event polling**: Routes ghi event vào `license_events` table (đã có từ phase 2). Bot có 1 background tick mỗi 5s scan event mới (`WHERE id > last_seen_id`), post lên audit channel. Simple, decoupled.
- **B. In-process emit**: Bot và dashboard chung process? Check `start.js`. Nếu có → dùng EventEmitter chung. Nếu separate process → A bắt buộc.

→ **Default: A (polling)** — KISS, không cần thay đổi server.js.

## Slash Command Spec

```
/license issue
  user: User (required)
  machine_id: string 64-hex (required) — full HWID, paste full từ app
  label: string (required) — vd "PT-acc-1"
  expires_days: integer (optional) — null = vĩnh viễn
  note: string (optional)

/license revoke
  token_mask_or_user: string — first/last 4 hex hoặc @user mention
  reason: string (optional)

/license reset-machine
  token_mask_or_user: string

/license info
  user: User

/license list
  user: User (optional, filter)
```

## Implementation Steps

### 6.1 Slash command registration
- Extend `deploy-commands.js` builder với `SlashCommandBuilder` + subcommands
- Run `npm run deploy:commands` để push lên Discord API

### 6.2 `bot/commands/license.js` skeleton
```js
const { SlashCommandBuilder, PermissionFlagsBits, EmbedBuilder } = require('discord.js')
const db = require('../../shared/db-licenses')
const notifier = require('../../shared/license-notifier')

module.exports = {
  data: new SlashCommandBuilder()
    .setName('license')
    .setDescription('WindowHelper license management')
    .setDefaultMemberPermissions(PermissionFlagsBits.Administrator)
    .addSubcommand(sub => sub.setName('issue')
      .setDescription('Issue token bound to user machine')
      .addUserOption(o => o.setName('user').setRequired(true))
      .addStringOption(o => o.setName('machine_id').setRequired(true))
      .addStringOption(o => o.setName('label').setRequired(true))
      .addIntegerOption(o => o.setName('expires_days'))
      .addStringOption(o => o.setName('note'))
    )
    // ... revoke, reset_machine, info, list
  ,
  async execute(interaction) {
    const sub = interaction.options.getSubcommand()
    if (sub === 'issue') return handleIssue(interaction)
    // ...
  }
}
```

### 6.3 handleIssue logic
```js
async function handleIssue(interaction) {
  await interaction.deferReply({ ephemeral: true })
  const user = interaction.options.getUser('user')
  const machineId = interaction.options.getString('machine_id').trim().toLowerCase()
  const label = interaction.options.getString('label')
  const days = interaction.options.getInteger('expires_days')
  const note = interaction.options.getString('note')

  // Validate
  if (!/^[a-f0-9]{64}$/.test(machineId)) {
    return interaction.editReply('❌ Machine ID phải là 64 hex chars')
  }
  const expires_at = days ? Math.floor(Date.now()/1000) + days*86400 : null
  const machine_id_short = machineId.slice(0, 8)

  // Create token
  const { id, token } = db.createToken({
    user_label: label,
    expires_at, note,
    machine_id: machineId,
    machine_id_short,
    discord_user_id: user.id,
    issued_by_discord_id: interaction.user.id,
  })
  db.recordEvent(id, 'issue', null, null, JSON.stringify({ by: interaction.user.id }))

  // DM user
  const dmResult = await notifier.sendTokenDM(interaction.client, user.id, {
    token, machine_id_short, expires_at, label
  })

  // Reply admin
  if (dmResult.ok) {
    await interaction.editReply(`✅ Issued to <@${user.id}>. Token mask: \`${token.slice(0,4)}****${token.slice(-4)}\``)
  } else {
    await interaction.editReply(
      `⚠️ Token issued nhưng DM fail (user khóa DM?). Token plaintext:\n\`\`\`${token}\`\`\`\nGửi tay cho user.`
    )
  }

  // Audit log
  await notifier.logAuditEvent(interaction.client, process.env.LICENSE_AUDIT_CHANNEL_ID, {
    type: 'issue', color: 0x22c55e,
    user_id: user.id, by: interaction.user.id,
    machine_short: machine_id_short, label, expires_at, dm_ok: dmResult.ok
  })
}
```

### 6.4 `shared/license-notifier.js`
```js
const { EmbedBuilder } = require('discord.js')

async function sendTokenDM(client, userId, { token, machine_id_short, expires_at, label }) {
  try {
    const user = await client.users.fetch(userId)
    const expStr = expires_at ? new Date(expires_at*1000).toLocaleDateString('vi-VN') : 'Vĩnh viễn'
    const embed = new EmbedBuilder()
      .setColor(0x6366f1)
      .setTitle('🔐 WindowHelper License')
      .setDescription(
        `Mã kích hoạt:\n\`\`\`${token}\`\`\`\n\n` +
        `📋 **Cách dùng:**\n1. Mở WindowHelper.exe\n2. Nhập mã vào ô Enter code\n3. Bấm Activate`
      )
      .addFields(
        { name: 'Label', value: label, inline: true },
        { name: 'Machine ID', value: `\`${machine_id_short}\``, inline: true },
        { name: 'Hết hạn', value: expStr, inline: true },
      )
      .setFooter({ text: '⚠️ Không chia sẻ. Mã chỉ dùng được trên máy có Machine ID khớp.' })
    await user.send({ embeds: [embed] })
    return { ok: true }
  } catch (err) {
    return { ok: false, error: err.message }
  }
}

async function logAuditEvent(client, channelId, event) {
  if (!channelId) return
  try {
    const ch = await client.channels.fetch(channelId)
    const colorMap = { issue: 0x22c55e, activate: 0x22c55e, revoke: 0xeab308, reset: 0xeab308, reject: 0xef4444, expired: 0xef4444 }
    const embed = new EmbedBuilder()
      .setColor(colorMap[event.type] || 0x64748b)
      .setTitle(`📜 License ${event.type}`)
      .addFields(/* dynamic fields based on event.type */)
      .setTimestamp()
    await ch.send({ embeds: [embed] })
  } catch (_) { /* swallow */ }
}

module.exports = { sendTokenDM, logAuditEvent }
```

### 6.5 Event polling for server-originated events
- Bot startup: spawn `setInterval(pollLicenseEvents, 5000)`
- Track `lastEventId` in-memory (or table `meta`)
- Query: `SELECT * FROM license_events WHERE id > ? ORDER BY id LIMIT 50`
- For each event with type in `[activate, reject, expired-attempt]`, post audit embed

### 6.6 handleRevoke / handleResetMachine / handleInfo / handleList
- Similar structure: parse args → db method → DM user (cho revoke/reset) → audit log → reply admin
- `handleList`: query `WHERE discord_user_id = ?` if filter, return table format

### 6.7 Env vars
```
LICENSE_AUDIT_CHANNEL_ID=123456789012345678
# LICENSE_ADMIN_ROLE_ID optional — default dùng Administrator perm
```

## Todo
- [ ] `commands/license.js` skeleton + register
- [ ] `shared/license-notifier.js` sendTokenDM + logAuditEvent
- [ ] handleIssue end-to-end test (DM nhận được, log channel có embed)
- [ ] handleRevoke + DM "Token revoked, contact admin"
- [ ] handleResetMachine + DM "Machine reset, re-activate"
- [ ] handleInfo + handleList
- [ ] Event poller — activate event từ server → audit embed
- [ ] DM fallback path: test với user khóa DM
- [ ] Permission test: non-admin chạy → ephemeral "Bạn không có quyền"
- [ ] Deploy commands → Discord shows in slash menu

## Success Criteria
- Admin `/license issue` → user nhận DM trong <2s
- User paste token vào app → activate thành công (machine match)
- User paste token máy khác → 409 reject, audit channel có embed đỏ
- Admin `/license revoke` → user nhận DM + verify tiếp theo fail
- DM fail → admin nhận ephemeral với token plaintext + warning
- Audit channel có embed cho mọi event quan trọng

## Risks
- DM bị Discord rate limit nếu issue dồn dập → acceptable, scale nhỏ
- Bot down → admin không cấp được token mới; fallback: dashboard manual create vẫn hoạt động
- Event poller miss event nếu bot crash giữa chừng → track lastEventId in DB table `meta(key, value)`
- Token plaintext xuất hiện trong ephemeral admin reply khi DM fail → chấp nhận, đã ephemeral

## Security
- Admin role check via `setDefaultMemberPermissions(Administrator)` ở slash command builder
- Token KHÔNG bao giờ log vào console
- Audit embeds chỉ chứa token mask
- `LICENSE_ADMIN_ROLE_ID` env nếu muốn dùng custom role thay Administrator

## Next Steps
- Sau phase 6: full system ready. Test end-to-end với user thật.
- Future enhancement: `/license bulk-issue` từ CSV, expire-soon notification (cron DM "token hết hạn trong 3 ngày")
