Credential protocol
===================

The JSON envelope retains `_command_`, `_timestamp_` and `data` (underscores,
not literal asterisks). UI timestamps are milliseconds; device timestamps remain
Unix seconds. `get_pins`, `set_pin` and the unused `get_pager` handler are removed.

List active credentials
-----------------------

```json
{"_timestamp_":1790428038680,"_command_":"get_creds","data":{"credId":0,"credCount":15,"credType":"ctPin"}}
```

Response command: `c_setcred`. Its data has the column-array schema:

```json
{"credType":"ctPin","rows":{"pinid":[1],"pinname":["Kule"],"pinvalue":["49786543"],"datefrom":["0001-01-01"],"dateto":["9999-12-31"],"amount":["3"]},"rowCount":1}
```

For `ctCode`, columns are `codeid`, `codename`, `codevalue`, `codefrom`, `codeto`.
IDs are numbers; the remaining columns are strings. Every column is present even
when empty, and all column lengths equal rowCount. `credId` is an inclusive lower
ID bound, not a row offset. Defaults are ID 0 / count 15. Count is 0..100; invalid
pagination is rejected. The transport accepts only ctPin or ctCode; the storage
API separately supports CredType::All. Inactive records are excluded.

Create, update, delete
---------------------

```json
{"_timestamp_":1790437945143,"_command_":"set_cred","data":{"pinid":"0","credType":"ctPin","pinname":"Kule","pinvalue":"49786543","amount":"3","checkbox-unlimited":false,"datefrom":"0001-01-01","dateto":"9999-12-31","clicked":"savebutton"}}
```

```json
{"_timestamp_":1790437945143,"_command_":"set_cred","data":{"codeid":"0","credType":"ctCode","codename":"Tleskac","codevalue":"HZ1268956754M","codefrom":"0001-01-01","codeto":"9999-12-31","clicked":"savebutton"}}
```

ID 0 creates a record; a nonzero ID updates it. Delete uses the same identity
fields with `clicked: "deletebutton"` and a nonzero ID. Type, name and complete
credential text must match the stored record for updates/deletes. UI identity
fields remain disabled for existing records but are included in the request.
Numeric fields accept JSON integers or decimal strings. Overlong text is rejected
without truncation. The global packet limits remain 8 / 20 / 8 in config.h.

New packet credentials have one use. Updates preserve their remaining count.
An omitted door defaults to 0 for creation and retains the stored door on update.
Passwords use amount, with checkbox-unlimited=true taking precedence and storing
-1. If checkbox-unlimited=false, amount must be a nonnegative integer.

Dates are ISO calendar dates interpreted in UTC: validFrom starts at 00:00:00,
validTo includes the entire day (23:59:59). An empty date, lower `0001-01-01` or
upper `9999-12-31` means an unbounded endpoint (stored as 0). Omitted dates remain
unchanged on update and unbounded on creation. Invalid dates and reversed ranges
are rejected. Ordinary dates before 1970 are not supported.

The UI displays unbounded endpoints as "Neomezeno" and leaves the corresponding
date input empty, so the native calendar can start at the current date. The raw
wire value remains attached to the table cell. Saving an empty input sends the
appropriate 0001-01-01 / 9999-12-31 endpoint; choosing a date limits validity and
clearing it restores unlimited validity. This does not change NVS representation:
unbounded endpoints are still stored as zero in the firmware.

Mutation acknowledgements also use c_setcred:

```json
{"credType":"ctCode","success":true,"credId":2,"action":"created"}
```

All clients then receive a small c_setcred invalidation message:

```json
{"credType":"ctCode","changed":true}
```

Each client requests its current page again using get_creds. The UI keeps one
page selection per type, so edits do not replace another client's pagination.
A response without `rows` is an acknowledgement, notification or error, not an
empty table. Error responses use `success:false` and `error`:

```json
{"credType":"ctCode","success":false,"error":"invalid_date_range"}
```

Human-readable status is sent separately, before the structured response, using
`c_lastresult` for credentials, door operations and diagnostic snapshots:

```json
{"_command_":"c_lastresult","_timestamp_":1790437945,"data":{"lastresult":"Credential saved"}}
```

The UI replaces the innerHTML of `#lastresult`; an empty string clears it.
Diagnostic content uses `c_diagnostics` (`html`, `text`, `json`) and
`c_diagnostic_snapshots` (`count`, `html`, optionally `text` and `json`).
The `json` field retains the serialized JSON string returned by diagnostics.
`get_diagnostics` returns the former; both `snapshot_diagnostics` and
`get_diagnostic_snapshots` return the latter. Creating a snapshot sends only
`count` and `html`, after its separate `c_lastresult` status.
Only the UI maps these fields to DOM IDs; missing diagnostic elements are ignored.
Firmware no longer sends `c_content`.

`c_enordis` addresses operations, not DOM IDs. Entering password entry sends
`{"opendoor":"disable"}` in `data`; returning to Home sends `{"opendoor":"open"}`.
The existing disable notification during door opening uses the same operation key.
The UI disables both the Open Box button and its confirmation button, including
when the confirmation dialog is already open. Unknown operations and permission
values are ignored. This does not introduce operation tickets or edit locks.

The transport-independent
credential handlers return lastresult internally; main.cpp extracts it into
the dedicated message before sending the remaining structured response.

Examples rejected without changing storage:

```json
{"_command_":"set_cred","_timestamp_":1790437945143,"data":{"credType":"ctCode","codeid":0,"codename":"Tleskac","codevalue":"HZ1268956754M","codefrom":"2026-02-30","clicked":"savebutton"}}
{"_command_":"get_creds","_timestamp_":1790437945143,"data":{"credType":"unknown","credId":0,"credCount":15}}
```

Implementation and checks
-------------------------

main.cpp only delivers requests/responses; cred_protocol.cpp contains the shared,
native-testable parsers and handlers. pass_store returns records, never HTML.
Generic names use CredStorage, CredRecord, CredType, credId and credType.
The binary layout, CredentialV1 discriminator, NVS namespace `pins`, write order
and recovery algorithms are retained; no reset is caused by the API rename.
The existing temporary reset for older, smaller record formats remains.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File test/run_cred_protocol.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File test/run_pass_store_types.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File test/run_timeout_rollover.ps1
pio run -e esp32-s3-n16r16
```

`prepare_cred_ui.py` generates `.pio/build/cred-ui/test.html` from the actual UI
and scripts, replacing WebSocket transport with an in-memory message log.
Open it in a browser (or use headless Chrome with `--dump-dom`). The body attribute
`data-test-result` must begin with PASS. Tests cover both dialogs, command fields,
row rendering/escaping, error handling, empty tables and pagination on refresh.
`run_pass_store_console.ps1` accepts get_creds / set_cred messages and preserves
support for the existing NVS JSON seeds.
