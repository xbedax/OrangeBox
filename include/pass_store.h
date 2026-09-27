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

enum class CredType : uint8_t {
    All = 0, // Query filter only; never a stored credential type.
    Password = 1,
    PacketNumber = 2
};

static_assert(PACKET_NUMBER_MIN > 0 && PACKET_NUMBER_MIN <= PACKET_NUMBER_MAX,
              "Invalid packet number length range");
static_assert(PACKET_NUMBER_MATCH > 0 && PACKET_NUMBER_MATCH <= PACKET_NUMBER_MIN,
              "Packet prefix must fit the shortest accepted number");

struct CredRecord {
    RecordType recordType = RecordType::CredentialV1; // First byte of the NVS blob.
    CredType credType = CredType::Password;
    uint32_t credId;
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
        return credType == CredType::PacketNumber ? packetNumber : pin;
    }
};

static_assert(offsetof(CredRecord, recordType) == 0, "Record type must be the first byte");
static_assert(std::is_trivially_copyable<CredRecord>::value, "NVS record must be byte-copyable");

struct CacheRecord {
    CredType credType = CredType::Password;
    uint32_t credId;
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
        return credType == CredType::PacketNumber ? packetNumber : pin;
    }
};


class CredStorage {
public:
    bool begin();
    // Record-taking operations use rec.credType (defaults to Password).
    // All is only valid as a getCreds filter, never for storage or credential verification.
    uint32_t addCred(const CacheRecord &cred);
    bool updateCred(const CacheRecord &cred);
    // Queries return active credentials only, as does access verification.
    size_t getCreds(uint32_t firstCredId, uint32_t numCreds, std::vector<CacheRecord>& creds,
                   CredType credType = CredType::Password);
    bool getCred(uint32_t credId, CacheRecord& cred, CredType credType);
    void garbageCollect();
    bool refreshCache();
    uint8_t verifyCred(const char* credValue, CredType credType = CredType::Password);
    bool usedCred(uint8_t doorNum);
    uint32_t removeCred(const CacheRecord& rec);
#ifdef BOX_SIMULATION
    const std::vector<CacheRecord>& debugCache() const;
#endif

private:
    Preferences prefs;
    std::vector<CacheRecord> credsCache;
    uint32_t maxCredId = 0;
    uint32_t nextCredId = 1;
    uint32_t verifiedCredId = 0;
    bool rebuildCache();
    bool verifyLocalChain(uint32_t credId);
    void credKey(uint32_t id, char out[ID_LENGTH + 1]);
    uint32_t writeCred(const CredRecord& rec);
    bool readData(uint32_t id, CredRecord& rec);
    bool update(const CacheRecord& rec);
    bool isActive(const CredRecord& rec);
    bool ensurePool();
    void resetPool();
    bool writeData(const CredRecord& rec);
    bool isStoredCredValid(const CredRecord& rec);
    //bool findLastReachableFromHead(CredRecord& lastRec);
    bool repairTailInsert();
    bool repairCredChain();
    bool repairInterruptedDelete();
    bool repairDeleteGap(const CredRecord& prevRec, CredRecord& nextRec);
    

};


#endif // PASS_STORE_H
