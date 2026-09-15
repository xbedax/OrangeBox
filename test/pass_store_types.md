Pass store credential types
===========================

Run native regression tests on Windows with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File test/run_pass_store_types.ps1
```

The tests use the real pass_store implementation with in-memory Preferences.
They cover default password initialization, type isolation, packet length limits,
prefix matching, pagination filters, update/delete, use counts across restart,
interrupted insertion/deletion for both types, and the temporary old-format reset.

API
---

Existing callers can keep writing `rec.pin` and omit the type: both record structs
initialize `pinType` to `PinType::Password` even with `CacheRecord rec;`.
Do not zero the entire object with memset; zero is a query filter, not a credential.
For packet numbers use:

```cpp
CacheRecord rec{};
rec.pinType = PinType::PacketNumber;
strcpy(rec.packetNumber, "AB1234567890"); // Must fit PACKET_NUMBER_MAX + 1.
strcpy(rec.name, "delivery");
rec.doorNum = 0;
rec.remaining = 1;
uint32_t id = pinStorage.addPin(rec);

uint8_t door = pinStorage.verifyPin("AB1234567890", PinType::PacketNumber);
bool consumed = pinStorage.usedPin(door);
String table;
pinStorage.getPins(0, 10, table, PinType::PacketNumber);
pinStorage.getPins(0, 10, table, PinType::All);
```

Record-taking APIs use the type in the record. Updates and deletes require the
same type and complete credential text; only verifyPin matches packet prefixes. A verified credential is consumed by usedPin after the door opens.
Passwords retain exact numeric matching. Packet numbers accept visible ASCII
characters (without whitespace), preserve leading zeroes and are case-sensitive.
The default MIN / MAX / MATCH values are provisionally 8 / 64 / 8 in config.h.
Lengths exclude the terminating NUL; MATCH must fit the minimum accepted length.
getPins, verifyPin and usedPin default to Password semantics; All (0) is accepted only by getPins.

Storage
-------

The first byte of every persisted PinRecord, including HEAD and TAIL, is
RecordType::CredentialV1. pinType is a separate field. All blob reads and writes
go through readData/writeData. readData requires the current size and record type.
The current version still stores the native struct layout; future layout changes
must introduce a new record format and corresponding decoding.

PinStorage::begin contains a marked TEMPORARY block: an existing HEAD smaller
than sizeof(PinRecord) clears only the pins namespace before normal recovery.
There is no migration of old test data. Remove that block after deployment.
Existing chain-repair algorithms and write ordering are retained.
