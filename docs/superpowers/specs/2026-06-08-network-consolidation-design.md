# Network Consolidation & Web UI Fixes — Design Spec
Date: 2026-06-08

## Summary

Consolidate three firmware builds into two, remove softAP dependency between
display and remote units, add static IP configuration to both builds, fix the
web UI SWR meter and antenna buttons to match the LVGL display, and switch to
deterministic mDNS hostnames.

---

## Architecture

### Builds

| Environment | Defines | Role |
|---|---|---|
| `esp32-display` | `WITH_DISPLAY`, `WITH_TOUCH` | Shack controller: LVGL + touch + web server. Standalone by default; optionally drives LVGL from a remote unit's tuner. |
| `esp32-remote` | *(none)* | Headless: full web server + tuner UART. Replaces both old `esp32-remote` and `esp32-nodisplay`. |

`esp32-nodisplay` is deleted. The `-DREMOTE_UNIT` flag is removed from
`esp32-remote`. All `#ifdef REMOTE_UNIT` blocks are removed from the codebase.

### Display ↔ Remote Communication

Both units connect to the infrastructure WiFi network. No AP is needed for
unit-to-unit communication.

**Display standalone mode** (default): display unit uses its own UART-connected
tuner. Full web server and LVGL work as before.

**Display + remote mode**: at boot the display unit attempts to connect to the
remote unit at the configured `remoteHost` (default `ldgcontrol-remote.local`).
If the connection succeeds, the remote is the tuner authority:
- Display LVGL is driven by the remote's telemetry via SSE subscription to
  remote's `/api/events`
- Display web server proxies `/api/command/*` calls to the remote's command API
- Display web server's `/api/status` returns the last-known remote state
- If the remote disconnects, display falls back to its local tuner automatically

The remote unit has no knowledge of the display unit and requires no changes to
support this — it just runs its web server normally.

### softAP

The captive-portal AP (`LDGConfig`) is still used for initial WiFi setup.
After STA join it is **always** disconnected regardless of build — the
`#ifndef WITH_DISPLAY` guard on `softAPdisconnect` is removed.

### mDNS

Both builds advertise a deterministic hostname:
- Display: `ldgcontrol-display.local`
- Remote: `ldgcontrol-remote.local`

The MAC-address-based hostname is removed.

---

## DeviceConfig Changes

New fields added to `struct DeviceConfig` (both builds):

```cpp
// Static IP
bool useStaticIP;
char staticIP[16];
char staticNetmask[16];
char staticGateway[16];
char staticDNS[16];
```

Display build only:
```cpp
char remoteHost[64];   // IP or hostname; default "ldgcontrol-remote.local"
```

All new fields are persisted via `Preferences` in NVS. `loadDefaults()` sets
`useStaticIP = false` and `remoteHost = "ldgcontrol-remote.local"`.

### Static IP Application

In `setupWiFi()`, before `WiFi.begin()`:
```cpp
if (m_config.useStaticIP && m_config.staticIP[0]) {
    IPAddress ip, mask, gw, dns;
    ip.fromString(m_config.staticIP);
    mask.fromString(m_config.staticNetmask);
    gw.fromString(m_config.staticGateway);
    dns.fromString(m_config.staticDNS);
    WiFi.config(ip, gw, mask, dns);
}
```

---

## Web Server API Changes

### `/api/config` GET

Add to response:
```json
{
  "useStaticIP": false,
  "staticIP": "192.168.1.100",
  "staticNetmask": "255.255.255.0",
  "staticGateway": "192.168.1.1",
  "staticDNS": "192.168.1.1",
  "currentIP": "192.168.1.55",
  "currentNetmask": "255.255.255.0",
  "currentGateway": "192.168.1.1",
  "currentDNS": "192.168.1.1",
  "remoteHost": "ldgcontrol-remote.local"
}
```

`current*` fields are always the live DHCP/static values from `WiFi.*`; they
are used to pre-fill the static IP form when `useStaticIP` is false.
`remoteHost` is only included in display builds.

### `/api/config` POST

Accepts the same fields for write. `remoteHost` write is only honoured in
display builds.

---

## Web UI Changes (data/index.html)

### Settings Tab — Network Section

Replaces the current read-only SSID/IP display:

```
Network
  SSID: [read-only]           IP Mode: [○ DHCP  ● Static]
  IP Address:  [________]     Netmask:  [________]
  Gateway:     [________]     DNS:      [________]

  (Display build only)
  Remote Unit Host: [ldgcontrol-remote.local___________]
```

- IP/Netmask/Gateway/DNS fields are disabled when DHCP is selected.
- On page load, fields are pre-filled from `currentIP` / `currentNetmask` /
  `currentGateway` / `currentDNS` so the user sees live values to copy if
  switching to static.
- Saved via existing `saveSettings()` → POST `/api/config`.

### Control Tab — Antenna Buttons

Remove the `Toggle Ant` button and the `Antenna: --` status row.

Replace with two buttons side-by-side:
- Left button: `ANT 1` (header) + configured `ant1Name` label (subtext)
- Right button: `ANT 2` (header) + configured `ant2Name` label (subtext)

State:
- Active antenna: full accent color (matches current active-button style)
- Inactive antenna: dimmed / secondary style

Behaviour:
- Clicking the **inactive** button sends `POST /api/command/toggle`
- Clicking the **active** button is a no-op (or visually does nothing)
- Active antenna is determined by `data.antenna` from `/api/status` SSE events
  (value `0` = ANT1, `1` = ANT2, matching `tuner_ant_t`)

### SWR Meter — Scale Fix

The meter has two pivots at bottom-right (forward) and bottom-left (reflected).
Each needle sweeps a **90° arc** — forward sweeps right→top, reflected sweeps
left→top. The current outer arc and scale ticks are drawn as one 180°
semicircle; they need to be two separate 90° arcs.

Fix:
- Remove the single `ctx.arc(cx, cy, radius, Math.PI, 2 * Math.PI)` outer arc
- Draw two arcs from each pivot point's perspective:
  - Forward arc: `φ = π` (left/zero) to `φ = 3π/2` (top/full-scale), counterclockwise — **same path the forward needle travels**
  - Reflected arc: `φ = 2π` (right/zero) to `φ = 3π/2` (top/full-scale), clockwise — **same path the reflected needle travels**
- Distribute scale ticks and labels (0 / 25 / 50 / 75 / 100%) along each arc
  independently
- Both arcs meet and cross at `φ = 3π/2` (top center = 100% full scale for
  both needles)
- Draw SWR curves within the area bounded by the two needle arcs (unchanged in
  logic, may need clipping adjustment)

---

## Code Removal

The following code is deleted entirely:

- `connectSSEClient()` in `main.cpp` (`#ifdef REMOTE_UNIT` block)
- `processSSEClient()` in `main.cpp` (`#ifdef REMOTE_UNIT` block)
- `postTelemetry()` in `main.cpp` (`#ifdef REMOTE_UNIT` block)
- `s_sseClient`, `s_sseBuffer`, `lastMeterPublish` variables (remote-only)
- AP-IP fallback `IPAddress(192, 168, 4, 1)` references
- `[env:esp32-nodisplay]` in `platformio.ini`

## Code Added (display build)

New functions in `main.cpp` under `#ifdef WITH_DISPLAY`:

- `connectRemoteUnit()` — resolves `remoteHost`, opens SSE to
  `/api/events`, called from `loop()` when remote is configured and not yet
  connected
- `processRemoteSSE()` — reads SSE lines from the remote, calls
  `onRemoteTelemetry()` / `onRemoteStatus()` (these already exist)
- Command proxy in web server: `h_cmd_*` handlers check `remoteUnitConnected`;
  if true, open a synchronous HTTPS client connection to `remoteHost:443` and
  forward the request (same pattern as the existing `postTelemetry()`), then
  relay the response status back to the web UI caller

---

## Testing Notes

- Static IP: configure static IP on device, reboot, verify it comes up on the
  configured address and `ldgcontrol-[build].local` resolves
- Remote mode: flash display + remote, confirm display LVGL shows remote
  telemetry, confirm web UI commands reach the tuner
- Standalone fallback: disconnect remote unit, confirm display falls back to
  local tuner within one reconnect cycle
- Antenna buttons: verify active antenna highlight matches `antenna` field in
  SSE events; verify toggle reaches tuner
- SWR meter: at 50% forward / 0% reflected, forward needle points to 10 o'clock
  position and the scale tick at that position reads 50%; reflected needle stays
  at 3 o'clock (zero)
