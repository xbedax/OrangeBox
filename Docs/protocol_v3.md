# OrangeBox UI ↔ Controller Protocol

**Status:** current protocol specification  
**Audience:** OrangeBox firmware, Web UI, native mobile applications, test/mock implementations, and server-side integrations

## 1. Purpose and design rules

This document defines the application protocol used between an OrangeBox controller and its interactive clients.

The protocol is deliberately independent of any concrete HTML structure. A controller reports domain state and operation results; each client decides how to represent them. The current Web UI is one implementation of the client side, not part of the wire-format definition.

The following rules are normative:

1. Messages are JSON objects transported over WebSocket unless a section explicitly defines another transport.
2. Protocol-visible state is represented as structured data, not HTML fragments.
3. User-facing result messages are transported as stable symbolic result codes (`R*`) plus named parameters. The client performs localization.
4. Local UI strings (`L*`) form a shared client vocabulary and are not transmitted by the controller.
5. Log records use stable numeric message identifiers plus parameters. Log message IDs are never recycled and the meaning of an existing ID or parameter position must not be changed.
6. All protocol-visible timestamps are Unix epoch time in seconds.
7. `millis()` is reserved for relative timing inside firmware. Firmware timeout tests use `timeoutElapsed()` or an equivalent wrap-safe elapsed-time calculation.
8. Door numbers in the protocol are logical door numbers. Door `0` is valid. Value `255` is reserved for unknown/special internal values and is not a valid requested door number.

## 2. Transport

The interactive protocol uses WebSocket with JSON application messages. The firmware WebSocket endpoint is `/`.

A normal deployment therefore uses the controller/server host with the root WebSocket path. Development clients may override the URL to point at a local mock server; such overrides are development configuration and are not protocol semantics.

Logging currently uses a separate WebSocket log transport described in section 12.

## 3. Common message envelope

Application messages use this envelope:

```json
{
  "_command_": "<command>",
  "_timestamp_": 1790000000,
  "data": {
    "...": "..."
  }
}
```

### 3.1 `_command_`

String command identifier. It selects the semantic meaning of `data`.

### 3.2 `_timestamp_`

Unix epoch time in **seconds**.

The timestamp identifies the sender's wall-clock time at message creation. It may be used by a client to estimate clock drift, but is not a sequencing or transaction identifier.

### 3.3 `data`

Command-specific JSON object.

### 3.4 `_scope_`

The current Web UI sender can include `_scope_`, but the controller does not use it for dispatch and current protocol commands do not depend on it. It is therefore reserved and clients must not rely on it.

## 4. Time handling rules

1. All protocol-visible timestamps MUST be Unix epoch time in seconds.
2. Firmware MUST use `time()` / `time_t` for timestamps persisted, logged, transmitted, or otherwise exposed outside internal relative-timing logic.
3. `millis()` MUST be used only for internal relative timing such as delays, watchdogs, retries, debounce intervals, and state-machine timeouts.
4. All `millis()`-based timeout comparisons MUST use `timeoutElapsed()` or an equivalent wrap-safe elapsed-time calculation.
5. Epoch timestamps and `millis()` values MUST NOT be mixed.

## 5. Command overview

### 5.1 Client → controller

| Command | Purpose |
|---|---|
| `opendoor` | Request opening of a logical door |
| `get_door` | Query the state of one logical door |
| `get_ambient` | Query ambient-light state |
| `get_status` | Request current client-relevant controller status/permissions |
| `get_creds` | Retrieve a page of credentials |
| `set_cred` | Create, update, or delete a credential |
| `get_diagnostics` | Retrieve current diagnostic data |
| `snapshot_diagnostics` | Store a diagnostic snapshot |
| `get_diagnostic_snapshots` | Retrieve stored diagnostic snapshots |
| `_pong_` | Application watchdog response |

### 5.2 Controller → client

| Command | Purpose |
|---|---|
| `c_doorstate` | Report logical door state |
| `c_ambientstate` | Report ambient-light state |
| `c_enordis` | Enable or disable a logical client operation |
| `c_setcred` | Return credential data/change result or notify that credentials changed |
| `c_lastresult` | Report a user-facing result code plus parameters |
| `c_diagnostics` | Return current diagnostics |
| `c_diagnostic_snapshots` | Return diagnostic snapshots |
| `_ping_` | Application watchdog request |

The protocol is not RPC-style `<request>_response`. A request can cause one or more semantically different controller messages, and asynchronous state changes may be broadcast without a preceding request.

## 6. Result messages and localization

### 6.1 `c_lastresult`

**Direction:** controller → client

User-facing operation results are sent as a symbolic code, never as localized prose.

Example:

```json
{
  "_command_": "c_lastresult",
  "_timestamp_": 1790000000,
  "data": {
    "lastresult": "R21",
    "params": {
      "door_number": 0
    }
  }
}
```

The client resolves `R21` in its language catalog and substitutes the named parameter `<door_number>`.

`params` is optional and is omitted when a result code requires no parameters.

### 6.2 Result code catalog

The symbolic definitions are maintained in `include/result_code.h`; client translations are maintained in `UI/js/translations.js`.

| Code | Canonical meaning | Parameters |
|---|---|---|
| `R17` | Missing door number in request | – |
| `R18` | Invalid door number | – |
| `R19` | Invalid door-number format | – |
| `R20` | Door open | `door_number` |
| `R21` | Opening door | `door_number` |
| `R22` | Door closed | `door_number` |
| `R23` | Diagnostic snapshot stored | – |
| `R24` | Diagnostic snapshot failed | – |
| `R25` | Camera operation error | – |
| `R26` | Presence code required but not supplied | – |
| `R27` | Invalid presence code | – |
| `R28` | Local operation is in progress | – |
| `R29` | Image not ready | – |
| `R30` | Door request queue unavailable/full | – |
| `R31` | Invalid credential type | – |
| `R32` | Invalid credential pagination | – |
| `R33` | Invalid credential action | – |
| `R34` | Invalid credential ID | – |
| `R35` | Credential not found | – |
| `R36` | Invalid credential name | – |
| `R37` | Invalid credential value | – |
| `R38` | Invalid credential validity range | – |
| `R39` | Invalid credential door number | – |
| `R40` | Invalid unlimited-use option | – |
| `R41` | Invalid remaining-use count | – |
| `R42` | Credential change failed | – |
| `R43` | Credential saved | – |
| `R44` | Credential deleted | – |

Result-code identifiers are protocol identifiers. Clients should treat an unknown `R*` code as an unknown message identifier and provide a safe fallback rather than assuming the English wording.

### 6.3 Local client string catalog (`L*`)

`L*` identifiers name local UI text. They are **not transmitted by the controller**, but Web UI and native applications should reuse these identifiers where the same concept is presented so that all clients speak the same UI vocabulary.

Current identifiers are:

```text
L_box_control
L_open_box
L_door_state
L_ambient
L_camera_data
L_active_pins
L_Name
L_Value
L_Not_before
L_Not_after
L_Count
L_Add_new
L_Id
L_Be_sure
L_Click_row
L_Active_codes
L_Box_time
L_Unlimited
L_Pin_details
L_Pin_value
L_Pin_name
L_Use_num
L_Lab_unlimited
L_Empty_meaning
L_Btn_save
L_Btn_delete
L_Btn_cancel
L_Btn_propose
L_Btn_confirm
L_Code_details
L_Code_value
L_Code_name
L_Open_confirm
L_Presence_code
L_Presence_act
L_Verify_code
```

Current language catalogs are Czech (`cs`), English (`en`), and German (`de`). Clients may add languages, but a language catalog should contain the complete set of supported keys. Missing keys may fall back to English in the Web UI, but missing translations should still be treated as catalog defects.

## 7. Door protocol

### 7.1 `get_door`

**Direction:** client → controller

Request:

```json
{
  "_command_": "get_door",
  "_timestamp_": 1790000000,
  "data": {
    "doornum": 0
  }
}
```

`doornum` accepts a JSON integer or a decimal numeric string. Valid range is `0..254`.

Successful response is `c_doorstate`.

### 7.2 `c_doorstate`

**Direction:** controller → client

```json
{
  "_command_": "c_doorstate",
  "_timestamp_": 1790000000,
  "data": {
    "door_num": 0,
    "state": "closed"
  }
}
```

Fields:

| Field | Type | Meaning |
|---|---|---|
| `door_num` | integer | Logical door number |
| `state` | string | Currently `open` or `closed` |

The controller may internally detect a mixed state for a logical door mapped to multiple physical contacts. The current client protocol exposes such a state as `open`, because a partly open group must not be displayed as closed.

`c_doorstate` can be a direct reply to `get_door` or an asynchronous notification after a physical door-state change.

### 7.3 `opendoor`

**Direction:** client → controller

Typical request:

```json
{
  "_command_": "opendoor",
  "_timestamp_": 1790000000,
  "data": {
    "doornum": 0,
    "presence": "123456",
    "checkbox-presence": true
  }
}
```

Fields:

| Field | Required | Meaning |
|---|---:|---|
| `doornum` | yes | Logical door number `0..254`; integer or decimal numeric string |
| `presence` | conditional | Presence code supplied by the client |
| `checkbox-presence` | no | When true, controller validates `presence` against the currently valid presence code |

When accepted, the request is queued for the state machine and the initiating client receives `c_lastresult` with `R21` and parameter `door_number`.

Failure is reported with an appropriate `R*` code, for example `R18`, `R26`, `R27`, or `R30`.

Door opening is asynchronous. Acceptance of `opendoor` does not mean the physical door is already open. Physical state is reported separately by `c_doorstate`.

## 8. Ambient-light protocol

### 8.1 `get_ambient`

**Direction:** client → controller

The current controller treats ambient lighting as a single box-level state. Request `data` is currently unused.

```json
{
  "_command_": "get_ambient",
  "_timestamp_": 1790000000,
  "data": {}
}
```

### 8.2 `c_ambientstate`

**Direction:** controller → client

```json
{
  "_command_": "c_ambientstate",
  "_timestamp_": 1790000000,
  "data": {
    "state": "on"
  }
}
```

`state` is `on` or `off`.

The command can be a direct response to `get_ambient` or an asynchronous state-change notification.

Future hardware may expose more than one independently controlled light zone; no multi-light selector is currently part of the active semantics.

## 9. Client operation availability

### 9.1 `c_enordis`

**Direction:** controller → client

`c_enordis` controls whether the client should allow a **logical operation**. Keys do not identify HTML elements.

Current example:

```json
{
  "_command_": "c_enordis",
  "_timestamp_": 1790000000,
  "data": {
    "opendoor": "disable"
  }
}
```

Values are:

- `enable`
- `disable`

The Web UI maps a logical operation to any controls carrying that operation identifier. A native application is free to represent the same operation differently.

At present the implemented operation is `opendoor`.

### 9.2 `get_status`

**Direction:** client → controller

`get_status` requests the current client-relevant state after connection/page initialization.

The current controller responds with:

1. current state for logical door `0` using `c_doorstate`, and
2. current `opendoor` permission using `c_enordis`.

The status mechanism is intended to grow as more client operations become dynamically enabled/disabled.

### 9.3 Future concurrency control

A future extension is expected to arbitrate conflicting operations such as door control and credential editing between multiple clients/controller-local actions. The intended model is a short-lived operation ticket/lease: the first requester obtains authority, competing clients are disabled, the controller validates the ticket when executing the operation, and tickets expire so a dead client cannot permanently block the system.

This ticket mechanism is not part of the current wire protocol and no field names are reserved here for it.

## 10. Credential protocol

A credential is an authorization or identification record handled by the common credential subsystem.

Current types:

| `credType` | Meaning |
|---|---|
| `ctPin` | PIN/password credential |
| `ctCode` | parcel/order-number credential |

Common controller code is credential-generic. Type-specific names and fields are used only where the credential type actually differs.

### 10.1 `get_creds`

**Direction:** client → controller

```json
{
  "_command_": "get_creds",
  "_timestamp_": 1790000000,
  "data": {
    "credType": "ctPin",
    "credId": 0,
    "credCount": 15
  }
}
```

Fields:

| Field | Required | Meaning |
|---|---:|---|
| `credType` | yes | `ctPin` or `ctCode` |
| `credId` | no | First credential ID / paging anchor; default `0` |
| `credCount` | no | Maximum requested records; default `15`, valid `0..100` |

The controller returns a `c_lastresult` only when the credential helper produced a result code, followed by a `c_setcred` payload.

### 10.2 `c_setcred` — credential page

**Direction:** controller → client

The response contains parallel arrays grouped under `rows`.

PIN example:

```json
{
  "_command_": "c_setcred",
  "_timestamp_": 1790000000,
  "data": {
    "credType": "ctPin",
    "rows": {
      "pinid": [1, 2],
      "pinname": ["Courier", "Family"],
      "pinvalue": ["12345678", "87654321"],
      "datefrom": ["2026-10-01", "0001-01-01"],
      "dateto": ["2026-12-31", "9999-12-31"],
      "amount": ["5", "-1"]
    },
    "rowCount": 2
  }
}
```

Code example uses the corresponding type-specific fields:

```json
{
  "credType": "ctCode",
  "rows": {
    "codeid": [7],
    "codename": ["Order 123"],
    "codevalue": ["HZ1268956754M"],
    "codefrom": ["2026-10-01"],
    "codeto": ["2026-10-31"]
  },
  "rowCount": 1
}
```

The client renders these records itself. No HTML is transferred by the credential protocol.

Open-ended validity is represented in the client-facing date format by the sentinel dates:

- `0001-01-01` for no lower bound
- `9999-12-31` for no upper bound

### 10.3 `set_cred`

**Direction:** client → controller

One generic command performs create, update, and delete operations. `credType` selects the concrete credential fields.

Common fields:

| Field | Meaning |
|---|---|
| `credType` | `ctPin` or `ctCode` |
| `clicked` | action identifier: save or delete button/action beacon |
| type-specific ID | `0` for create; non-zero for update/delete |
| type-specific name | human-readable credential name |
| type-specific value | PIN or parcel/order number |
| type-specific validity fields | not-before / not-after date |
| `doornum` | optional logical door mapping, `0..254` |

PIN additionally supports:

| Field | Meaning |
|---|---|
| unlimited-use flag | boolean |
| remaining-use count | signed count; `-1` represents unlimited |

Parcel/order-number credentials currently have a single-use semantic in storage.

The controller validates type, pagination/action syntax, ID, name/value, validity range, mapped door, unlimited flag, and remaining count. Validation failures use `R31..R42` as appropriate.

Successful create/update/delete returns `c_lastresult` (`R43` or `R44`) and a `c_setcred` response describing the operation result.

### 10.4 Credential-change notification

After a successful mutation the controller broadcasts:

```json
{
  "_command_": "c_setcred",
  "_timestamp_": 1790000000,
  "data": {
    "credType": "ctPin",
    "changed": true
  }
}
```

Each client then reloads the page it is currently displaying, preserving its own credential type and pagination state.

This avoids broadcasting a specific client's current table page to all clients.

### 10.5 Paging outlook

The current request already carries `credId` and `credCount`. Further paging/handshake work is expected to formalize navigation and update synchronization, including a clean way to tell clients that a credential table changed and let each client request the portion it needs.

## 11. Diagnostics

### 11.1 `get_diagnostics`

**Direction:** client → controller

Response command: `c_diagnostics`.

Current payload can contain:

```json
{
  "html": "...",
  "text": "...",
  "json": "..."
}
```

Diagnostics are an engineering/debugging surface rather than the model for normal application data. The presence of an `html` diagnostic representation does not imply that normal UI protocol messages may contain HTML.

### 11.2 `snapshot_diagnostics`

Stores a diagnostic snapshot.

The initiating client receives `c_lastresult` with `R23` or `R24`, plus `c_diagnostic_snapshots` containing the current snapshot data.

### 11.3 `get_diagnostic_snapshots`

**Direction:** client → controller

Response command: `c_diagnostic_snapshots`.

Current payload can contain:

```json
{
  "count": 3,
  "html": "...",
  "text": "...",
  "json": "..."
}
```

## 12. Logging protocol

Logging is structurally separate from UI localization.

### 12.1 Stable message IDs

Every log message has a stable numeric identifier defined symbolically in `include/logger.h`. Human-readable format strings are mapped in `include/log_messages.def`.

Rules:

1. A log message ID is permanent once assigned.
2. Retired IDs MUST NOT be reused.
3. The semantic meaning of an existing message ID MUST NOT change.
4. Parameter positions/meanings for an existing message ID MUST remain stable.
5. Source code should use the symbolic `A_*` constant rather than a bare numeric literal.

The `A_*` prefix therefore names source-code constants; the value placed on the wire is numeric.

### 12.2 Log parameters

Log format strings use positional parameters such as `{p1}`, `{p2}`, ... . The logger transports the message identifier and typed parameter values rather than preformatted localized prose.

Logs are technical records and are not localized by the UI translation catalog.

### 12.3 WebSocket log envelope

The log transport uses the common envelope with command `c_log`:

```json
{
  "_command_": "c_log",
  "_timestamp_": 1790000000,
  "data": {
    "source": "BOX-007",
    "area": 6,
    "severity": 2,
    "message": 123,
    "timestamp": 1790000000
  }
}
```

`message` is the stable numeric message identifier. The concrete log-record representation may additionally carry the message parameters according to the logger transport implementation.

Current log areas include:

| Area | Meaning |
|---:|---|
| `2` | Internal environment / conditions |
| `3` | Communication |
| `4` | System |
| `5` | Access control / credential management |
| `6` | Business operations (door operation, image capture, credential use, etc.) |

Severity values are defined by the logger configuration and should be consumed as protocol data rather than inferred from text.

## 13. Watchdog

OrangeBox uses an application-level watchdog over the WebSocket connection.

Controller periodically sends `_ping_`; the client responds with `_pong_`.

The payload is not application data and clients must not attach business semantics to it.

Watchdog scheduling and disconnect deadlines are internal relative timing and therefore use `millis()` plus wrap-safe timeout handling; watchdog messages themselves carry normal epoch-second `_timestamp_` values.

## 14. Controller state model

The controller state machine currently defines these states:

| State | Meaning |
|---|---|
| `Home` | Normal idle state; all doors closed, ambient inactive |
| `Password` | Local keyboard PIN/password entry |
| `Presence` | Presence code displayed/active |
| `Opening` | A valid open request is being executed; lock pulse/opening sequence active |
| `Open` | At least one relevant door is physically open |
| `Closed` | Door has closed; post-close ambient/camera activity may still be running |
| `External` | External command/control operation active |
| `BadPass` | Delay/state after invalid password entry |
| `Scan` | Waiting for a parcel/order number from the scanner |

Principal event groups are:

- keyboard events (`KeyboardKey1`, `KeyboardKey2`, `KeyboardKey3`, Enter, Cancel),
- `WebOpenBox`,
- door sensor events (`DoorOpened`, `DoorClosed`),
- timeout events (password, presence, ambient, door-open, scan),
- scanner/result events handled by the scanner/state-machine integration.

A simplified external-operation path is:

```text
Home
  |
  | accepted open request / valid local credential
  v
Opening
  |
  | physical door opens
  v
Open
  |
  | physical door closes
  v
Closed
  |
  | post-close timeout / cleanup
  v
Home
```

Local password, presence-code, and scanner flows enter their own intermediate states and converge on the opening path after successful authorization.

Protocol commands do not directly set state-machine state. For example, `opendoor` validates and queues an open request; the state machine then decides the actual transition. Physical door transitions are reported independently through `c_doorstate`.

### 14.1 Client-operation gating

The state machine exposes whether external door control is currently allowed. `get_status` and state changes use this to drive `c_enordis` for `opendoor`.

This separation is intentional: the controller owns operation validity; the client owns presentation.

## 15. Numeric input convention

Where protocol fields are numeric, browser form clients may naturally submit decimal strings while native clients naturally submit JSON integers.

Protocol parsers should therefore accept both representations where documented, reject booleans/non-decimal text, enforce the documented range, and normalize to the internal numeric type.

This convention currently applies to credential numeric fields and door numbers.

## 16. Client initialization

A client establishing a new session should obtain the state it needs rather than assuming defaults. The current Web UI performs the equivalent of:

1. request current PIN credential page with `get_creds`,
2. request current parcel/order-code page with `get_creds`,
3. request current door state with `get_door`,
4. request current ambient state with `get_ambient`,
5. request current operation status with `get_status`,
6. begin processing asynchronous controller notifications and watchdog traffic.

A native application may request only the subsets needed by its current screen, but it should not infer current controller state from stale local state.

## 17. Protocol evolution rules

When extending the protocol:

1. Prefer domain-specific structured messages over presentation-specific mutations.
2. Do not add dependencies on HTML element IDs, DOM layout, CSS classes, or other Web-UI implementation details.
3. Prefer stable symbolic codes for user-facing results and client-local translation.
4. Preserve existing `R*` meaning and placeholder names once published.
5. Keep the `L*` key set synchronized across language catalogs; clients may add additional languages using the same identifiers.
6. Preserve log numeric message IDs and their parameter semantics permanently; never recycle retired IDs.
7. Add new credential types through the generic credential model where practical rather than duplicating the entire list/edit protocol.
8. Treat new asynchronous state notifications as authoritative domain state, not as commands telling a particular UI how to draw itself.
9. Maintain epoch seconds for all externally visible time values and wrap-safe `millis()` timing internally.

## 18. Planned extensions

The following directions are known but are not normative parts of the current wire protocol:

- formal credential paging/navigation and table-change handshake,
- multi-client operation tickets/leases for mutually exclusive controller/UI operations,
- broader `c_enordis` coverage, including credential-edit operations,
- multi-door UI/editor and full presentation of logical-door mixed states,
- persistent controller configuration and door mapping editing,
- potential independently controlled ambient/light zones,
- further consolidation of structured logging transports while preserving stable numeric log IDs.

Field names and message shapes for these future features should be specified only when their implementation is designed.

## Appendix A — result codes

```text
R17  Missing door number
R18  Invalid door number
R19  Invalid door-number format
R20  Door open                         params: door_number
R21  Opening door                     params: door_number
R22  Door closed                       params: door_number
R23  Diagnostic snapshot stored
R24  Diagnostic snapshot failed
R25  Camera operation error
R26  Presence code required
R27  Invalid presence code
R28  Local operation underway
R29  Image not ready
R30  Door request queue unavailable/full
R31  Invalid credential type
R32  Invalid credential pagination
R33  Invalid credential action
R34  Invalid credential ID
R35  Credential not found
R36  Invalid credential name
R37  Invalid credential value
R38  Invalid validity date range
R39  Invalid credential door number
R40  Invalid unlimited-use option
R41  Invalid remaining-use count
R42  Credential change failed
R43  Credential saved
R44  Credential deleted
```

## Appendix B — local UI string identifiers

```text
L_box_control
L_open_box
L_door_state
L_ambient
L_camera_data
L_active_pins
L_Name
L_Value
L_Not_before
L_Not_after
L_Count
L_Add_new
L_Id
L_Be_sure
L_Click_row
L_Active_codes
L_Box_time
L_Unlimited
L_Pin_details
L_Pin_value
L_Pin_name
L_Use_num
L_Lab_unlimited
L_Empty_meaning
L_Btn_save
L_Btn_delete
L_Btn_cancel
L_Btn_propose
L_Btn_confirm
L_Code_details
L_Code_value
L_Code_name
L_Open_confirm
L_Presence_code
L_Presence_act
L_Verify_code
```

## Appendix C — source-of-truth files

The implementation definitions most directly governing this protocol are currently:

```text
include/result_code.h        R* protocol result identifiers
UI/js/translations.js        R* translations and L* local UI string catalog
src/main.cpp                 WebSocket command handlers and controller notifications
src/cred_protocol.cpp        transport-independent credential wire parsing/formatting
include/cred_protocol.h      credential protocol interface
include/state_machine.h      controller states and events
src/state_machine.cpp        state transition behavior
src/websocket.cpp            common WebSocket envelope and watchdog transport
include/logger.h             stable numeric log message IDs and log areas
include/log_messages.def     numeric log ID -> format/parameter catalog
src/websocket_log_transport.cpp  WebSocket log wire transport
include/timeout.h            wrap-safe internal timeout helpers
```
