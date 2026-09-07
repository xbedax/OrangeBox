#ifndef PASS_STORE_H
#define PASS_STORE_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <string>

#define FIRST_KEY 0
#define FIRST_KEY_KEY "00000000"
#define LAST_KEY (0xffffffffU)
#define LAST_KEY_KEY "FFFFFFFF"
#define ID_LENGTH 8
#define MAX_PIN_LENGTH 8
#define MAX_NAME_LENGTH 10
#define MIN_PIN_LENGTH 6

struct PinRecord {
    uint32_t pinId;
    uint32_t prevId;
    uint32_t nextId;
    uint8_t  doorNum;
    char name[MAX_NAME_LENGTH + 1];
    char pin[MAX_PIN_LENGTH + 1];
    u_int64_t validFrom;
    u_int64_t validTo;
    int32_t remaining;
};

struct CacheRecord {
    uint32_t pinId;
    char name[MAX_NAME_LENGTH + 1];
    char pin[MAX_PIN_LENGTH + 1];
    uint8_t doorNum;
    u_int64_t validFrom;
    u_int64_t validTo;
    int32_t remaining;
};


class PinStorage {
public:
    bool begin();
    uint32_t addPin(const CacheRecord &pin);
    bool updatePin(const CacheRecord &pin);
    size_t getPins(uint32_t firstPinId, uint32_t numPins, String &pinsTable);
    void garbageCollect();
    bool refreshCache();
    uint8_t usePin(const char* pinValue);
    uint32_t removePin(const CacheRecord& rec);
#ifdef BOX_SIMULATION
    const std::vector<CacheRecord>& debugCache() const;
#endif

private:
    Preferences prefs;
    std::vector<CacheRecord> pinsCache;
    uint32_t maxPinId = 0;
    uint32_t nextPinId = 1;
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
