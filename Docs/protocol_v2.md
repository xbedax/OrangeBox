# OrangeBox UI ↔ Box Protocol

**Status:** working specification, revision 2  
**Date:** 2026-09-30  
**Scope:** application protocol between OrangeBox controller and UI clients. The protocol is intentionally independent of a concrete HTML representation so that the same controller can serve the web UI and future native mobile applications.

## 1. Design principles

The protocol carries **domain state and operations**, not instructions for manipulating a particular HTML document.

Consequences:

- the controller reports door state using `c_doorstate`, not by hiding/showing HTML elements;
- the controller reports ambient-light state using `c_ambientstate`;
- credential lists and changes use structured JSON through `get_creds`, `set_cred` and `c_setcred`;
- human-readable operation results use a dedicated status message (`c_lastresult` in the current implementation; see the planned cleanup note below);
- `c_enordis` remains part of the protocol because enabling/disabling user operations is an application-level concern, but its keys describe logical frontend operations/functional areas rather than DOM element IDs.

The former generic HTML mutation commands `c_content` and `c_visibility` are **not part of the current protocol**. Compatibility code may temporarily remain in source while migration is tested, but clients must not depend on it.

## 2. Terminology

### 2.1 Credential

A **credential** is a record that may authorize or identify an access/delivery action. Generic firmware functionality uses `cred` / `credential`; PIN- and parcel-code-specific names remain only where the concrete type requires different fields or semantics.

Current credential types:

| Type | Wire value | Specific value field | Notes |
|---|---|---|---|
| PIN/password | `ctPin` | `pinvalue` | Supports usage counter / unlimited mode |
| Parcel/order number | `ctCode` | `codevalue` | Currently treated as a single-use credential by storage logic |

The protocol distinguishes the **generic operation** from **type-specific fields**. Listing, paging, creating, updating and deleting use the common credential path; the record fields still identify PIN vs. parcel/order code where appropriate.

### 2.2 Door number

The protocol works with logical door numbers. A simple physical door normally has the same logical number as its physical door index; grouped physical doors may form another logical door.

Valid externally addressable door numbers are `0..254`. Door `0` is the first normal door. Value `255` is reserved internally for unknown/special state and must not be sent as a normal door identifier.

The protocol does not distinguish physical and logical doors at the wire level.

## 3. Transport and message envelope

The current controller/UI transport is **WebSocket + JSON**.

Application messages use this envelope:

```json
{
  "_command_": "<command>",
  "_timestamp_": 1790784000,
  "data": {
    "...": "..."
  }
}
```

### 3.1 `_command_`

Identifies the application message type.

### 3.2 `_timestamp_`

Protocol timestamp. The target/current protocol semantics are **Unix epoch time in seconds** on both sides.

Clock drift is calculated from this common time base. Boot-relative `millis()` values are internal timing values and MUST NOT be used as protocol timestamps.

### 3.3 Time handling rules

1. All protocol-visible timestamps MUST be Unix epoch time in seconds.
2. Firmware MUST use `time()` / `time_t` for timestamps persisted, logged, transmitted, or otherwise exposed outside internal timing logic.
3. `millis()` MUST be used only for internal relative timing.
4. All `millis()`-based timeout comparisons MUST use the project `timeoutElapsed()` helper (or an explicitly equivalent wrap-safe elapsed-time calculation).
5. Epoch time and `millis()`-based values MUST NOT be mixed.

The current WebSocket envelope implementation conforms to the timestamp rule: `notifyClients()` and `sendMessage()` both populate `_timestamp_` from `time(nullptr)`.


### 3.3 `data`

Command-specific JSON object. Incoming controller handlers receive this nested object after command dispatch.

### 3.4 `_scope_`

`_scope_` is currently reserved. New protocol messages do not require it and the controller ignores it when present.

No semantics are specified in this revision. It may be removed in a later cleanup once compatibility requirements are known.

## 4. Current command map

### 4.1 UI/client → Box

| Command | Purpose | Status |
|---|---|---|
| `opendoor` | Request opening of a logical door | current |
| `get_door` | Read current state of one logical door | current |
| `get_ambient` | Read ambient-light state | current |
| `get_creds` | Retrieve credentials of one type | current |
| `set_cred` | Create, update or delete a credential | current |
| `get_diagnostics` | Retrieve current diagnostics | current |
| `snapshot_diagnostics` | Store and return diagnostic snapshot information | current |
| `get_diagnostic_snapshots` | Retrieve stored diagnostic snapshots | current |
| `_pong_` | Application watchdog response | current |

### 4.2 Box → UI/client

| Command | Purpose | Status |
|---|---|---|
| `c_doorstate` | Door-state reply/change notification | current |
| `c_ambientstate` | Ambient-light reply/change notification | current |
| `c_setcred` | Credential data/result/change notification | current |
| `c_lastresult` | Human-readable status/result | current implementation |
| `c_diagnostics` | Current diagnostic data | current |
| `c_diagnostic_snapshots` | Diagnostic snapshot data | current |
| `c_enordis` | Enable/disable logical UI operations | partially generalized |
| `_ping_` | Application watchdog request | current |

### 4.3 Separate log transport

`c_log` uses the same general JSON envelope but belongs to the logging transport rather than the normal UI control flow.

## 5. Commands removed from the protocol surface

The following names belong to earlier implementation stages and must not be used by new clients:

- `c_content`
- `c_visibility`
- `get_pins`
- `set_pin`
- `set_code`
- `get_PinInfo`

Old handlers/constants may temporarily remain in source for migration safety. Their presence in code does not make them part of revision 2.

`get_pager` is also not a current command. Paging remains planned, but the final mechanism will be part of the generic credential handshake rather than the old PIN-specific pager design.

The former `get_status` concept is expected to evolve into retrieval/synchronization of operation enable/disable state; the final request command is not specified yet.

## 6. Door state

### 6.1 `get_door`

**Direction:** UI → Box

```json
{
  "_command_": "get_door",
  "_timestamp_": 1790784000,
  "data": {
    "doornum": 0
  }
}
```

`doornum` may be represented by a JSON number or a numeric string where compatibility requires it. Values outside `0..254` are invalid.

### 6.2 `c_doorstate`

**Direction:** Box → UI  
**Use:** direct reply to `get_door` and asynchronous state-change notification.

```json
{
  "_command_": "c_doorstate",
  "_timestamp_": 1790784000,
  "data": {
    "door_num": 0,
    "state": "closed"
  }
}
```

Current wire states:

- `closed`
- `open`

The controller internally supports `DOOR_MIXED` for grouped logical doors. The current external representation deliberately maps a mixed group to `open` so that a partially open group can never appear safely closed.

A dedicated UI representation for `mixed` is deferred until door configuration/grouping and per-door UI are implemented.

$$$ Note thet controller prevents opening more than exactly one logical door at a time to prevent mis-delivery.

### 6.3 `opendoor`

**Direction:** UI → Box

Typical payload:

```json
{
  "_command_": "opendoor",
  "_timestamp_": 1790784000,
  "data": {
    "doornum": "0",
    "presence": "X",
    "checkbox-presence": false
  }
}
```

Fields:

| Field | Meaning |
|---|---|
| `doornum` | logical door number `0..254` |
| `presence` | presence code, when verification is requested |
| `checkbox-presence` | whether presence-code verification is required |

When the request passes validation, the controller injects a `WebOpenBox` event into the state machine. The controller reports a human-readable result separately through `c_lastresult`.

$$$ The user opening box door from UI may request the person entering the box to prove her physical presence near the box. To facilitaate this the controller presents on the box display a random number (a presence code) which has limited validity. It is expected that person reads the presence code to user which enters it into opening form. Door is opened only when the entered code matches the generated one.

## 7. Ambient state

### 7.1 `get_ambient`

**Direction:** UI → Box

The current controller returns the global/current ambient state. A client may still carry `lightId` for future use, but it is not currently used to select an independently controlled light.

```json
{
  "_command_": "get_ambient",
  "_timestamp_": 1790784000,
  "data": {
    "lightId": 0
  }
}
```

### 7.2 `c_ambientstate`

**Direction:** Box → UI  
**Use:** direct reply and asynchronous change notification.

```json
{
  "_command_": "c_ambientstate",
  "_timestamp_": 1790784000,
  "data": {
    "state": "on"
  }
}
```

States:

- `on`
- `off`

**TODO:** add a formal firmware-side validation/test for light/door identifiers before independent lighting is implemented. The current hardware assumption is that all ambient lights switch together.

## 8. Credential protocol

$$$ Currently two types of credentials are defined:
Pin: an decimal number with possible leading zeroes of fixed length PIN_CODE_LEN (see the config.h - 8 currently)
Code: a case sensitive string consisting any combination of 0..9a..zA..Z characters of the lenght between PACKET_NUMBER_MIN and PACKET_NUMBER_MAX  (both values -currently 8 and 20 respectively- are project wide and specified in config.h). The controller uses only the prefix of the length specified by PACKET_NUMBER_MATCH  (currently 8) to match the code read with stored value. These values are subject of future adjustment.

### 8.1 `get_creds`

**Direction:** UI → Box

```json
{
  "_command_": "get_creds",
  "_timestamp_": 1790784000,
  "data": {
    "credId": 0,
    "credCount": 15,
    "credType": "ctPin"
  }
}
```

Fields:

| Field | Meaning |
|---|---|
| `credType` | `ctPin` or `ctCode` |
| `credId` | first credential ID / paging anchor; default `0` |
| `credCount` | maximum number of requested records; current implementation accepts up to `100`, default `15` |

Invalid type or paging data produces an unsuccessful structured response and a human-readable `c_lastresult` where applicable.

$$$ The controller ignores credId and credCout at present and sends all active records of requested type. It's to be expected the attributes will be utilized after completion of paging mechanism.

### 8.2 `c_setcred` — credential data

**Direction:** Box → UI

A normal list response contains structured data. HTML rendering is exclusively a client concern.

PIN example:

```json
{
  "_command_": "c_setcred",
  "_timestamp_": 1790784000,
  "data": {
    "credType": "ctPin",
    "rows": {
      "pinid": [1, 2],
      "pinname": ["Courier", "Neighbour"],
      "pinvalue": ["12345678", "87654321"],
      "datefrom": ["2026-09-01", "0001-01-01"],
      "dateto": ["2026-10-01", "9999-12-31"],
      "amount": ["5", "-1"]
    },
    "rowCount": 2
  }
}
```

Code example uses corresponding fields:

- `codeid`
- `codename`
- `codevalue`
- `codefrom`
- `codeto`

and does not carry the PIN usage counter.

The column-array form is the current wire representation. The UI constructs its own table/widgets from it.

### 8.3 `set_cred`

**Direction:** UI → Box

`set_cred` is the generic mutation command. `credType` selects the concrete record schema.

#### PIN create/update example

```json
{
  "_command_": "set_cred",
  "_timestamp_": 1790784000,
  "data": {
    "credType": "ctPin",
    "pinid": "0",
    "pinname": "Courier",
    "pinvalue": "12345678",
    "datefrom": "2026-09-30",
    "dateto": "2026-10-31",
    "checkbox-unlimited": true,
    "amount": "-1",
    "doornum": 0,
    "clicked": "savebutton"
  }
}
```

#### Parcel/order-code create/update example

```json
{
  "_command_": "set_cred",
  "_timestamp_": 1790784000,
  "data": {
    "credType": "ctCode",
    "codeid": "0",
    "codename": "Order 123",
    "codevalue": "HZ1268956754M",
    "codefrom": "2026-09-30",
    "codeto": "2026-10-31",
    "doornum": 0,
    "clicked": "savebutton"
  }
}
```

Common action semantics:

- `clicked = "savebutton"`, ID `0` → create;
- `clicked = "savebutton"`, non-zero ID → update;
- `clicked = "deletebutton"`, non-zero ID → delete.

Current controller result fields include:

```json
{
  "success": true,
  "credType": "ctPin",
  "credId": 17,
  "action": "created"
}
```

Possible action values are `created`, `updated`, `deleted`.

Type-specific behaviour:

- PIN supports `amount` / `checkbox-unlimited`;
- code credentials use their own value/date fields and currently default to one remaining use internally;
- an optional `doornum` maps the credential to a logical door;
- valid-from / valid-to are calendar dates interpreted in UTC by the protocol parser;
- empty/open-ended limits are represented internally with zero and on the wire using the established sentinel dates `0001-01-01` / `9999-12-31` where formatted data is returned.

### 8.4 Credential-change notification

After a successful change the controller notifies connected clients that the affected credential set changed:

```json
{
  "_command_": "c_setcred",
  "_timestamp_": 1790784000,
  "data": {
    "credType": "ctPin",
    "changed": true
  }
}
```

The intended client behaviour is **not** to accept a controller-rendered table. Each client reloads the page/range it currently displays, preserving its own credential type and pagination state.

This is the foundation for the future paging handshake.

## 9. `c_enordis` — operation availability

**Direction:** Box → UI

`c_enordis` is intentionally retained. Unlike the removed HTML-mutating commands, it represents an application rule: whether the client should allow the user to invoke a logical operation.

It must not address DOM element IDs.

### 9.1 Current implementation

The currently implemented use only controls the door-open operation. The current firmware sends a logical operation key based on `opendoor` and currently uses `disable` versus `open` as the values.

Conceptually:

```json
{
  "_command_": "c_enordis",
  "data": {
    "opendoor": "disable"
  }
}
```

or the corresponding enabled/open state.

The web UI is responsible for translating that operation state into whichever concrete controls must be disabled. A native client may represent the same operation differently.

### 9.2 Target semantics

The long-term form should use stable functional-area/operation identifiers, for example:

```json
{
  "_command_": "c_enordis",
  "data": {
    "door_controls": "disable",
    "cred_changes": "enable"
  }
}
```

Recommended value vocabulary for the generalized form is strictly:

- `enable`
- `disable`

The present `open` token should therefore be regarded as a transitional implementation detail when the generalized contract is introduced.

### 9.3 Future ticket/lease mechanism

Future multi-client editing/control requires more than presentation-level disabling. Planned behaviour:

1. a client requests a ticket/lease for a protected action or functional area;
2. the first successful requester receives the ticket;
3. other clients receive `c_enordis` updates disabling the conflicting operation;
4. the controller validates the ticket when the protected operation is executed;
5. tickets expire automatically so a crashed browser/app cannot permanently lock the box;
6. controller-originated activities may similarly reserve a functional area and disable conflicting UI actions.

This mechanism is **not specified or implemented in revision 2**. `c_enordis` currently provides only the operation-availability notification needed by the existing door state machine.

## 10. Human-readable operation status

The current controller separates human-readable status from structured responses using:

```json
{
  "_command_": "c_lastresult",
  "data": {
    "lastresult": "Credential saved"
  }
}
```

This replaces the former `c_content.lastresult` pattern and keeps the protocol independent of presentation.

A rename/new command `c_setlaststatus` has been proposed. It is **not treated as current in this revision** until implemented consistently in controller and clients. If adopted, it should preserve the same principle: semantic status data, no DOM target.

## 11. Diagnostics

Diagnostics already use domain-specific responses and therefore fit the UI-independent model.

### 11.1 `get_diagnostics` → `c_diagnostics`

Request:

```json
{
  "_command_": "get_diagnostics",
  "data": {}
}
```

Response currently contains diagnostic representations such as:

- `html`
- `text`
- `json`

Although the `html` representation remains useful for diagnostics tooling, it is payload data explicitly requested as a format; it is not an instruction to mutate the main UI.

### 11.2 `snapshot_diagnostics` / `get_diagnostic_snapshots`

Responses use `c_diagnostic_snapshots` and may include:

- `count`
- `html`
- `text`
- `json`

A snapshot operation also reports its human-readable result separately through the last-result mechanism.

## 12. Watchdog

The application watchdog uses `_ping_` / `_pong_` over WebSocket.

Controller:

```json
{
  "_command_": "_ping_",
  "_timestamp_": 1790784000,
  "data": {
    "ping_data": "ping"
  }
}
```

Client response:

```json
{
  "_command_": "_pong_",
  "_timestamp_": 1790784000,
  "data": {}
}
```

The payload content of `_pong_` is not semantically significant; receipt feeds the controller-side client watchdog.

The watchdog's internal timeout arithmetic uses controller uptime, which is deliberately separate from the epoch timestamp carried by the application envelope.

## 13. Logging

### `c_log`

Logging uses the same general envelope but a separate log transport/destination.

```json
{
  "_command_": "c_log",
  "_timestamp_": 1790784000,
  "data": {
    "source": "BOX-007",
    "area": 5,
    "severity": 2,
    "message": "...",
    "timestamp": 1790784000
  }
}
```

The nested event timestamp describes the log event. Numeric area/severity values are defined by the logger/configuration and should be documented from those definitions rather than frozen independently here.

MQTT remains a possible future log transport and is outside the current WebSocket UI protocol.

## 14. Controller state machine

The communication protocol interacts with an event-driven controller state machine. Read-only requests (`get_door`, `get_ambient`, `get_creds`, diagnostics) do not themselves change controller state. `opendoor` creates a `WebOpenBox` event.

### 14.1 States

| State | Meaning |
|---|---|
| `Home` | idle/home state; normal external door control available |
| `Password` | local keyboard PIN/password entry |
| `BadPass` | bad-password penalty period |
| `Presence` | presence code displayed/valid |
| `Opening` | valid open request accepted; lock activation/opening in progress |
| `Open` | requested door is physically open |
| `Closed` | requested door has closed; post-close ambient interval still active |
| `External` | reserved state for an external command/ownership model |

### 14.2 Principal transitions

```text
                         local Key1
                    +-----------------> Password
                    |                       |
                    |                       | valid credential
                    |                       v
                    |                    Opening
                    |                       |
                    |                       | requested door opens
                    |                       v
Home ---------------+--------------------> Open
 |                  WebOpenBox               |
 |                                            | requested door closes
 | local Key2                                 v
 v                                          Closed
Presence                                      |
 |                                            | ambient timeout
 | cancel / expiry                            v
 +------------------------------------------> Home

Password -- invalid credential --> BadPass -- penalty expires --> Password
Password -- cancel/timeout -------------------------------------> Home
Presence -- cancel/expiry --------------------------------------> Home
Opening -- opening timeout/failure -----------------------------> Home
```

The diagram is intentionally conceptual: both local keyboard authentication and WebSocket open requests converge on the same `Opening → Open → Closed → Home` physical-door sequence.

### 14.3 State entry effects relevant to protocol

- Entering `Home` enables external door control.
- Entering `Password` disables external door control.
- Entering `Opening` disables external door control, activates the requested lock and turns ambient lighting on.
- Entering `Open` starts camera-related behaviour where enabled and consumes a locally verified credential as appropriate.
- Entering `Closed` starts the ambient-off delay and clears the active door target.
- Leaving `Closed` for `Home` turns ambient lighting off.

Door sensor changes are reported through `c_doorstate`; ambient changes are reported through `c_ambientstate`. The state machine therefore governs physical sequencing, while the UI decides how reported state is presented.

### 14.4 UI-operation locking

The current state-machine callback disables `opendoor` while external opening is unsafe and re-enables it in `Home`. This is the present use of `c_enordis`.

The future ticket/lease mechanism described above should extend this concept rather than embedding HTML-specific control logic into the state machine.

## 15. Paging and synchronization roadmap

Credential paging is intentionally deferred until QR/parcel-code integration is complete.

The desired model is a handshake, not controller-pushed rendered tables:

1. client asks for a credential type and a page/range;
2. controller returns structured `c_setcred` data for that range;
3. when the credential set changes, controller broadcasts a lightweight change notification;
4. every client requests the range it currently needs;
5. paging metadata must eventually define ordering, start anchor, count returned and end-of-list indication.

`credId` and `credCount` already reserve the basic request shape, but the final paging contract is not frozen in revision 2.

## 16. Door configuration roadmap

The protocol already permits logical door numbers up to `254` and the HAL/state model anticipates grouped logical doors.

Full multi-door support still requires:

- persistent controller-side door configuration;
- UI door editor;
- one or more door controls in UI rather than a single fixed button;
- deliberate representation of `DOOR_MIXED`;
- validation tests for door identifiers and mappings.

Until then, door `0` is simply the first door; it has no special protocol meaning.

## 17. Open decisions / TODOs

The following are intentionally unresolved rather than undocumented accidents:

1. **`_scope_`** — ignored and likely removable; decision deferred.
2. **Credential paging handshake** — planned after scanner integration.
3. **Credential change notification details** — current `changed:true` is usable, final paging-aware contract remains to be frozen.
4. **Operation-state synchronization on client connect** — successor to the old `get_status`; final request/response command not specified.
5. **`c_enordis` generalized vocabulary** — migrate current door-only `opendoor: disable/open` form to stable functional keys and `enable/disable` values.
6. **Ticket/lease ownership** for conflicting multi-client operations — planned, not implemented.
7. **`c_lastresult` vs `c_setlaststatus` naming** — current implementation uses `c_lastresult`; proposed naming change should be made only as an explicit protocol migration.
8. **`DOOR_MIXED` external representation** — intentionally deferred until real grouped-door configuration/UI exists.
9. **Identifier tests** — add formal firmware tests for valid logical door/light IDs.
10. **Timestamp integration test** — assert `_timestamp_` is Unix epoch time in seconds across controller, UI and mock.
11. **Timeout helper cleanup** — replace the remaining direct watchdog timeout comparison in `WebSocketManager::cleanupConnections()` with `timeoutElapsed()`.

$$$ It is necessary to expect that any credencial report is going to be extended by logical number field

## Appendix A — concise current command map

```text
UI/client -> Box
    opendoor
    get_door
    get_ambient
    get_creds
    set_cred
    get_diagnostics
    snapshot_diagnostics
    get_diagnostic_snapshots
    _pong_

Box -> UI/client
    c_doorstate
    c_ambientstate
    c_setcred
    c_lastresult
    c_diagnostics
    c_diagnostic_snapshots
    c_enordis
    _ping_

Box -> log transport
    c_log
```

## Appendix B — credential type map

```text
ctPin   PIN/password credential
ctCode  parcel/order-number credential
```

## Appendix C — compatibility/retirement list

```text
c_content       retired from protocol; temporary compatibility code may remain
c_visibility    retired from protocol; temporary compatibility code may remain
get_pins        superseded by get_creds
set_pin         superseded by set_cred + credType=ctPin
set_code        superseded by set_cred + credType=ctCode
get_PinInfo     obsolete cleanup candidate
get_pager       old placeholder; future paging will be generic
get_status      old placeholder; future operation-state synchronization TBD
```
