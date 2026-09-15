#ifndef PASS_STORE_H
#define PASS_STORE_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <string>
#include <cstddef>
#include <type_traits>
#include "config.h"

#define FIRST_KEY 0
#define FIRST_KEY_KEY "00000000"
#define LAST_KEY (0xffffffffU)
#define LAST_KEY_KEY "FFFFFFFF"
#define ID_LENGTH 8
#define MAX_PIN_LENGTH 8
#define MAX_NAME_LENGTH 10
#define MIN_PIN_LENGTH 6

enum class RecordType : uint8_t {
    CredentialV1 = 1
};

enum class PinType : uint8_t {
    All = 0, // Query filter only; never a stored credential type.
    Password = 1,
    PacketNumber = 2
};

static_assert(PACKET_NUMBER_MIN > 0 && PACKET_NUMBER_MIN <= PACKET_NUMBER_MAX,
              "Invalid packet number length range");
static_assert(PACKET_NUMBER_MATCH > 0 && PACKET_NUMBER_MATCH <= PACKET_NUMBER_MIN,
              "Packet prefix must fit the shortest accepted number");

struct PinRecord {
    RecordType recordType = RecordType::CredentialV1; // First byte of the NVS blob.
    PinType pinType = PinType::Password;
    uint32_t pinId;
    uint32_t prevId;
    uint32_t nextId;
    uint8_t  doorNum;
    char name[MAX_NAME_LENGTH + 1];
    union {
        char pin[MAX_PIN_LENGTH + 1] = {};
        char packetNumber[PACKET_NUMBER_MAX + 1];
    };
    u_int64_t validFrom;
    u_int64_t validTo;
    int32_t remaining;
    const char* credential() const {
        return pinType == PinType::PacketNumber ? packetNumber : pin;
    }
};

static_assert(offsetof(PinRecord, recordType) == 0, "Record type must be the first byte");
static_assert(std::is_trivially_copyable<PinRecord>::value, "NVS record must be byte-copyable");

struct CacheRecord {
    PinType pinType = PinType::Password;
    uint32_t pinId;
    char name[MAX_NAME_LENGTH + 1];
    union {
        char pin[MAX_PIN_LENGTH + 1] = {};
        char packetNumber[PACKET_NUMBER_MAX + 1];
    };
    uint8_t doorNum;
    u_int64_t validFrom;
    u_int64_t validTo;
    int32_t remaining;
    const char* credential() const {
        return pinType == PinType::PacketNumber ? packetNumber : pin;
    }
};


class PinStorage {
public:
    bool begin();
    // Record-taking operations use rec.pinType (defaults to Password).
    // All is only valid as a getPins filter, never for storage or PIN verification.
    uint32_t addPin(const CacheRecord &pin);
    bool updatePin(const CacheRecord &pin);
    size_t getPins(uint32_t firstPinId, uint32_t numPins, String &pinsTable,
                   PinType pinType = PinType::Password);
    void garbageCollect();
    bool refreshCache();
    uint8_t verifyPin(const char* pinValue, PinType pinType = PinType::Password);
    bool usedPin(uint8_t doorNum);
    uint32_t removePin(const CacheRecord& rec);
#ifdef BOX_SIMULATION
    const std::vector<CacheRecord>& debugCache() const;
#endif

private:
    Preferences prefs;
    std::vector<CacheRecord> pinsCache;
    uint32_t maxPinId = 0;
    uint32_t nextPinId = 1;
    uint32_t verifiedPinId = 0;
    bool rebuildCache();
    bool verifyLocalChain(uint32_t pinId);
    void pinKey(uint32_t id, char out[ID_LENGTH + 1]);
    uint32_t writePin(const PinRecord& rec);
    bool readData(uint32_t id, PinRecord& rec);
    bool update(const CacheRecord& rec);
    bool isActive(const PinRecord& rec);
    bool ensurePool();
    void resetPool();
    bool writeData(const PinRecord& rec);
    bool isStoredPinValid(const PinRecord& rec);
    //bool findLastReachableFromHead(PinRecord& lastRec);
    bool repairTailInsert();
    bool repairPinChain();
    bool repairInterruptedDelete();
    bool repairDeleteGap(const PinRecord& prevRec, PinRecord& nextRec);
    

};


#endif // PASS_STORE_H
