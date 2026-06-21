#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "pass_store.h"

/*
    PinStorage class manages a double linked list of PinRecords stored in Preferences.
    Each record has a unique pinId and links to the previous and next records in the chain.
    The chain is anchored by two special records with pinId=0 (HEAD) and pinId=0xffffffff (TAIL).
    The class provides methods to add, update, remove, and retrieve pins, as well as to validate pins and perform garbage collection.

    Sentinel records
    pinId           role    prevId                                  nextId 
    0x00000000      HEAD    — (self or ignored)                     first real record (or TAIL if empty)
    0xFFFFFFFF      TAIL    last real record (or HEAD if empty)     next free pinId for new insertions

    Fault tolerant insertion sequence — 3 writes:
        Relink (TAIL.prevId).nextId → newId
        Write new record with correct prevId/nextId
        Relink TAIL.prevId → newId
    (rollback if only step1 is done, redo if step 2 is done)

    Fault tolerant deletion sequence — 3 writes:
        Relink (del.prevId).nextId → del.nextId
        Remove (or zero out) deleted record
        Relink (del.nextId).prevId → del.prevId 
    (redo if step 1 is done)   

    pinsCache (a simple array of cacheRecords) used to speedup access to active pins, rebuild from storage on startup, update on add/update/remove operations when eeprom change successful, verify chain links before update/remove operations to avoid cache desync
*/   


/*
tohle je treba presunout do main
    void begin() {
        
        PinRepository = new PinStorage();
        PinRepository->begin();
        PinRepository->getPins();
        PinRepository->garbageCollect();
    }

 */   

/*
------------------- public functions -------------------------
    bool begin();
    uint32_t addPin(const CacheRecord &pin);
    bool updatePin(const CacheRecord &pin);
    uint32_t removePin(uint32_t pinId);
    size_t getPins(std::vector<PinRecord>& pinsTable);
    void garbageCollect();
    bool usePin(char* pinValue);
*/ 

bool validatePin(const CacheRecord& p);

bool PinStorage::updatePin(const CacheRecord& rec) {

    if (!validatePin(rec)) {
        Serial.println("[PinStore] updatePin: invalid pin");
        return false;
    }

    for (auto& record : pinsCache) {
        if (record.pinId == rec.pinId) {
            record.remaining = rec.remaining;
            record.validFrom = rec.validFrom;
            record.validTo = rec.validTo;
            break;
        }
    }
    return update(rec);
}


// Add new pin, return true if added successfully, false otherwise (e.g. invalid pin or storage failure)
uint32_t PinStorage::addPin(const CacheRecord& rec) {
    if (!validatePin(rec)) {
        Serial.println("[PinStore] addPin: invalid pin");
        return 0;
    }
    CacheRecord newRec = rec;
    PinRecord pinRec;
    pinRec.pinId = 0; // will be assigned in writePin
    strncpy(pinRec.name, rec.name, MAX_NAME_LENGTH);
    strncpy(pinRec.pin, rec.pin, MAX_PIN_LENGTH);
    pinRec.validFrom = rec.validFrom;
    pinRec.validTo = rec.validTo;
    pinRec.remaining = rec.remaining;
    pinRec.doorNum = rec.doorNum;
    uint32_t newId = writePin(pinRec);
    if (newId == 0) {
        Serial.println("[PinStore] addPin: failed to write new pin");
        return 0;
    }
    newRec.pinId = newId;
    pinsCache.push_back(newRec);
    return newId;
}
    
// Update pin.amount to reflect usage, return true if pin was found and updated, false otherwise
uint8_t PinStorage::usePin(const char* pinEntered) {
    time_t now = time(nullptr);
    for (auto& p : pinsCache) {
        if (strcmp(p.pin, pinEntered) != 0)
            continue;
        if (p.validFrom && now < p.validFrom)
            return false;
        if (p.validTo && now > p.validTo)
            return false;
        if (p.remaining == 0)
            return false;
        if (p.remaining > 1) {
            p.remaining--;
            updatePin(p);
        }
        if (p.remaining == 1) {
            removePin(p);
        }
        return p.doorNum;
    }
    return 0;
}

//
uint32_t PinStorage::removePin(const CacheRecord& rec) {
    PinRecord pinRec;
    char prevKey[ID_LENGTH + 1];
    char nextKey[ID_LENGTH + 1];

    if (!readData(rec.pinId, pinRec)) {
        Serial.printf("[PinStore] removePin: record with pinId=%u not found\n", rec.pinId);
        return 0;
    }
    if (!verifyLocalChain(rec.pinId)) {
        Serial.printf("[PinStore] update: pinId=%u not properly chained\n", rec.pinId);
        return 0;
    }
    if (strcmp (pinRec.name, rec.name) != 0 || strcmp (pinRec.pin, rec.pin) != 0) {
        Serial.printf("[PinStore] removePin: name or pin mismatch for pinId=%u\n", rec.pinId);
        return 0;
    }
    // update links of prev and next records
    PinRecord prevRec;
    PinRecord nextRec;
    if (!readData(pinRec.prevId, prevRec)) {
        Serial.printf("[PinStore] removePin: failed to read prev record with id=%u\n", pinRec.prevId);
        return 0;
    }
    if (!readData(pinRec.nextId, nextRec)) {
        Serial.printf("[PinStore] removePin: failed to read next record with id=%u\n", pinRec.nextId);
        return 0;
    }
    prevRec.nextId = pinRec.nextId;
    nextRec.prevId = pinRec.prevId;
    pinKey(prevRec.pinId, prevKey);
    prefs.putBytes(prevKey, &prevRec, sizeof(prevRec));     // update prev record
    char recKey[ID_LENGTH + 1];
    pinKey(rec.pinId, recKey);
    prefs.remove(recKey);                                   // remove the record
    pinKey(nextRec.pinId, nextKey);
    prefs.putBytes(nextKey, &nextRec, sizeof(nextRec));     // update next record

    // remove the record
    return rec.pinId;
}

size_t PinStorage::getPins(uint8_t firstPinId, uint8_t numPins, String &pinsTable)
{
    pinsTable.clear();
    //pinsTable->reserve(pinsCache.size());
    for (const auto& rec : pinsCache) {
        PinRecord pin;
        pin.pinId = rec.pinId;
        pin.prevId = 0;
        pin.nextId = 0;
        if (rec.pinId >= firstPinId && rec.pinId < firstPinId + numPins) {
            pinsTable += "<tr><td>" + String(rec.pinId) + "</td><td>" + String(rec.name) + "</td><td>" + String(rec.pin) + "</td><td>" + String(rec.validFrom) + "</td><td>" + String(rec.validTo) + "</td><td>" + String(rec.remaining) + "</td></tr>";
        }
    }
    return pinsCache.size();
}

/*
--------------internal functions-------------------------

    void pinKey(uint32_t id, char out[ID_LENGTH + 1]);
    bool readData(uint32_t id, PinRecord& rec);
    bool isValid(const PinRecord& rec);
    bool registerUse(const char* pinEntered);
    uint32_t writePin(const PinRecord &pinIn);
    bool verifyLocalChain(uint32_t pinId);  
*/


// Handle update of password entry
bool PinStorage::update(const CacheRecord& cacheRec) {
    // Verify record exists in chain before overwriting data
    PinRecord pinRec;

    if (!readData(cacheRec.pinId, pinRec)) {
        Serial.printf("[PinStore] update: record with pinId=%u not found\n", cacheRec.pinId);
        return false;
    }   
    if (strcmp (pinRec.name, cacheRec.name) != 0 || strcmp (pinRec.pin, cacheRec.pin) != 0) {
        Serial.printf("[PinStore] update: name or pin mismatch for pinId=%u\n", cacheRec.pinId);
        return false;
    }
    pinRec.validFrom = cacheRec.validFrom;
    pinRec.validTo = cacheRec.validTo;
    pinRec.remaining = cacheRec.remaining;
    pinRec.doorNum = cacheRec.doorNum;                                 //### upravit až bude číslo dveří v UI

    if (!writePin(pinRec)) {
        Serial.printf("[PinStore] update: write failed for pinId=%u\n", pinRec.pinId);
        return false;
    }
    return true;
}


// Record Key formatting: 8 hex digits of pinId + 0x00  
void PinStorage::pinKey(uint32_t id, char out[ID_LENGTH + 1]) {
    snprintf(out, ID_LENGTH + 1, "%08x", id);
}

bool PinStorage::readData(uint32_t id, PinRecord& rec) {
    char key[ID_LENGTH + 1];
    pinKey(id, key);
    return prefs.getBytes(key, &rec, sizeof(rec)) == sizeof(rec);
}

bool validatePin(const CacheRecord& p) {

    int len = strlen(p.pin);
    if (len < MIN_PIN_LENGTH || len > MAX_PIN_LENGTH)
        return false;
    for (int i=0;i<len;i++)
        if (!isdigit(p.pin[i]))
            return false;
    if (strlen(p.name) == 0)
        return false;
    if (strlen(p.name) > MAX_NAME_LENGTH)
        return false;
    if (p.validFrom && p.validTo && p.validFrom > p.validTo)
        return false;
    return true;
} // validatePin


// Insert new record into the chain, update prev and next records accordingly, return new pinId
uint32_t PinStorage::writePin(const PinRecord &pinIn)
{
    PinRecord pin = pinIn;
    PinRecord storedpin;
    PinRecord lastpin;
    PinRecord prevpin;
    char recKey[ID_LENGTH + 1];
    char prevKey[ID_LENGTH + 1];


    if (pinIn.pinId == 0 || !readData(pinIn.pinId, storedpin)) {   // not found, create new record
        if (prefs.getBytes(LAST_KEY_KEY, &lastpin, sizeof(lastpin)) != sizeof(lastpin)) {
            Serial.println("[PinStore] writePin: failed to read last record");
            return 0;
        }
        pin.pinId = lastpin.nextId;
        if (!readData(lastpin.prevId, prevpin)) {
            Serial.printf("[PinStore] writePin: failed to read prev record with id=%u\n", lastpin.prevId);
            return 0;
        }
        
        pin.nextId = LAST_KEY;
        pin.prevId = lastpin.prevId;
        prevpin.nextId = pin.pinId;   
        lastpin.nextId = lastpin.nextId + 1;
        lastpin.prevId = pin.pinId;
        prefs.putBytes(LAST_KEY_KEY, &lastpin, sizeof(lastpin));
        pinKey(pin.pinId, recKey);
        prefs.putBytes(recKey, &pin, sizeof(pin));
        pinKey(prevpin.pinId, prevKey);
        prefs.putBytes(prevKey, &prevpin, sizeof(prevpin));
    } else {                                            // found, update data   
        if (!verifyLocalChain(pin.pinId)) {
            Serial.printf("[PinStore] update: pinId=%u not properly chained\n", pin.pinId);
            return 0;
        }
 
        pinKey(pin.pinId, recKey);
        prefs.putBytes(recKey, &pin, sizeof(pin));
    }
    return pin.pinId;
}

// Verify that the record with the given key is correctly linked in the chain (prevId and nextId point to valid records and link back to this record)
bool PinStorage::verifyLocalChain(uint32_t pinId) {
    PinRecord rec;
    PinRecord prevrec;
    PinRecord nextrec;

    if (!readData(pinId, rec))
        return false;
    if (rec.pinId == FIRST_KEY || rec.pinId == LAST_KEY)
        return true;
    if (!readData(rec.prevId, prevrec))
         return false;
    if (!readData(rec.nextId, nextrec))
        return false;
    return prevrec.nextId == rec.pinId && nextrec.prevId == rec.pinId;
}

bool PinStorage::isActive(const PinRecord& rec) {
    time_t now = time(nullptr);
    if (rec.validFrom && now < rec.validFrom)
        return false;
    if (rec.validTo && now > rec.validTo)
        return false;
    if (rec.remaining == 0)
        return false;
    return true;
}

bool PinStorage::rebuildCache() {
    pinsCache.clear();
    maxPinId = 0;

    uint32_t cur = FIRST_KEY;
    if (!prefs.isKey(FIRST_KEY_KEY)) {
        Serial.println("[PinStore] rebuildCache: HEAD unreadable");
        return false;
    }

    while (cur != LAST_KEY) {
        PinRecord curPin;
        CacheRecord cacheRec;

        if (!readData(cur, curPin)) {
            Serial.printf("[PinStore] rebuildCache: broken link at id=%u\n", cur);
            break;
        }

        // maxPinId tracks every real pinId regardless of validity
        if (cur != FIRST_KEY && cur != LAST_KEY) {
            if (curPin.pinId > maxPinId) {
                maxPinId = curPin.pinId;
            }
            if (isActive(curPin)) {
                cacheRec.pinId = curPin.pinId;
                cacheRec.validFrom = curPin.validFrom;
                cacheRec.validTo = curPin.validTo;
                cacheRec.doorNum = curPin.doorNum;
                cacheRec.remaining = curPin.remaining;
                strncpy(cacheRec.name, curPin.name, MAX_NAME_LENGTH);
                cacheRec.name[MAX_NAME_LENGTH] = '\0';
                strncpy(cacheRec.pin, curPin.pin, MAX_PIN_LENGTH);
                cacheRec.pin[MAX_PIN_LENGTH] = '\0';
                pinsCache.push_back(cacheRec);
            }
        }

        cur = curPin.nextId;
    }

    Serial.printf("[PinStore] Cache: %zu active record(s), maxPinId=%u\n",
                  pinsCache.size(), maxPinId);
    return true;
}

bool PinStorage::begin()
{
    prefs.begin("pins", false);
    if (!prefs.isKey(FIRST_KEY_KEY)) {
        PinRecord rec = {};
        rec.pinId = FIRST_KEY;
        rec.prevId = 0; 
        rec.nextId = LAST_KEY;
        rec.name[0] = '\0';
        rec.pin[0] = '\0';
        prefs.putBytes(FIRST_KEY_KEY, &rec, sizeof(rec));
    }
    if (!prefs.isKey(LAST_KEY_KEY)) {
        PinRecord rec = {};
        rec.pinId = LAST_KEY;
        rec.prevId = FIRST_KEY; 
        rec.nextId = 1;
        rec.name[0] = '\0';
        rec.pin[0] = '\0';
        prefs.putBytes(LAST_KEY_KEY, &rec, sizeof(rec));
    }
    rebuildCache();
    return true;
}


void PinStorage::garbageCollect()
{  
    // TODO: implement garbage collect
}
