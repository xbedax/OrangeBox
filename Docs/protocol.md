# OrangeBox UI ↔ Box Protocol

**Status:** working draft / implementation inventory  
**Purpose:** describe the union of protocol behaviour found in the current OrangeBox firmware, current UI, and the Perl mock controller. The document intentionally preserves one-sided and incomplete features instead of hiding them.

## 1. Sources and authority

This draft is based on three implementation sources:

1. **ESP32 firmware** from the OrangeBox repository snapshot (`include/`, `src/`, `lib/`).
2. **Current UI snapshot** (`index.html`, `modal.js`, `clicktable.js`, `websoc.js`) supplied after the repository snapshot.
3. **Perl mock controller** (`websocket_server.pl`) used to develop the UI and to demonstrate intended box behaviour not yet implemented in the production firmware.

When these sources differ, the difference is documented explicitly. No source is silently treated as obsolete.

## 2. Terminology

### 2.1 Credential

A **credential** is a value that may authorize or identify an access/delivery action.

The current UI and mock controller already use a general credential-list concept with at least these types:

| Credential type | `credType` | Current concrete edit command |
|---|---|---|
| PIN | `ctPin` | `set_pin` |
| Parcel / order code | `ctCode` | `set_code` |

The production firmware is still primarily PIN-oriented (`get_pins`, `set_pin`, `PinRecord`, `pinStorage`, etc.). A planned code refactor is to rename functionality that is truly generic from **pin** to **credential**, while keeping PIN-specific fields and commands PIN-specific.

This documentation therefore uses **credential** for generic concepts and **PIN** / **code** for concrete credential types.
$$$  Celý firmware už by měl být agnostický. Pin a Code se používá jen tam, kde se jedná o specifický typ, včechny společné části používají cred $$$

### 2.2 HTML beacon / HTML key

An **HTML beacon** is an element ID addressable in the UI with `document.getElementById()`.

Several Box → UI commands operate generically on these element IDs rather than returning domain-specific response objects.

Examples currently used include:

- `door_state_open`
- `door_state_closed`
- `door_state_mixed` (firmware defines it; current UI does not yet present it)
- `ambient_state_on`
- `ambient_state_off`
- `door_controls`
- `pinrows`
- `coderows`
- `lastresult`
- diagnostic element IDs when present in a UI

$$$ Mělo by být odstraněno. Až potestujeme protokol, nechám pročistit headery. $$$

## 3. Transport

The current UI and production box communicate using **WebSocket + JSON**.

Current firmware creates an `AsyncWebSocket` on path `/`. The current UI normally derives the WebSocket URL from the page hostname, but the supplied development snapshot overrides it with:

```text
ws://127.0.0.1:8080     $$$ to proto že tenhle override se používá při ladění na stanici, až když se nahrává ne server se zakomentuje (do a commit se dělá na stanici) $$$
```

The Perl mock controller listens on a configurable WebSocket TCP port, default `8080`.

A future MQTT transport is mentioned elsewhere in the project, especially for logging, but is not specified here because no complete MQTT wire implementation was found in the supplied sources.

## 4. Common message envelope

The intended/current application message shape is:

```json
{
  "_command_": "<command>",
  "_timestamp_": 123456,
  "_scope_": "<optional scope>",
  "data": {
    "...": "..."
  }
}
```

### 4.1 `_command_`

Command identifier. It selects a registered firmware handler or a UI command handler.

### 4.2 `data`

Object containing command-specific data.

The production firmware dispatches only this nested `data` object to command handlers.

### 4.3 `_scope_`

The current UI sender supports `_scope_` as an optional parameter. `JSON.stringify()` omits it when its JavaScript value is `undefined`.

The production firmware currently does not use `_scope_` during dispatch.

Treat `_scope_` as **reserved / partially implemented** until its semantics are explicitly defined.

### 4.4 `_timestamp_`

Timestamp semantics are currently inconsistent and must not yet be treated as a single normalized time base.

- **UI → Box:** UI sends `Date.now()`, i.e. Unix epoch milliseconds.
- **Production Box → UI:** firmware sends `millis()`, i.e. milliseconds since ESP boot.
- **Log transport:** `_timestamp_` is the log record timestamp.
- **Perl mock controller:** currently uses a field named `timestamp` (without underscores), with Perl `time()` seconds.

Therefore the earlier description "epoch time in milliseconds" is **not valid for all current senders**.

A future protocol revision should normalize both the field name and time base.
$$$ tohle by mělo být pořešeno, mock, controller i UI by měly používat výhradně Unix epoch time, odpovídajícím způsobem je upraven i mechanismus počítání driftu hodin $$$

## 5. Command inventory

### 5.1 UI → Box / controller

| Command | Production firmware | Current UI | Perl mock | Notes |
|---|---:|---:|---:|---|
| `opendoor` | yes | yes | yes | Open door request |
| `get_door` | yes | yes | yes | Query door state |
| `get_ambient` | yes | yes | yes | Query ambient light state |
| `get_pins` | yes | no in current UI | no | Older PIN-specific list command |
| `get_creds` | no | yes | yes | Generic credential list command; target direction |
| `set_pin` | yes | yes | yes | Create/update/delete PIN |
| `set_code` | no | yes | yes | Create/update/delete parcel/order code |
| `get_pager` | partial | yes | no | Production handler is currently placeholder |
| `get_status` | no | yes | no | UI requests it; no handler found in firmware/mock |
| `get_diagnostics` | yes | no current UI call found | no | Diagnostics |
| `snapshot_diagnostics` | yes | no current UI call found | no | Store diagnostic snapshot |
| `get_diagnostic_snapshots` | yes | no current UI call found | no | Read snapshots |
| `get_PinInfo` | defined only | no | no | Constant exists; no registered handler found |
| `_pong_` | yes | yes | accepted indirectly | Watchdog response |

### 5.2 Box / controller → UI

| Command | Production firmware | Current UI | Perl mock | Notes |
|---|---:|---:|---:|---|
| `c_content` | yes | yes | yes | Set `.innerHTML` of one or more HTML elements |
| `c_visibility` | yes | yes | yes | Show/hide one or more HTML elements |
| `c_enordis` | yes | yes | no | Enable/disable one or more HTML elements |
| `_ping_` | yes | yes | yes | Application-level watchdog request |
| `c_log` | separate log transport | not handled by current UI | no | Same envelope, different destination/use |

$$$ c_content je úplně odstraněno, stejně c_visibility (jejich ponechaná obsluha ve firmware je jen pro jistotu, odstraním hned jak se ukáže, že se nevolají),c_enordis je parciálně implementováno viz naše diskuse v chatu $$$

## 6. Generic UI-control commands

These commands deliberately address HTML element IDs. They are not domain-specific responses.

### 6.1 `c_content`

**Direction:** Box/controller → UI

Sets `innerHTML` for each key in `data`.

```json
{
  "_command_": "c_content",
  "_timestamp_": 123456,
  "data": {
    "lastresult": "Opening door number 0"
  }
}
```

UI behaviour is effectively:

```text
for each key in data:
    document.getElementById(key).innerHTML = data[key]
```

Current uses include:

- `lastresult`
- `pinrows`
- `coderows`
- diagnostic HTML/text/JSON targets

#### Important consequence

`c_content` may carry **HTML strings**, not only plain text. For example credential-list responses return complete `<tr>...</tr>` fragments for a `<tbody>`.

### 6.2 `c_visibility`

**Direction:** Box/controller → UI

```json
{
  "_command_": "c_visibility",
  "data": {
    "door_state_open": "yes",
    "door_state_closed": "no"
  }
}
```

Values:

- `"yes"` → UI removes/honours `hidden = false`
- `"no"` → UI sets `hidden = true`

Current firmware defines these door beacons:

- `door_state_open`
- `door_state_closed`
- `door_state_mixed`

Current UI contains open/closed images but not a mixed-state element. Production firmware therefore currently maps `DOOR_MIXED` to the open visual state and logs a warning.

Ambient-state beacons:

- `ambient_state_on`
- `ambient_state_off`

### 6.3 `c_enordis` $$$  připomínám můj popis v chatu $$$

**Direction:** Box → UI

Generic enable/disable operation.

```json
{
  "_command_": "c_enordis",
  "data": {
    "door_controls": "disable"
  }
}
```

Values:

- `"disable"` → `element.disabled = true`
- any other value currently behaves as enable; firmware emits `"enable"`

Production firmware uses this for `door_controls` while the door state machine is opening/otherwise preventing a new UI open action.

## 7. Credential model

### 7.1 Generic list request: `get_creds`

**Direction:** UI → controller  
**Current implementation:** UI + Perl mock; not yet production firmware.

Request payload generated by current UI:

```json
{
  "_command_": "get_creds",
  "_timestamp_": 1780000000000,
  "data": {
    "credId": 0,
    "credCount": 15,
    "credType": "ctPin"
  }
}
```

or:

```json
{
  "_command_": "get_creds",
  "_timestamp_": 1780000000000,
  "data": {
    "credId": 0,
    "credCount": 15,
    "credType": "ctCode"
  }
}
```

Fields:

| Field | Meaning | Current behaviour |
|---|---|---|
| `credId` | first credential ID / paging anchor | UI sends it; mock currently ignores it |
| `credCount` | requested number of records | UI sends it; mock currently ignores it |
| `credType` | credential type | mock dispatches `ctPin` vs `ctCode` |

Current mock responses:

For `ctPin`:

```json
{
  "_command_": "c_content",
  "data": {
    "pinrows": "<tr>...</tr>..."
  }
}
```

For `ctCode`:

```json
{
  "_command_": "c_content",
  "data": {
    "coderows": "<tr>...</tr>..."
  }
}
```

### 7.2 Older production list request: `get_pins`

**Direction:** UI/client → production firmware

Request fields accepted by current firmware:

- `pinId` (preferred in current handler)
- `pinid` as fallback
- `pinCount`
- `pincount` as fallback

Defaults:

- first PIN ID becomes `1` when missing/zero
- count becomes `100` when missing/zero

Response:

```json
{
  "_command_": "c_content",
  "data": {
    "pinrows": "<tr>...</tr>...",
    "pinrowcount": 4
  }
}
```

This is the production predecessor of generic `get_creds`.

### 7.3 PIN edit: `set_pin` $$$ set_creds $$$

**Direction:** UI → Box/controller

The current modal submits all form fields plus a synthetic `clicked` field identifying which submit button was used.

Typical create/save request:

```json
{
  "_command_": "set_pin",
  "_timestamp_": 1780000000000,
  "data": {
    "pinid": "0",
    "pinname": "Courier",
    "pinvalue": "12345678",
    "amount": "-1",
    "checkbox-unlimited": true,
    "datefrom": "2026-09-16",
    "dateto": "2026-09-30",
    "clicked": "savebutton"
  }
}
```

Current production firmware understands:

| Field | Meaning |
|---|---|
| `clicked` | `savebutton` or `deletebutton` |
| `pinid` | `0` / absent for a new PIN; non-zero for update/delete |
| `pinname` | PIN label/name |
| `pinvalue` | PIN value |
| `datefrom` | valid-from date/time as accepted by storage conversion |
| `dateto` | valid-to date/time |
| `amount` | remaining uses; `-1` means unlimited, `0` expired |
| `doornum` | optional mapped door number; UI does not currently provide this in PIN modal |

The UI additionally sends `checkbox-unlimited`; production firmware currently derives effective unlimited behaviour from `amount`, not from this checkbox field.

#### Create vs update

- `pinid == 0` → add new PIN
- `pinid != 0` → update existing PIN

The current UI disables `pinname` and `pinvalue` when editing an existing row, which matches production firmware behaviour where the mutable values are primarily validity/count/door mapping.

#### Delete

`clicked = "deletebutton"`.

Production firmware currently expects identifying PIN information including ID/name/value when deleting.

#### Responses

The initiating client receives status through:

```json
{
  "_command_": "c_content",
  "data": {
    "lastresult": "..."
  }
}
```

After successful production PIN data change, firmware currently broadcasts an updated `pinrows` table to all connected clients.

A source comment explicitly says this broad table broadcast is temporary and should later become an update-notification mechanism so each UI can request the page it currently displays.

### 7.4 Code edit: `set_code` $$$ set_creds $$$

**Direction:** UI → controller  
**Current implementation:** UI + Perl mock; not yet production firmware.

Typical request:

```json
{
  "_command_": "set_code",
  "_timestamp_": 1780000000000,
  "data": {
    "codeid": "0",
    "codename": "Order 123",
    "codevalue": "HZ1268956754M",
    "codefrom": "2026-09-16",
    "codeto": "2026-09-30",
    "clicked": "savebutton"
  }
}
```

Fields used by the current UI/mock:

| Field | Meaning |
|---|---|
| `clicked` | `savebutton` or `deletebutton` |
| `codeid` | `0` for create, non-zero for update/delete |
| `codename` | human-readable label |
| `codevalue` | parcel/order code value, UI maxlength currently 20 |
| `codefrom` | not-before date |
| `codeto` | not-after date |

The mock controller returns `lastresult` to the initiating client and broadcasts updated `coderows` to connected clients after a successful change.

### 7.5 Refactoring target for Codex

The protocol currently demonstrates a generic **credential** concept but still has PIN-specific implementation names in production firmware.

Recommended refactor rule:

> Rename code from `pin` to `credential` only where the logic is genuinely shared by all credential types. Keep concrete protocol field names such as `pinvalue`, concrete commands such as `set_pin`, and PIN-specific storage semantics unchanged unless a deliberate protocol migration is being made.

Examples of likely generic concepts:

- list/page credential records
- credential ID/count/type
- UI table/modal plumbing
- add/update/delete operation shape
- change notification

Examples that remain concrete:

- `pinvalue`
- use counter (`amount`) if only PINs support it
- `codevalue`
- any PIN-specific fixed length/storage representation

Do **not** mechanically rename wire-level keys without a migration plan.

## 8. Door commands

### 8.1 `get_door`

**Direction:** UI → Box/controller

Current UI request:

```json
{
  "_command_": "get_door",
  "data": {
    "doornum": 0
  }
}
```

Production firmware requires `doornum` and accepts a numeric JSON value or a numeric string. Values above 254 are rejected; 255 is reserved internally as an unknown/special value.

Successful response uses `c_visibility` with door-state beacons.

Errors are returned via `c_content.lastresult` where implemented.

### 8.2 `opendoor`

**Direction:** UI → Box/controller

Current UI modal supplies:

- `doornum`
- `presence`
- `checkbox-presence`
- synthetic `clicked = "buttonconfirm"`

Example:

```json
{
  "_command_": "opendoor",
  "data": {
    "doornum": "0",
    "presence": "X",
    "checkbox-presence": false,
    "clicked": "buttonconfirm"
  }
}
```

Production firmware uses:

- `doornum`
- `presence`
- `checkbox-presence`

It ignores `clicked` for this command.

If presence verification is requested, an empty/expired/non-matching presence code is rejected and a `c_content.lastresult` error is returned.

On acceptance, firmware sends `c_content.lastresult` and dispatches a `WebOpenBox` event to the state machine.

The Perl mock simulates physical behaviour by:

1. marking door open,
2. turning ambient light on,
3. broadcasting `c_visibility`,
4. closing the door after `dooropentime`,
5. turning ambient light off after an additional `ambientoverhang`.

This mock sequence demonstrates intended UI behaviour but is not a literal description of the production state machine.

## 9. Ambient light

### `get_ambient`

**Direction:** UI → Box/controller

Current UI sends:

```json
{
  "_command_": "get_ambient",
  "data": {
    "lightId": 0
  }
}
```

Production firmware currently ignores request data and returns global/current ambient state via `c_visibility`: $$$ c_ambientstate $$$

```json
{
  "_command_": "c_visibility",
  "data": {
    "ambient_state_on": "yes",
    "ambient_state_off": "no"
  }
}
```

`lightId` is therefore currently UI-side future-facing data rather than a production selection parameter.

## 10. Paging

### `get_pager`

**Direction:** UI → Box

Current UI:

```json
{
  "_command_": "get_pager",
  "data": {
    "pinId": 0,
    "pinCount": 15
  }
}
```

Production firmware has a registered handler and reads `pinId` / `pinCount`, but the actual pager behaviour is explicitly marked TODO. It logs the request and returns an empty `c_content` payload.

This command should be considered **partial / placeholder**, not obsolete.

With the generic credential model, paging will probably need to be generalized together with `get_creds`; that is a design/refactor issue, not an already implemented protocol fact.

## 11. `get_status`

The current UI sends on page initialization:

```json
{
  "_command_": "get_status",
  "data": {
    "dummy": "-"
  }
}
```

No corresponding handler was found in either the production firmware snapshot or supplied Perl mock controller.

Status: **UI-only / unfinished**.

## 12. Watchdog

OrangeBox currently uses an application-level `_ping_` / `_pong_` exchange inside WebSocket messages.

### 12.1 Production firmware

Box broadcasts:

```json
{
  "_command_": "_ping_",
  "_timestamp_": 123456,
  "data": {
    "ping_data": "ping"
  }
}
```

Current UI responds:

```json
{
  "_command_": "_pong_",
  "_timestamp_": 1780000000000,
  "data": {
    "pongdata": "some_data"
  }
}
```

Production firmware registers `_pong_` and updates the client watchdog timestamp. The content of `data` is currently ignored.

### 12.2 Perl mock difference

The mock sends `_ping_` with empty `data` and updates its `$last_pong` time on **every received application message**, not only `_pong_`.

This is a mock/firmware behavioural difference and should not be normalized silently.

## 13. Diagnostics

These production firmware commands are registered even though the supplied current UI does not call them.

### 13.1 `get_diagnostics`

**Direction:** client → Box

Response via `c_content`:

- `diagnostics` — HTML
- `diagnostics_text` — text
- `diagnostics_json` — JSON representation/string produced by diagnostics subsystem

### 13.2 `snapshot_diagnostics`

Stores a diagnostic snapshot.

Response via `c_content`: $$$ c_diagnostics $$$

- `lastresult`
- `diagnostic_snapshot_count`
- `diagnostic_snapshots` — HTML

### 13.3 `get_diagnostic_snapshots`

Response via `c_content`:

- `diagnostic_snapshot_count`
- `diagnostic_snapshots`
- `diagnostic_snapshots_text`
- `diagnostic_snapshots_json`

The implemented command name is **`get_diagnostic_snapshots`** (plural).

## 14. Logging

### `c_log`

The firmware has a dedicated WebSocket log transport that uses the same envelope shape but sends to the log server transport rather than the normal UI control channel.

```json
{
  "_command_": "c_log",
  "_timestamp_": 123,
  "data": {
    "source": "BOX-007",
    "area": 5,
    "severity": 2,
    "message": "...",
    "timestamp": 123
  }
}
```

Fields:

| Field | Meaning |
|---|---|
| `source` | log source, normally box/source identifier |
| `area` | logger area enum/value |
| `severity` | severity enum/value |
| `message` | log text |
| `timestamp` | event/log timestamp |

The exact numeric ranges should be documented from the logger/config definitions rather than assumed permanently; comments in the project indicate these may change.

A future MQTT transport may make logs subscribable by other consumers. That is planned behaviour, not part of the current WebSocket protocol implementation.

## 15. Response model

The protocol is **not a conventional request → `<request>_response` RPC protocol**.

A UI request commonly produces one or more generic UI mutation commands.

Examples:

```text
UI                          BOX
 |                           |
 |---- get_door ------------>|
 |<--- c_visibility ---------|
 |                           |
 |---- get_ambient --------->|
 |<--- c_visibility ---------|
 |                           |
 |---- get_pins ------------>|
 |<--- c_content ------------|
```

Credential mutations can additionally produce a direct status response and a broadcast table update:

```text
UI A                        BOX                        UI B
 |                           |                          |
 |---- set_pin ------------->|                          |
 |<--- c_content(lastresult)-|                          |
 |<--- c_content(pinrows) ---|---- c_content(pinrows)->|
```

The Perl mock implements the same pattern for `set_code` / `coderows`.

## 16. Current UI initialization sequence

The supplied current UI calls these functions when the page loads:

1. `getCreds(0, 15, 1)` → `get_creds`, `credType = ctPin`
2. `getCreds(0, 15, 2)` → `get_creds`, `credType = ctCode`
3. `getPinPager(0, 15)` → `get_pager`
4. `getDoorState(0)` → `get_door`
5. `getAmbientState(0)` → `get_ambient`
6. `getStatusContent()` → `get_status`
7. installs row handlers for PIN and code tables

$$$ sem ještě přijde zjištění stavu dynamicky povolovaných / zakazovaných částí ovládání / funkcí  až to bude implementováno $$$

This means the current UI expects generic `get_creds` even though the production firmware snapshot still implements `get_pins` instead.

## 17. Known inconsistencies / unfinished edges

These are intentionally retained in the documentation because they identify work to be completed rather than necessarily obsolete code.

### 17.1 Credential API split

- Production: `get_pins`, `set_pin`
- Current UI/mock: `get_creds`, `set_pin`, `set_code`

Target appears to be generic retrieval with concrete edit commands, unless later redesigned.

### 17.2 `get_PinInfo`

Defined in `config.h`, but no registered production handler was found. $$$ přijde smazat při globálním úklidu config.h $$$

### 17.3 `get_status`

Sent by current UI; no production/mock handler found.

### 17.4 Door mixed state

Firmware supports `DOOR_MIXED` and `door_state_mixed`, but current UI does not. Firmware temporarily represents mixed as open in the UI response.
$$$ reálně se bude používat až bude dokončeno ukládání konfigurace na controlleru a editace dveří v UI $$$

### 17.5 Timestamp mismatch

- UI: `_timestamp_` = epoch ms
- production firmware: `_timestamp_` = boot-relative ms
- Perl mock: `timestamp` = epoch seconds
$$$ mělo by být vyřešeno $$$

### 17.6 `_scope_`

Supported by UI sender; ignored by current production dispatcher.
$$$ Tohle je asi kandidát na zrušení, nové zprávy ho nepoužívají, nicméně zatím budu zneužívat toho, že přijímací rutiny controlleru ho ignorují a odložím finální rozhodnutí. $$$

### 17.7 Mock envelope field name

Mock uses `timestamp` rather than `_timestamp_` in its outgoing messages. The UI command handling still works because it dispatches primarily by `_command_`, but clock-drift processing expects `_timestamp_`.
$$$ Mělo by být pořešeno, mock posílá _timestamp_, globálně by měl být pořešen i rozpor sekundy / milisekundy, mock má explicitní proměnnou $clockdrift pro trestování posunu hodin controlleru $$$
### 17.8 Ambient `lightId`

UI sends it; production handler currently ignores request data.
$$$ Tohle bych zatím nechal být. Předpokládám,  že zatím se rozsvěcí všechna světla v boxu naráz, (škoda mosfetů), kdybychom se někdy dopracovali k lednici, asi bych v ní nechtěl zbytečně topit. Spíš mi připiš do poznámek, abych doplnil formální test čísla dveří do firmware. $$$

### 17.9 Pager

Registered but not implemented beyond logging/empty response.
$$$ Pager určitě bude, teď na něj není čas. Nejde jen o pager samotný, musí se udělat i celý hanshaking změnila se tabulka - řeknete si, jako část chcete poslat mezi controllerem a UI $$$

### 17.10 Table update strategy

Production PIN mutation currently broadcasts complete `pinrows`; source comments say the intended future design is an update notification followed by client-driven refresh of the currently visible page.
$$$ viz pager $$$

## 18. Guidance for the credential refactor

This section is intended as an implementation constraint for Codex or another code-refactoring agent.

1. **Preserve protocol compatibility by default.** Do not rename `_command_` values or JSON keys merely because C++/JS symbols are renamed.
2. **Rename only genuinely generic implementation concepts** from `pin` to `credential` / `cred`.
3. **Keep concrete credential types explicit.** PIN and parcel/order code have different fields and may acquire different validation/storage rules.
4. **Prefer one generic list/paging path** (`get_creds` + `credType`) over duplicate PIN/code list handlers, because the current UI and mock already follow this model.
5. **Do not invent `set_cred` yet.** Current evidence supports concrete `set_pin` and `set_code`; a generic edit command is not implemented in the supplied sources.
6. **Treat incomplete one-sided commands as TODOs, not dead code**, unless explicitly removed by project decision.
7. **Preserve generic UI mutation commands** (`c_content`, `c_visibility`, `c_enordis`) because both UI and firmware are structured around them.
8. **Separate refactoring from protocol cleanup.** Timestamp normalization, `_scope_`, mixed-door UI, paging semantics, and update notifications should be deliberate protocol changes rather than accidental consequences of renaming.

$$$ Tohle projdi pořádně, refaktor by měl být hotový, FW uz umí spravovat PINy i Čísla zakázek, je to sjednoceno pod generické zprávy cred_* $$$

## 19. Candidate next protocol decisions

The following decisions are not resolved by the supplied implementation and should be made explicitly before considering this document a stable protocol specification:

- canonical timestamp field and time base $$$ pořešeno $$$
- formal semantics of `_scope_` $$$ odloženo $$$
- final credential paging contract (`credId`, `credCount`, ordering, return count, end-of-list indication) $$$ plánováno po dokončení integrace čtečky$$$
- whether credential table responses remain HTML fragments or become structured records rendered by the UI $$$ vyřešeno, posílá se tabulka dat, ne html fragment, renderování html zajišťuje UI $$$
- notification format for credential-list changes $$$ plánováno, implementace společně se stránkováním $$$
- production storage/handler design for `ctCode` / `set_code` $$$ pořešeno $$$
- intended meaning and response for `get_status` $$$ tohle se přerodí v načtení stavu povolených / zakázaných operací $$$
- how door number `0` is interpreted in the final multi-door model $$$ tady nevím, kam míříš, dveře 0 jsou prostě první dveře, speciální význam má jen 255 - unknown, logické dveře jsou stejné s fyzickými, pokud nedojde ke spojení, číslo spojených dveří se musí vejít mezi poslední číslo fyzických dveří a 254, protože protokol nerozlišuje fyzické a logické dveře, de facto pracuje jen s logickými dveřmi $$$
- UI representation of `DOOR_MIXED` $$$ tohle musí počkat, řešení tohodle musí předcházet hluboká orba v UI (tlačítko pro každé dveře, editor dveří) i v controlleru, tam zase úplně chybí ukládání konfigurace $$$
- whether `c_content`, `c_visibility`, `c_enordis` remain the long-term UI API or become an intermediate compatibility layer $$$ tohle všechno jsou kandidáti na odstřel, byl to nejrychlejší způsob jak docílit funkčního html UI, ale je to krajně nevhodné pro implementaci nativní aplikace pro mobily: ambinet a door už namísto c_visibility používá specifické příkazy c_ambientstate a c_doorstate, pro přenos tabulek se namísto c_content používá c_setcred - vše čisté Json zprávy. c_enordis zůstane zachován jako  identifikátor příkazu, ale mění se obsah, sekce data bude obsahovat jen identfikátory funkčních celků UI a cílový stav door_controls:disable  cred_changes:enabled .... viz diskuse v chatu. Vznikne nový příkaz c_setlaststatus. Obecně je cílem oddělit controller od konkrkétní html reprezentace, což . $$$

---

## Appendix A — concise command map

```text
UI -> Box/controller
    opendoor
    get_door
    get_ambient
    get_pins                  [production legacy/current]
    get_creds                 [current UI + mock]
    set_pin
    set_code                  [current UI + mock]
    get_pager                 [partial]
    get_status                [UI only]
    get_diagnostics
    snapshot_diagnostics
    get_diagnostic_snapshots
    get_PinInfo               [defined only]
    _pong_

Box/controller -> UI
    c_content
    c_visibility
    c_enordis
    _ping_

Box -> log transport
    c_log
```

## Appendix B — current generic credential types

```text
ctPin   PIN credential
ctCode  parcel/order code credential
```

