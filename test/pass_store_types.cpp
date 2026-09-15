#include <cassert>
#include <cstring>
#include <iostream>
#include "pass_store.h"
#include "gpio_hal.h"
#include "logger.h"

// Storage tests do not need a log transport.
Logger logger;
void Logger::logPrint(uint8_t, const String&, const char*, uint8_t) {}

static CacheRecord credential(PinType type, const std::string& value, uint8_t door) {
    CacheRecord rec = {};
    rec.pinType = type;
    std::strcpy(rec.name, "test");
    if (type == PinType::PacketNumber) std::strcpy(rec.packetNumber, value.c_str());
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

static PinRecord read(uint32_t id) {
    char key[9];
    std::snprintf(key, sizeof(key), "%08X", id);
    PinRecord rec;
    assert(storage().getBytes(key, &rec, sizeof(rec)) == sizeof(rec));
    return rec;
}

static void write(const PinRecord& rec) {
    char key[9];
    std::snprintf(key, sizeof(key), "%08X", rec.pinId);
    assert(storage().putBytes(key, &rec, sizeof(rec)) == sizeof(rec));
}

int main() {
    CacheRecord defaultRec;
    PinRecord defaultStored;
    assert(defaultRec.pinType == PinType::Password);
    assert(defaultStored.pinType == PinType::Password);
    assert(defaultStored.recordType == RecordType::CredentialV1);
    assert(CacheRecord{}.pinType == PinType::Password);
    Preferences::clearAll();
    PinStorage pins;
    assert(pins.begin());

    auto password = credential(PinType::Password, "12345678", 1);
    password.pinId = pins.addPin(password);
    assert(password.pinId);
    const std::string packetText(PACKET_NUMBER_MAX, 'A');
    auto packet = credential(PinType::PacketNumber, packetText, 2);
    packet.pinId = pins.addPin(packet);
    assert(packet.pinId);
    String table;
    assert(pins.getPins(0, 10, table) == 1);
    assert(pins.getPins(0, 1, table, PinType::PacketNumber) == 1);
    assert(std::string(table.c_str()).find(packetText) != std::string::npos);
    assert(pins.getPins(0, 10, table, PinType::All) == 2);
    assert(pins.getPins(packet.pinId, 10, table, PinType::Password) == 0);
    assert(pins.getPins(0, 0, table, PinType::All) == 0);
    assert(pins.getPins(0, 10, table, static_cast<PinType>(99)) == 0);
    assert(pins.verifyPin("12345678") == 1);
    assert(pins.usedPin(1));
    assert(pins.verifyPin("1234567") == DOOR_UNKNOWN);
    assert(pins.verifyPin(packetText.c_str()) == DOOR_UNKNOWN);
    assert(pins.verifyPin(packetText.c_str(), PinType::All) == DOOR_UNKNOWN);
    assert(pins.verifyPin(nullptr) == DOOR_UNKNOWN);
    std::string scanned = packetText;
    if (PACKET_NUMBER_MATCH < PACKET_NUMBER_MAX) scanned.back() = 'B';
    assert(pins.verifyPin(scanned.c_str(), PinType::PacketNumber) == 2);
    assert(pins.usedPin(2));
    scanned[0] = 'C';
    assert(pins.verifyPin(scanned.c_str(), PinType::PacketNumber) == DOOR_UNKNOWN);

    auto bad = packet;
    bad.pinType = PinType::All;
    assert(!pins.addPin(bad));
    bad = password;
    bad.pin[0] = 'X';
    assert(!pins.addPin(bad));
    bad = packet;
    std::memset(bad.packetNumber, 'A', sizeof(bad.packetNumber));
    assert(!pins.addPin(bad));
    const std::string shortPacket(PACKET_NUMBER_MIN - 1, 'A');
    assert(!pins.addPin(credential(PinType::PacketNumber, shortPacket, 2)));
    assert(pins.verifyPin((packetText + "A").c_str(), PinType::PacketNumber) == DOOR_UNKNOWN);
    auto minimum = credential(PinType::PacketNumber, std::string(PACKET_NUMBER_MIN, 'Z'), 5);
    minimum.pinId = pins.addPin(minimum);
    assert(minimum.pinId);
    assert(pins.removePin(minimum) == minimum.pinId);
    auto escaped = credential(PinType::PacketNumber, packetText, 5);
    escaped.packetNumber[0] = '<';
    escaped.pinId = pins.addPin(escaped);
    assert(escaped.pinId);
    assert(pins.getPins(escaped.pinId, 1, table, PinType::PacketNumber) == 1);
    assert(std::string(table.c_str()).find("&lt;") != std::string::npos);
    assert(pins.removePin(escaped) == escaped.pinId);
    auto expired = credential(PinType::PacketNumber, packetText, 5);
    expired.validTo = 1;
    expired.pinId = pins.addPin(expired);
    assert(expired.pinId);
    assert(pins.removePin(expired) == expired.pinId);
    bad = packet;
    bad.validFrom = 2;
    bad.validTo = 1;
    assert(!pins.addPin(bad));

    // Identical text must never allow a different type to update/delete a password.
    if (PACKET_NUMBER_MIN <= 8 && PACKET_NUMBER_MAX >= 8) {
        auto other = credential(PinType::PacketNumber, "12345678", 3);
        other.pinId = password.pinId;
        assert(!pins.updatePin(other));
        assert(!pins.removePin(other));
        assert(pins.verifyPin("12345678") == 1);
        assert(pins.usedPin(1));
        other.pinId = pins.addPin(other);
        assert(other.pinId);
        assert(pins.verifyPin("12345678", PinType::PacketNumber) == 3);
        assert(pins.usedPin(3));
        assert(pins.removePin(other) == other.pinId);
    }

    packet.remaining = 2;
    assert(pins.updatePin(packet));
    assert(pins.verifyPin(packetText.c_str(), PinType::PacketNumber) == 2);
    assert(read(packet.pinId).remaining == 2);
    assert(pins.usedPin(2));
    assert(read(packet.pinId).remaining == 1);
    PinStorage restarted;
    assert(restarted.begin());
    assert(restarted.getPins(0, 10, table, PinType::All) == 2);
    assert(std::strcmp(read(packet.pinId).packetNumber, packetText.c_str()) == 0);
    assert(read(packet.pinId).remaining == 1);
    assert(restarted.verifyPin(packetText.c_str(), PinType::PacketNumber) == 2);
    assert(restarted.usedPin(2));
    assert(restarted.verifyPin(packetText.c_str(), PinType::PacketNumber) == DOOR_UNKNOWN);
    assert(restarted.verifyPin("12345678") == 1);
    assert(restarted.usedPin(1));
    for (const auto& entry : Preferences::dumpNamespace("pins")) {
        assert(entry.second.size() == sizeof(PinRecord));
        assert(entry.second[0] == static_cast<uint8_t>(RecordType::CredentialV1));
    }

    // Simulate interrupted insert steps 1 and 2 for each credential type.
    for (PinType type : {PinType::Password, PinType::PacketNumber}) {
        for (int step : {1, 2}) {
            Preferences::clearAll();
            PinStorage initial;
            initial.begin();
            auto rec = credential(type, type == PinType::Password ? "12345678" : packetText, 4);
            rec.pinId = initial.addPin(rec);
            assert(rec.pinId == 1);
            auto tail = read(LAST_KEY);
            tail.prevId = FIRST_KEY;
            tail.nextId = 1;
            write(tail);
            if (step == 1) storage().remove("00000001");
            PinStorage recovered;
            recovered.begin();
            assert(recovered.getPins(0, 10, table, PinType::All) == (step == 1 ? 0 : 1));
        }
        // Interrupted deletion before/after removal of the target blob.
        for (int step : {1, 2}) {
            Preferences::clearAll();
            PinStorage initial;
            initial.begin();
            auto rec = credential(type, type == PinType::Password ? "12345678" : packetText, 4);
            rec.pinId = initial.addPin(rec);
            auto head = read(FIRST_KEY);
            head.nextId = LAST_KEY;
            write(head);
            if (step == 2) storage().remove("00000001");
            PinStorage recovered;
            recovered.begin();
            assert(recovered.getPins(0, 10, table, PinType::All) == 0);
            assert(read(LAST_KEY).prevId == FIRST_KEY);
        }
    }

    // Temporary upgrade reset clears only pins, not unrelated configuration.
    Preferences other;
    other.begin("settings");
    const uint8_t marker = 42;
    other.putBytes("keep", &marker, 1);
    const uint8_t oldHead[56] = {};
    static_assert(sizeof(oldHead) < sizeof(PinRecord), "Test needs the larger new layout");
    storage().putBytes(FIRST_KEY_KEY, oldHead, sizeof(oldHead));
    storage().putBytes("obsolete", oldHead, sizeof(oldHead));
    PinStorage upgraded;
    assert(upgraded.begin());
    assert(Preferences::dumpNamespace("pins").size() == 2);
    assert(other.isKey("keep"));
    assert(read(FIRST_KEY).nextId == LAST_KEY);
    assert(read(LAST_KEY).prevId == FIRST_KEY);

    // Unknown record format must not be accepted as the current format.
    auto head = read(FIRST_KEY);
    head.recordType = static_cast<RecordType>(99);
    write(head);
    PinStorage invalid;
    invalid.begin();
    assert(read(FIRST_KEY).recordType == RecordType::CredentialV1);
    std::cout << "PASS: credential types, persistence, recovery and legacy reset\n";
}
