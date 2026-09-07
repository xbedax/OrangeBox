#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "pass_store.h"
#include "logger.h"
#include "config.h"
#include "gpio_hal.h"

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
        Write new record with correct prevId/nextId(TAIL)
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

static const uint32_t PIN_GC_MAX_SCAN_ID = 4096;

static bool isRealPinId(uint32_t pinId) {
    return pinId != FIRST_KEY && pinId != LAST_KEY;
}

static CacheRecord toCacheRecord(const PinRecord& rec) {
    CacheRecord cacheRec = {};
    cacheRec.pinId = rec.pinId;
    cacheRec.validFrom = rec.validFrom;
    cacheRec.validTo = rec.validTo;
    cacheRec.doorNum = rec.doorNum;
    cacheRec.remaining = rec.remaining;
    strncpy(cacheRec.name, rec.name, MAX_NAME_LENGTH);
    cacheRec.name[MAX_NAME_LENGTH] = '\0';
    strncpy(cacheRec.pin, rec.pin, MAX_PIN_LENGTH);
    cacheRec.pin[MAX_PIN_LENGTH] = '\0';
    return cacheRec;
}

bool PinStorage::updatePin(const CacheRecord& rec) {

    if (!validatePin(rec)) {
        Serial.println("[PinStore] updatePin: invalid pin");
        return false;
    }

    for (auto& record : pinsCache) {
        if (record.pinId == rec.pinId) {
            record.doorNum = rec.doorNum;
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
} //addPin
    
// Update pin.amount to reflect usage, return true if pin was found and updated, false otherwise
uint8_t PinStorage::usePin(const char* pinEntered) {
    time_t now = time(nullptr);
    logger.logPrint(SEVERITY_DEBUG, "usePin: checking pin " + String(pinEntered) + " at time " + String(now) + "\n", BOX_HOST_NAME, LOGAREA_ACCESS);
    for (auto& p : pinsCache) {
        logger.logPrint(SEVERITY_DEBUG, "usePin: checking pinId=" + String(p.pinId) + ", name=" + String(p.name) + ", pin=" + String(p.pin) + ", validFrom=" + String(p.validFrom) + ", validTo=" + String(p.validTo) + ", remaining=" + String(p.remaining) + "\n", BOX_HOST_NAME, LOGAREA_ACCESS);
        if (strcmp(p.pin, pinEntered) != 0)
            continue;
        if (p.validFrom && now < p.validFrom)
            return DOOR_UNKNOWN;
        if (p.validTo && now > p.validTo)
            return DOOR_UNKNOWN;
        if (p.remaining == 0)
            return DOOR_UNKNOWN;
        uint8_t doorNum = p.doorNum;
        if (p.remaining > 1) {
            p.remaining--;
            updatePin(p);
        } else if (p.remaining == 1) {
            CacheRecord usedPin = p;
            removePin(usedPin);
        }
        return doorNum;
    }
    return DOOR_UNKNOWN; // Pin not found or not valid
} // usePin

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

    for (auto it = pinsCache.begin(); it != pinsCache.end(); ++it) {
        if (it->pinId == rec.pinId) {
            pinsCache.erase(it);
            break;
        }
    }

    return rec.pinId;
} // removePin

size_t PinStorage::getPins(uint32_t firstPinId, uint32_t numPins, String &pinsTable)
{
    pinsTable.clear();
    //pinsTable->reserve(pinsCache.size());
    uint32_t PinsFound = 0;
    for (const auto& rec : pinsCache) {
//        PinRecord pin;
//        pin.pinId = rec.pinId;
//        pin.prevId = 0;
//        pin.nextId = 0;
        logger.logPrint(SEVERITY_DEBUG, "getPins: checking pinId=" + String(rec.pinId) + ", name=" + String(rec.name) + ", pin=" + String(rec.pin) + ", validFrom=" + String(rec.validFrom) + ", validTo=" + String(rec.validTo) + ", remaining=" + String(rec.remaining) + "\n", BOX_HOST_NAME, LOGAREA_ACCESS);
        if (rec.pinId >= firstPinId && PinsFound < numPins) {
            PinsFound++;
            logger.logPrint(SEVERITY_DEBUG, "getPins: adding pinId=" + String(rec.pinId) + ", name=" + String(rec.name)  + "\n", BOX_HOST_NAME, LOGAREA_ACCESS);
            pinsTable += "<tr><td>" + String(rec.pinId) + "</td><td>" + String(rec.name) + "</td><td>" + String(rec.pin) + "</td><td>" + String(rec.validFrom) + "</td><td>" + String(rec.validTo) + "</td><td>" + String(rec.remaining) + "</td></tr>";
        }
    }
    return PinsFound;
} // getPins

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
} // update



// Record Key formatting: 8 hex digits of pinId + 0x00  
void PinStorage::pinKey(uint32_t id, char out[ID_LENGTH + 1]) {
    snprintf(out, ID_LENGTH + 1, "%08X", id);
}

bool PinStorage::readData(uint32_t id, PinRecord& rec) {
    char key[ID_LENGTH + 1];
    pinKey(id, key);
    return prefs.getBytes(key, &rec, sizeof(rec)) == sizeof(rec);
}

// Check if the pin attributes contain valid values (length, characters, date range)
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

    Serial.printf("[PinStore] writePin: pinId=%u, name=%s, pin=%s, doorNum=%u, validFrom=%llu, validTo=%llu, remaining=%d\n",
                  pin.pinId, pin.name, pin.pin, pin.doorNum, pin.validFrom, pin.validTo, pin.remaining); // ###
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
        pinKey(prevpin.pinId, prevKey);
        prefs.putBytes(prevKey, &prevpin, sizeof(prevpin));
        pinKey(pin.pinId, recKey);
        prefs.putBytes(recKey, &pin, sizeof(pin));
        lastpin.nextId = lastpin.nextId + 1;
        lastpin.prevId = pin.pinId;
        int written =  prefs.putBytes(LAST_KEY_KEY, &lastpin, sizeof(lastpin));
         if (written == 0 ) {
            Serial.printf("[PinStore] update: failed to write pinId=%u\n", pin.pinId); 
            return 0;
        } else  {
            Serial.printf("[PinStore] update: partial write for pinId=%u, written=%d\n  (of %d)", pin.pinId, written, sizeof(pin)); //###
            
        }

    } else {                                            // found, update data   
        if (!verifyLocalChain(pin.pinId)) {
            Serial.printf("[PinStore] update: pinId=%u not properly chained\n", pin.pinId);
            return 0;
        }
 
        pinKey(pin.pinId, recKey);
        int written = prefs.putBytes(recKey, &pin, sizeof(pin));    
        if (written == 0 ) {
            Serial.printf("[PinStore] update: failed to write pinId=%u\n", pin.pinId); 
            return 0;
        } else  {
            Serial.printf("[PinStore] update: partial write for pinId=%u, written=%d\n  (of %d)", pin.pinId, written, sizeof(pin)); //###
            
        }
    }
    return pin.pinId;
} //writePin

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
} // isActive

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
                cacheRec = toCacheRecord(curPin);
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
    garbageCollect();
    rebuildCache();
    return true;
}

bool PinStorage::refreshCache()
{
    return rebuildCache();
}

#ifdef BOX_SIMULATION
const std::vector<CacheRecord>& PinStorage::debugCache() const
{
    return pinsCache;
}
#endif

bool PinStorage::writeData(const PinRecord& rec) {
    char key[ID_LENGTH + 1];
    pinKey(rec.pinId, key);
    return prefs.putBytes(key, &rec, sizeof(rec)) == sizeof(rec);
}

void PinStorage::resetPool() {
    prefs.clear();

    PinRecord head = {};
    head.pinId = FIRST_KEY;
    head.prevId = FIRST_KEY;
    head.nextId = LAST_KEY;
    writeData(head);

    PinRecord tail = {};
    tail.pinId = LAST_KEY;
    tail.prevId = FIRST_KEY;
    tail.nextId = 1;
    writeData(tail);

    pinsCache.clear();
    maxPinId = 0;
    nextPinId = 1;
}

bool PinStorage::ensurePool() {
    PinRecord head = {};
    PinRecord tail = {};
    bool hasHead = readData(FIRST_KEY, head);
    bool hasTail = readData(LAST_KEY, tail);

    if (!hasHead && !hasTail) {
        Serial.println("[PinStore] GC: empty pin pool, creating sentinels");
        resetPool();
        return false;
    }

    if (!hasHead || !hasTail || head.pinId != FIRST_KEY || tail.pinId != LAST_KEY) {
        Serial.println("[PinStore] GC: corrupted sentinels, resetting pin pool");
        resetPool();
        return false;
    }

    return true;
}

bool PinStorage::isStoredPinValid(const PinRecord& rec) {
    if (!isRealPinId(rec.pinId)) {
        return false;
    }

    PinRecord normalized = rec;
    normalized.name[MAX_NAME_LENGTH] = '\0';
    normalized.pin[MAX_PIN_LENGTH] = '\0';
    CacheRecord cacheRec = toCacheRecord(normalized);
    return validatePin(cacheRec);
}

// Going from HEAD to TAIL, find the last reachable record in the chain, return true the last valid points to TAIL, false if the chain is broken or exceeds the maximum number of steps
//bool PinStorage::findLastReachableFromHead(PinRecord& lastRec) {
//    PinRecord current = {};
//    if (!readData(FIRST_KEY, current) || current.pinId != FIRST_KEY) {
//        return false;
//    }
//    for (uint32_t steps = 0; steps < PIN_GC_MAX_SCAN_ID; steps++) {
//        if (current.nextId == LAST_KEY) {
//            lastRec = current;
//            return true;
//        }
//
//        if (!isRealPinId(current.nextId)) {
//            return false;
//        }
//
//        PinRecord next = {};
//        if (!readData(current.nextId, next) || next.pinId != current.nextId) {
//            return false;
//        }
//
//        if (!isStoredPinValid(next) || next.prevId != current.pinId) {
//            return false;
//        }
//
//        current = next;
//    }
//
//    return false;
//} // findLastReachableFromHead

bool PinStorage::repairTailInsert() {
    PinRecord tail = {};
    if (!readData(LAST_KEY, tail) || tail.pinId != LAST_KEY) {
        return false;
    }
    if (tail.prevId == FIRST_KEY) {         // empty pool, nothing to repair, at least form insertion point of view
        return true;
    }
    if (tail.prevId == LAST_KEY) {          // to avoid infinite loop, this is an invalid state, should not happen
        return false;
    }
    PinRecord tailPrev = {};
    if (!readData(tail.prevId, tailPrev) || tailPrev.pinId != tail.prevId) {
        return false;
    }
    if (tailPrev.nextId == LAST_KEY) {      // no interrupted insert, everything is fine
        return true;
    }   

//    PinRecord lastReachable = {};
    PinRecord newRec = {};    
//    if (!findLastReachableFromHead(lastReachable)) {
//        return false;
//    }

    if (!readData(tailPrev.nextId, newRec)) {  // after 1st step of insert ... rollback to last reachable record
//        if (!findLastReachableFromHead(lastReachable)) {
//            return false;
//        }
        Serial.printf("[PinStore] GC: rolled back interrupted insert, tailPrev.nextId=%u\n",
                      tailPrev.nextId);
        tailPrev.nextId = LAST_KEY;
        writeData(tailPrev);
    } else {
        Serial.printf("[PinStore] GC: found interrupted insert, tailPrev.nextId=%u\n", tailPrev.nextId);
        if ( !isStoredPinValid(newRec) || newRec.nextId != LAST_KEY || newRec.prevId != tailPrev.pinId) {
            return false;
        }
        tail.prevId = newRec.pinId;
        tailPrev.nextId = newRec.pinId + 1;
        writeData(tail);
    }
    return true;
} // repairTailInsert

bool PinStorage::repairDeleteGap(const PinRecord& prevRec, PinRecord& nextRec) {
    uint32_t deletedId = nextRec.prevId;
    if (!isRealPinId(deletedId) || deletedId == prevRec.pinId || deletedId == nextRec.pinId) {
        Serial.println("[PinStore] GC: tangled chain, resetting pin pool");
        return false;
    }

    PinRecord deleted = {};
    if (readData(deletedId, deleted)) {
        if (deleted.pinId != deletedId
            || deleted.prevId != prevRec.pinId
            || deleted.nextId != nextRec.pinId
            || !isStoredPinValid(deleted)) {
                Serial.printf("[PinStore] GC: corrupted delete record pinId=%u, resetting pin pool\n", deletedId);
            return false;
        }
        char key[ID_LENGTH + 1];
        pinKey(deletedId, key);
        prefs.remove(key);
        Serial.printf("[PinStore] GC: removed interrupted delete record pinId=%u\n",
                      deletedId);
    } else {
        Serial.printf("[PinStore] GC: completed interrupted delete after removed pinId=%u\n",
                      deletedId);
    }

    nextRec.prevId = prevRec.pinId;
    writeData(nextRec);
    return true;
} // repairDeleteGap

bool PinStorage::repairInterruptedDelete() {
    PinRecord current = {};
    if (!readData(FIRST_KEY, current) || current.pinId != FIRST_KEY) {
        return false;
    } 

    for (uint32_t steps = 0; steps < PIN_GC_MAX_SCAN_ID; steps++) {
        if (current.nextId == FIRST_KEY || current.nextId == current.pinId) {
            return false;
        }
        PinRecord next = {};
        if (!readData(current.nextId, next) || next.pinId != current.nextId) {
            return false;
        }
        if (isRealPinId(next.pinId) && !isStoredPinValid(next)) {
            return false;
        }
        if (next.prevId != current.pinId) {
            if (!repairDeleteGap(current, next)) {
                return false;
            }
        }
        if (next.pinId == LAST_KEY) {
            return true;
        }
        current = next;
    }

    return false;
} // repairInterruptedDelete

bool PinStorage::repairPinChain() {
    PinRecord current = {};
    PinRecord tail = {};
    PinRecord next = {};
    PinRecord deleted = {};
    if (!readData(LAST_KEY, tail) || tail.pinId != LAST_KEY) {                    //tail present?
        return false;
    } 

    if (!readData(FIRST_KEY, current) || current.pinId != FIRST_KEY) {              //head present?
        return false;
    } 

    for (uint32_t steps = 0; steps < PIN_GC_MAX_SCAN_ID; steps++) {
        Serial.printf ( "[PinStore] GC: traversing chain, current pinId=%u, nextId=%u\n", current.pinId, current.nextId);
        if (current.nextId == FIRST_KEY || current.nextId == current.pinId) {
            return false;
        }
        if (!readData(current.nextId, next) ){
            if (current.pinId == tail.prevId ) {                                    // interrupted insert in step 1, roll back it
                current.nextId = LAST_KEY;
                writeData(current);
                Serial.println("[PinStore] GC: rolled back broken insert");
                return true;
            } else {                                                                // severely damaged chain, cannot repair
                Serial.println("[PinStore] GC: interrupted chain, resetting pin pool");
                return false;
            }
        }
        if( next.pinId != current.nextId) {
            Serial.println("[PinStore] GC: unexpected ID in chain, resetting pin pool");
            return false;
        }
        if (isRealPinId(next.pinId) && !isStoredPinValid(next)) {
            Serial.println("[PinStore] GC: dubious record in chain, resetting pin pool");
            return false;
        }
        if (next.prevId != current.pinId) {
            if (next.prevId == current.prevId) {                                    // interrupted insert in step 2, complete it
                if (next.pinId == LAST_KEY) {
                    Serial.printf("[PinStore] GC: redoing unfinished insert, pinId %u\n", current.pinId);
                    tail.prevId = current.pinId;
                    tail.nextId = current.pinId + 1;
                    writeData(tail);
                    return true;
                }
            } 
            if (readData(next.prevId, deleted) && next.prevId == deleted.pinId) {    // interrupted delete in step 1, complete it
                char key[ID_LENGTH + 1];
                pinKey(deleted.pinId, key);
                prefs.remove(key);
                Serial.printf("[PinStore] GC: removed interrupted delete record pinId=%u\n", deleted.pinId);
            }
            if (!readData(next.prevId, deleted)) {                                      // interrupted delete in step 2, complete it
                next.prevId = current.pinId;
                writeData(next);
                Serial.println("[PinStore] GC: completed interrupted delete");
                return true;
            }
            Serial.println("[PinStore] GC: broken chain, resetting pin pool");
            return false;
            if (!repairDeleteGap(current, next)) {
                return false;
            }
        }
        if (next.pinId == LAST_KEY) {
            return true;
        }
        current = next;
    }
    Serial.printf("[PinStore] GC: unable to traverse chain, resetting pool, last deleted pinId=%u\n", current.pinId);
    return false;
} // repairPinChain


void PinStorage::garbageCollect()
{
    if (!ensurePool()) {
        return;
    }
    if (!repairPinChain()) {
        Serial.println("[PinStore] GC: unrecoverable chain state, resetting pin pool");
        resetPool();

    }

//    if (!repairTailInsert()) {
//        Serial.println("[PinStore] GC: unrecoverable tail insert state, resetting pin pool");
//        resetPool();
//        return;
//    }

//    if (!repairInterruptedDelete()) {
//        Serial.println("[PinStore] GC: unrecoverable chain state, resetting pin pool");
//        resetPool();
//        return;
//    }
    Serial.println("[PinStore] GC: completed");
}
