# Input Backend Decision (Phase 0 Output)

Status: **PENDING PROBE** — run `PostMessageProbe.exe` against target PT client and fill this file.

## Probe Environment
- PT client version: _________________
- Server type (offline / private / official): _________________
- Window mode (windowed / fullscreen / borderless): _________________
- OS build: _________________
- Probe date: _________________

## Probe Results
| Test | Description | Result (PASS/FAIL) | Notes |
|------|-------------|--------------------|-------|
| T1 | `WM_KEYDOWN` F2 background | _____ | |
| T2 | `WM_RBUTTONDOWN` at (400,300) | _____ | |
| T3 | SHIFT + right-click stationary | _____ | |

## Backend Decision
> Fill exactly one of: `PostMessage` / `Hybrid` / `SendInput`.

**Chosen backend:** `__________`

### Rationale
- T1 keys: ____
- T2 mouse: ____
- T3 modifier: ____

### Decision Matrix
| T1 | T2 | T3 | Backend | Reason |
|----|----|----|---------|--------|
| ✓ | ✓ | ✓ | `PostMessage` | Best UX, user can multitask |
| ✓ | ✗ | * | `Hybrid` | PostMessage keys, SendInput mouse |
| ✗ | * | * | `SendInput` | Foreground only |

## Phase 2 Dependency
Phase 2 (`InputBackend` + `Humanizer` + `OutputGate`) reads this file to pick implementation.
