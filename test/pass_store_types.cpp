#include <cassert>
#include <cstring>
#include <iostream>
#include "pass_store.h"
#include "gpio_hal.h"
#include "logger.h"

// Storage tests do not need a log transport.
Logger logger;
void Logger::logPrint(uint8_t, const String&, const char*, uint8_t) {}

static CacheRecord credential(CredType type, const std::string& value, uint8_t door) {
    CacheRecord rec = {};
    rec.credType = type;
    std::strcpy(rec.name, "test");
    if (type == CredType::PacketNumber) std::strcpy(rec.packetNumber, value.c_str());
    else std::strcpy(rec.pin, value.c_str());
    rec.doorNum = door;
    rec.remaining = -1;
    return rec;
}

static Preferences storage() {
    Preferences prefs;
    assert(prefs.begin("pins"));
    return prefs;
}

static CredRecord read(uint32_t id) {
    char key[9];
    std::snprintf(key, sizeof(key), "%08X", id);
    CredRecord rec;
    assert(storage().getBytes(key, &rec, sizeof(rec)) == sizeof(rec));
    return rec;
}

static void write(const CredRecord& rec) {
    char key[9];
    std::snprintf(key, sizeof(key), "%08X", rec.credId);
    assert(storage().putBytes(key, &rec, sizeof(rec)) == sizeof(rec));
}

int main() {
    CacheRecord defaultRec;
    CredRecord defaultStored;
    assert(defaultRec.credType == CredType::Password);
    assert(defaultStored.credType == CredType::Password);
    assert(defaultStored.recordType == RecordType::CredentialV1);
    assert(CacheRecord{}.credType == CredType::Password);
    Preferences::clearAll();
    CredStorage creds;
    assert(creds.begin());

    auto password = credential(CredType::Password, "12345678", 1);
    password.credId = creds.addCred(password);
    assert(password.credId);
    const std::string packetText(PACKET_NUMBER_MAX, 'A');
    auto packet = credential(CredType::PacketNumber, packetText, 2);
    packet.credId = creds.addCred(packet);
    assert(packet.credId);
    std::vector<CacheRecord> table;
    assert(creds.getCreds(0, 10, table) == 1);
    assert(creds.getCreds(0, 1, table, CredType::PacketNumber) == 1);
    assert(std::string(table[0].credential()) == packetText);
    assert(creds.getCreds(0, 10, table, CredType::All) == 2);
    assert(creds.getCreds(packet.credId, 10, table, CredType::Password) == 0);
    assert(creds.getCreds(0, 0, table, CredType::All) == 0);
    assert(creds.getCreds(0, 10, table, static_cast<CredType>(99)) == 0);
    assert(creds.verifyCred("12345678") == 1);
    assert(creds.usedCred(1));
    assert(creds.verifyCred("1234567") == DOOR_UNKNOWN);
    assert(creds.verifyCred(packetText.c_str()) == DOOR_UNKNOWN);
    assert(creds.verifyCred(packetText.c_str(), CredType::All) == DOOR_UNKNOWN);
    assert(creds.verifyCred(nullptr) == DOOR_UNKNOWN);
    std::string scanned = packetText;
    if (PACKET_NUMBER_MATCH < PACKET_NUMBER_MAX) scanned.back() = 'B';
    assert(creds.verifyCred(scanned.c_str(), CredType::PacketNumber) == 2);
    assert(creds.usedCred(2));
    scanned[0] = 'C';
    assert(creds.verifyCred(scanned.c_str(), CredType::PacketNumber) == DOOR_UNKNOWN);

    auto bad = packet;
    bad.credType = CredType::All;
    assert(!creds.addCred(bad));
    bad = password;
    bad.pin[0] = 'X';
    assert(!creds.addCred(bad));
    bad = packet;
    std::memset(bad.packetNumber, 'A', sizeof(bad.packetNumber));
    assert(!creds.addCred(bad));
    const std::string shortPacket(PACKET_NUMBER_MIN - 1, 'A');
    assert(!creds.addCred(credential(CredType::PacketNumber, shortPacket, 2)));
    assert(creds.verifyCred((packetText + "A").c_str(), CredType::PacketNumber) == DOOR_UNKNOWN);
    auto minimum = credential(CredType::PacketNumber, std::string(PACKET_NUMBER_MIN, 'Z'), 5);
    minimum.credId = creds.addCred(minimum);
    assert(minimum.credId);
    assert(creds.removeCred(minimum) == minimum.credId);
    auto escaped = credential(CredType::PacketNumber, packetText, 5);
    escaped.packetNumber[0] = '<';
    escaped.credId = creds.addCred(escaped);
    assert(escaped.credId);
    assert(creds.getCreds(escaped.credId, 1, table, CredType::PacketNumber) == 1);
    assert(table[0].credential()[0] == '<');
    assert(creds.removeCred(escaped) == escaped.credId);
    auto expired = credential(CredType::PacketNumber, packetText, 5);
    expired.validTo = 1;
    expired.credId = creds.addCred(expired);
    assert(expired.credId);
    assert(creds.removeCred(expired) == expired.credId);
    bad = packet;
    bad.validFrom = 2;
    bad.validTo = 1;
    assert(!creds.addCred(bad));

    // Identical text must never allow a different type to update/delete a password.
    if (PACKET_NUMBER_MIN <= 8 && PACKET_NUMBER_MAX >= 8) {
        auto other = credential(CredType::PacketNumber, "12345678", 3);
        other.credId = password.credId;
        assert(!creds.updateCred(other));
        assert(!creds.removeCred(other));
        assert(creds.verifyCred("12345678") == 1);
        assert(creds.usedCred(1));
        other.credId = creds.addCred(other);
        assert(other.credId);
        assert(creds.verifyCred("12345678", CredType::PacketNumber) == 3);
        assert(creds.usedCred(3));
        assert(creds.removeCred(other) == other.credId);
    }

    packet.remaining = 2;
    assert(creds.updateCred(packet));
    assert(creds.verifyCred(packetText.c_str(), CredType::PacketNumber) == 2);
    assert(read(packet.credId).remaining == 2);
    assert(creds.usedCred(2));
    assert(read(packet.credId).remaining == 1);
    CredStorage restarted;
    assert(restarted.begin());
    assert(restarted.getCreds(0, 10, table, CredType::All) == 2);
    assert(std::strcmp(read(packet.credId).packetNumber, packetText.c_str()) == 0);
    assert(read(packet.credId).remaining == 1);
    assert(restarted.verifyCred(packetText.c_str(), CredType::PacketNumber) == 2);
    assert(restarted.usedCred(2));
    assert(restarted.verifyCred(packetText.c_str(), CredType::PacketNumber) == DOOR_UNKNOWN);
    assert(restarted.verifyCred("12345678") == 1);
    assert(restarted.usedCred(1));
    for (const auto& entry : Preferences::dumpNamespace("pins")) {
        assert(entry.second.size() == sizeof(CredRecord));
        assert(entry.second[0] == static_cast<uint8_t>(RecordType::CredentialV1));
    }

    // Simulate interrupted insert steps 1 and 2 for each credential type.
    for (CredType type : {CredType::Password, CredType::PacketNumber}) {
        for (int step : {1, 2}) {
            Preferences::clearAll();
            CredStorage initial;
            initial.begin();
            auto rec = credential(type, type == CredType::Password ? "12345678" : packetText, 4);
            rec.credId = initial.addCred(rec);
            assert(rec.credId == 1);
            auto tail = read(LAST_KEY);
            tail.prevId = FIRST_KEY;
            tail.nextId = 1;
            write(tail);
            if (step == 1) storage().remove("00000001");
            CredStorage recovered;
            recovered.begin();
            assert(recovered.getCreds(0, 10, table, CredType::All) == (step == 1 ? 0 : 1));
        }
        // Interrupted deletion before/after removal of the target blob.
        for (int step : {1, 2}) {
            Preferences::clearAll();
            CredStorage initial;
            initial.begin();
            auto rec = credential(type, type == CredType::Password ? "12345678" : packetText, 4);
            rec.credId = initial.addCred(rec);
            auto head = read(FIRST_KEY);
            head.nextId = LAST_KEY;
            write(head);
            if (step == 2) storage().remove("00000001");
            CredStorage recovered;
            recovered.begin();
            assert(recovered.getCreds(0, 10, table, CredType::All) == 0);
            assert(read(LAST_KEY).prevId == FIRST_KEY);
        }
    }

    // Temporary upgrade reset clears only pins, not unrelated configuration.
    Preferences other;
    other.begin("settings");
    const uint8_t marker = 42;
    other.putBytes("keep", &marker, 1);
    const uint8_t oldHead[56] = {};
    static_assert(sizeof(oldHead) < sizeof(CredRecord), "Test needs the larger new layout");
    storage().putBytes(FIRST_KEY_KEY, oldHead, sizeof(oldHead));
    storage().putBytes("obsolete", oldHead, sizeof(oldHead));
    CredStorage upgraded;
    assert(upgraded.begin());
    assert(Preferences::dumpNamespace("pins").size() == 2);
    assert(other.isKey("keep"));
    assert(read(FIRST_KEY).nextId == LAST_KEY);
    assert(read(LAST_KEY).prevId == FIRST_KEY);

    // Unknown record format must not be accepted as the current format.
    auto head = read(FIRST_KEY);
    head.recordType = static_cast<RecordType>(99);
    write(head);
    CredStorage invalid;
    invalid.begin();
    assert(read(FIRST_KEY).recordType == RecordType::CredentialV1);
    std::cout << "PASS: credential types, persistence, recovery and legacy reset\n";
}
