#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "pass_store.h"
#include "logger.h"
#include "config.h"
#include "gpio_hal.h"

/*
    CredStorage class manages a double linked list of CredRecords stored in Preferences.
    Each record has a unique credId and links to the previous and next records in the chain.
    The chain is anchored by two special records with credId=0 (HEAD) and credId=0xffffffff (TAIL).
    The class provides methods to add, update, remove, and retrieve credentials, as well as to validate credentials and perform garbage collection.

    Sentinel records
    credId           role    prevId                                  nextId
    0x00000000      HEAD    — (self or ignored)                     first real record (or TAIL if empty)
    0xFFFFFFFF      TAIL    last real record (or HEAD if empty)     next free credId for new insertions

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

    credsCache (a simple array of cacheRecords) used to speedup access to active credentials, rebuild from storage on startup, update on add/update/remove operations when eeprom change successful, verify chain links before update/remove operations to avoid cache desync
*/   


bool validateCred(const CacheRecord& p);

static bool isCredentialType(CredType type) {
    return type == CredType::Password || type == CredType::PacketNumber;
}

static bool validateCredential(const char* value, CredType type, size_t maxOverride = 0) {
    if (!value || !isCredentialType(type)) return false;
    const size_t minimum = type == CredType::Password ? MIN_PIN_LENGTH : PACKET_NUMBER_MIN;
    const size_t maximum = maxOverride ? maxOverride : (type == CredType::Password ? MAX_PIN_LENGTH : PACKET_NUMBER_MAX);
    const size_t length = strnlen(value, maximum + 1);
    if (length < minimum || length > maximum) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (type == CredType::Password ? (ch < '0' || ch > '9') : (ch < 33 || ch > 126))
            return false;
    }
    return true;
}

template<typename Destination, typename Source>
static void copyCredential(Destination& destination, const Source& source) {
    destination.credType = source.credType;
    const size_t maximum = source.credType == CredType::PacketNumber ? PACKET_NUMBER_MAX : MAX_PIN_LENGTH;
    const size_t length = strnlen(source.credential(), maximum);
    if (source.credType == CredType::PacketNumber) {
        // Indexed assignment also activates the packetNumber member of the union.
        for (size_t i = 0; i < PACKET_NUMBER_MAX; ++i)
            destination.packetNumber[i] = i < length ? source.packetNumber[i] : '\0';
        destination.packetNumber[PACKET_NUMBER_MAX] = '\0';
    } else {
        for (size_t i = 0; i < MAX_PIN_LENGTH; ++i)
            destination.pin[i] = i < length ? source.pin[i] : '\0';
        destination.pin[MAX_PIN_LENGTH] = '\0';
    }
}

static const uint32_t CRED_GC_MAX_SCAN_ID = 4096;

static bool isRealCredId(uint32_t credId) {
    return credId != FIRST_KEY && credId != LAST_KEY;
}

static CacheRecord toCacheRecord(const CredRecord& rec) {
    CacheRecord cacheRec = {};
    cacheRec.credId = rec.credId;
    cacheRec.validFrom = rec.validFrom;
    cacheRec.validTo = rec.validTo;
    cacheRec.doorNum = rec.doorNum;
    cacheRec.remaining = rec.remaining;
    strncpy(cacheRec.name, rec.name, MAX_NAME_LENGTH);
    cacheRec.name[MAX_NAME_LENGTH] = '\0';
    copyCredential(cacheRec, rec);
    return cacheRec;
}

bool CredStorage::updateCred(const CacheRecord& rec) {

    if (!validateCred(rec)) {
        logger.logPrint(A_CREDSTORE_UPDATECRED_INVALID_CREDENTIAL, LOGAREA_ACCESS, SEVERITY_ERROR);
        return false;
    }

    if (!update(rec)) return false;

    for (auto& record : credsCache) {
        if (record.credId == rec.credId) {
            record = rec;
            return true;
        }
    }
    credsCache.push_back(rec);
    return true;
}


// Add new credential, return true if added successfully, false otherwise (e.g. invalid credential or storage failure)
uint32_t CredStorage::addCred(const CacheRecord& rec) {
    if (!validateCred(rec)) {
        logger.logPrint(A_CREDSTORE_ADDCRED_INVALID_CREDENTIAL, LOGAREA_ACCESS, SEVERITY_ERROR);
        return 0;
    }
    CacheRecord newRec = rec;
    CredRecord credRec = {};
    credRec.credId = 0; // will be assigned in writeCred
    strncpy(credRec.name, rec.name, MAX_NAME_LENGTH);
    credRec.name[MAX_NAME_LENGTH] = '\0';
    copyCredential(credRec, rec);
    credRec.validFrom = rec.validFrom;
    credRec.validTo = rec.validTo;
    credRec.remaining = rec.remaining;
    credRec.doorNum = rec.doorNum;
    uint32_t newId = writeCred(credRec);
    if (newId == 0) {
        logger.logPrint(A_CREDSTORE_ADDCRED_FAILED_TO_WRITE_NEW_CREDENTIAL, LOGAREA_ACCESS, SEVERITY_ERROR);
        return 0;
    }
    newRec.credId = newId;
    credsCache.push_back(newRec);
    return newId;
} //addCred
    
uint8_t CredStorage::verifyCred(const char* credEntered, CredType credType) {
    verifiedCredId = 0;
//    char credEnteredSubstr [PACKET_NUMBER_MATCH + 1];
//    strncpy(credEnteredSubstr, credEntered, PACKET_NUMBER_MATCH);
//    credEnteredSubstr[(strlen(credEntered) < PACKET_NUMBER_MATCH)? strlen(credEntered)+1 : PACKET_NUMBER_MATCH+1] = '\0';
    if (!validateCredential(credEntered, credType, credType == CredType::PacketNumber ? PACKET_SCAN_MAX : 0))
        return DOOR_UNKNOWN;
    time_t now = time(nullptr);
    logger.logPrint(A_CREDENTIAL_VERIFY_STARTED, LOGAREA_OPERATION, SEVERITY_DEBUG, now);
    for (auto& p : credsCache) {
        logger.logPrint(A_CREDENTIAL_VERIFY_CANDIDATE, LOGAREA_OPERATION, SEVERITY_DEBUG, p.credId, p.name, p.validFrom, p.validTo, p.remaining);
        if (p.credType != credType)
            continue;
        if (credType == CredType::Password
                ? strcmp(p.pin, credEntered) != 0
                : strstr(credEntered, p.packetNumber) == NULL)
            continue;
        if (p.validFrom && now < p.validFrom)
            return DOOR_UNKNOWN;
        if (p.validTo && now > p.validTo)
            return DOOR_UNKNOWN;
        if (p.remaining == 0)
            return DOOR_UNKNOWN;
        verifiedCredId = p.credId;
        return p.doorNum;
    }
    return DOOR_UNKNOWN; // Credential not found or not valid
} // verifyCred

bool CredStorage::usedCred(uint8_t doorNum) {
    if (verifiedCredId == 0) return false;
    time_t now = time(nullptr);
    for (auto& p : credsCache) {
        if (p.credId != verifiedCredId || p.doorNum != doorNum)
            continue;
        if ((p.validFrom && now < p.validFrom)
            || (p.validTo && now > p.validTo)
            || p.remaining == 0) {
            verifiedCredId = 0;
            return false;
        }
        if (p.remaining > 1) {
            CacheRecord consumedCred = p;
            consumedCred.remaining--;
            if (!updateCred(consumedCred)) return false;
        } else if (p.remaining == 1) {
            if (!removeCred(p)) return false;
        }
        verifiedCredId = 0;
        return true;
    }
    verifiedCredId = 0;
    return false;
} // usedCred

//
uint32_t CredStorage::removeCred(const CacheRecord& rec) {
    CredRecord credRec;
    char prevKey[ID_LENGTH + 1];
    char nextKey[ID_LENGTH + 1];

    if (!readData(rec.credId, credRec)) {
        logger.logPrint(A_CREDSTORE_REMOVECRED_RECORD_WITH_CREDID_NOT_FOUND, LOGAREA_ACCESS, SEVERITY_ERROR, rec.credId);
        return 0;
    }
    if (!verifyLocalChain(rec.credId)) {
        logger.logPrint(A_CREDSTORE_UPDATE_CREDID_NOT_PROPERLY_CHAINED, LOGAREA_ACCESS, SEVERITY_ERROR, rec.credId);
        return 0;
    }
    if (!validateCredential(rec.credential(), rec.credType)
        || credRec.credType != rec.credType
        || strncmp(credRec.name, rec.name, MAX_NAME_LENGTH + 1) != 0
        || strcmp(credRec.credential(), rec.credential()) != 0) {
        logger.logPrint(A_CREDSTORE_REMOVECRED_NAME_OR_CREDENTIAL_MISMATCH_FOR_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, rec.credId);
        return 0;
    }
    // update links of prev and next records
    CredRecord prevRec;
    CredRecord nextRec;
    if (!readData(credRec.prevId, prevRec)) {
        logger.logPrint(A_CREDSTORE_REMOVECRED_FAILED_TO_READ_PREV_RECORD_WITH, LOGAREA_ACCESS, SEVERITY_ERROR, credRec.prevId);
        return 0;
    }
    if (!readData(credRec.nextId, nextRec)) {
        logger.logPrint(A_CREDSTORE_REMOVECRED_FAILED_TO_READ_NEXT_RECORD_WITH, LOGAREA_ACCESS, SEVERITY_ERROR, credRec.nextId);
        return 0;
    }
    prevRec.nextId = credRec.nextId;
    nextRec.prevId = credRec.prevId;
    credKey(prevRec.credId, prevKey);
    writeData(prevRec);                                    // update prev record
    char recKey[ID_LENGTH + 1];
    credKey(rec.credId, recKey);
    if (!prefs.remove(recKey))
        logger.logPrint(A_NVS_OPERATION_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, "remove", rec.credId);                                   // remove the record
    credKey(nextRec.credId, nextKey);
    writeData(nextRec);                                    // update next record

    for (auto it = credsCache.begin(); it != credsCache.end(); ++it) {
        if (it->credId == rec.credId) {
            credsCache.erase(it);
            break;
        }
    }

    return rec.credId;
} // removeCred

bool CredStorage::getCred(uint32_t credId, CacheRecord& cred, CredType credType) {
    CredRecord stored = {};
    if (!isRealCredId(credId) || !isCredentialType(credType)
        || !readData(credId, stored) || stored.credId != credId || stored.credType != credType
        || !isStoredCredValid(stored)) return false;
    cred = toCacheRecord(stored);
    return true;
}

size_t CredStorage::getCreds(uint32_t firstCredId, uint32_t numCreds,
                            std::vector<CacheRecord>& creds, CredType credType) {
    creds.clear();
    if (numCreds == 0 || (credType != CredType::All && !isCredentialType(credType))) return 0;
    const time_t now = time(nullptr);
    for (const auto& cred : credsCache) {
        if ((cred.validFrom && now < cred.validFrom) || (cred.validTo && now > cred.validTo)
            || cred.remaining == 0) continue;
        if (cred.credId >= firstCredId
            && (credType == CredType::All || cred.credType == credType)) {
            creds.push_back(cred);
            if (creds.size() == numCreds) break;
        }
    }
    return creds.size();
}

/*
--------------internal functions-------------------------

    void credKey(uint32_t id, char out[ID_LENGTH + 1]);
    bool readData(uint32_t id, CredRecord& rec);
    bool isValid(const CredRecord& rec);
    bool registerUse(const char* credEntered);
    uint32_t writeCred(const CredRecord &credIn);
    bool verifyLocalChain(uint32_t credId);
*/


// Handle update of password entry
bool CredStorage::update(const CacheRecord& cacheRec) {
    // Verify record exists in chain before overwriting data
    CredRecord credRec;

    if (!readData(cacheRec.credId, credRec)) {
        logger.logPrint(A_CREDSTORE_UPDATE_RECORD_WITH_CREDID_NOT_FOUND, LOGAREA_ACCESS, SEVERITY_ERROR, cacheRec.credId);
        return false;
    }   
    if (credRec.credType != cacheRec.credType
        || strcmp(credRec.name, cacheRec.name) != 0
        || strcmp(credRec.credential(), cacheRec.credential()) != 0) {
        logger.logPrint(A_CREDSTORE_UPDATE_MISMATCH, LOGAREA_ACCESS, SEVERITY_ERROR, cacheRec.credId);
        return false;
    }
    credRec.validFrom = cacheRec.validFrom;
    credRec.validTo = cacheRec.validTo;
    credRec.remaining = cacheRec.remaining;
    credRec.doorNum = cacheRec.doorNum;                                 //### upravit až bude číslo dveří v UI

    if (!writeCred(credRec)) {
        logger.logPrint(A_CREDSTORE_UPDATE_WRITE_FAILED_FOR_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, credRec.credId);
        return false;
    }
    return true;
} // update



// Record Key formatting: 8 hex digits of credId + 0x00
void CredStorage::credKey(uint32_t id, char out[ID_LENGTH + 1]) {
    snprintf(out, ID_LENGTH + 1, "%08X", id);
}

bool CredStorage::readData(uint32_t id, CredRecord& rec) {
    char key[ID_LENGTH + 1];
    credKey(id, key);
    if (prefs.getBytesLength(key) != sizeof(CredRecord)) {
        if (prefs.isKey(key))
            logger.logPrint(A_STORED_CREDENTIAL_INVALID, LOGAREA_ACCESS, SEVERITY_ERROR, id);
        return false;
    }
    CredRecord stored = {};
    if (prefs.getBytes(key, &stored, sizeof(stored)) != sizeof(stored)) {
        logger.logPrint(A_NVS_OPERATION_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, "read", id);
        return false;
    }
    if (stored.recordType != RecordType::CredentialV1) {
        logger.logPrint(A_STORED_CREDENTIAL_INVALID, LOGAREA_ACCESS, SEVERITY_ERROR, id);
        return false;
    }
    rec = stored;
    return true;
}

// Check if the credential attributes contain valid values (length, characters, date range)
bool validateCred(const CacheRecord& p) {
    if (!validateCredential(p.credential(), p.credType)) return false;
    const size_t nameLength = strnlen(p.name, MAX_NAME_LENGTH + 1);
    if (nameLength == 0 || nameLength > MAX_NAME_LENGTH)
        return false;
    if (p.validFrom && p.validTo && p.validFrom > p.validTo)
        return false;
    return true;
} // validateCred


// Insert new record into the chain, update prev and next records accordingly, return new credId
uint32_t CredStorage::writeCred(const CredRecord &credIn)
{
    CredRecord cred = credIn;
    CredRecord storedCred;
    CredRecord lastCred;
    CredRecord prevCred;
    char recKey[ID_LENGTH + 1];
    char prevKey[ID_LENGTH + 1];

    logger.logPrint(A_CREDENTIAL_WRITE, LOGAREA_ACCESS, SEVERITY_DEBUG, cred.credId, cred.name, cred.doorNum, cred.validFrom, cred.validTo, cred.remaining); // ###
    if (credIn.credId == 0 || !readData(credIn.credId, storedCred)) {   // not found, create new record
        if (!readData(LAST_KEY, lastCred)) {
            logger.logPrint(A_CREDSTORE_WRITECRED_FAILED_TO_READ_LAST_RECORD, LOGAREA_ACCESS, SEVERITY_ERROR);
            return 0;
        }
        cred.credId = lastCred.nextId;
        if (!readData(lastCred.prevId, prevCred)) {
            logger.logPrint(A_CREDSTORE_WRITECRED_FAILED_TO_READ_PREV_RECORD_WITH, LOGAREA_ACCESS, SEVERITY_ERROR, lastCred.prevId);
            return 0;
        }
        
        cred.nextId = LAST_KEY;
        cred.prevId = lastCred.prevId;
        prevCred.nextId = cred.credId;
        credKey(prevCred.credId, prevKey);
        writeData(prevCred);
        credKey(cred.credId, recKey);
        writeData(cred);
        lastCred.nextId = lastCred.nextId + 1;
        lastCred.prevId = cred.credId;
        int written = writeData(lastCred) ? sizeof(lastCred) : 0;
         if (written == 0 ) {
            logger.logPrint(A_CREDSTORE_UPDATE_FAILED_TO_WRITE_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, cred.credId);
            return 0;
        } else  {
            logger.logPrint(A_CREDSTORE_UPDATE_PARTIAL_WRITE_FOR_CREDID_WRITTEN_OF, LOGAREA_ACCESS, SEVERITY_ERROR, cred.credId, written, sizeof(cred)); //###
            
        }

    } else {                                            // found, update data   
        if (!verifyLocalChain(cred.credId)) {
            logger.logPrint(A_CREDSTORE_UPDATE_CREDID_NOT_PROPERLY_CHAINED, LOGAREA_ACCESS, SEVERITY_ERROR, cred.credId);
            return 0;
        }
 
        credKey(cred.credId, recKey);
        int written = writeData(cred) ? sizeof(cred) : 0;
        if (written == 0 ) {
            logger.logPrint(A_CREDSTORE_UPDATE_FAILED_TO_WRITE_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, cred.credId);
            return 0;
        } else  {
            logger.logPrint(A_CREDSTORE_UPDATE_PARTIAL_WRITE_FOR_CREDID_WRITTEN_OF, LOGAREA_ACCESS, SEVERITY_ERROR, cred.credId, written, sizeof(cred)); //###
            
        }
    }
    return cred.credId;
} //writeCred

// Verify that the record with the given key is correctly linked in the chain (prevId and nextId point to valid records and link back to this record)
bool CredStorage::verifyLocalChain(uint32_t credId) {
    CredRecord rec;
    CredRecord prevrec;
    CredRecord nextrec;

    if (!readData(credId, rec))
        return false;
    if (rec.credId == FIRST_KEY || rec.credId == LAST_KEY)
        return true;
    if (!readData(rec.prevId, prevrec))
         return false;
    if (!readData(rec.nextId, nextrec))
        return false;
    return prevrec.nextId == rec.credId && nextrec.prevId == rec.credId;
}

bool CredStorage::isActive(const CredRecord& rec) {
    time_t now = time(nullptr);
    if (rec.validFrom && now < rec.validFrom)
        return false;
    if (rec.validTo && now > rec.validTo)
        return false;
    if (rec.remaining == 0)
        return false;
    return true;
} // isActive

bool CredStorage::rebuildCache() {
    credsCache.clear();
    maxCredId = 0;

    uint32_t cur = FIRST_KEY;
    if (!prefs.isKey(FIRST_KEY_KEY)) {
        logger.logPrint(A_CREDSTORE_REBUILDCACHE_HEAD_UNREADABLE, LOGAREA_ACCESS, SEVERITY_ERROR);
        return false;
    }

    while (cur != LAST_KEY) {
        CredRecord curCred;
        CacheRecord cacheRec;

        if (!readData(cur, curCred)) {
            logger.logPrint(A_CREDSTORE_REBUILDCACHE_BROKEN_LINK_AT_ID, LOGAREA_ACCESS, SEVERITY_ERROR, cur);
            break;
        }

        // maxCredId tracks every real credId regardless of validity
        if (cur != FIRST_KEY && cur != LAST_KEY) {
            if (curCred.credId > maxCredId) {
                maxCredId = curCred.credId;
            }
            if (isActive(curCred)) {
                cacheRec = toCacheRecord(curCred);
                credsCache.push_back(cacheRec);
            }
        }

        cur = curCred.nextId;
    }

    logger.logPrint(A_CREDSTORE_CACHE_ACTIVE_RECORD_S_MAXCREDID, LOGAREA_ACCESS, SEVERITY_INFO, credsCache.size(), maxCredId);
    return true;
}

bool CredStorage::begin()
{
    // Keep the existing namespace: renaming the API must not discard stored data.
    if (!prefs.begin("pins", false)) {
        logger.logPrint(A_NVS_OPERATION_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, "open", FIRST_KEY);
        return false;
    }
    // TEMPORARY: discard the old, smaller test-data format. Remove after rollout.
    if (prefs.isKey(FIRST_KEY_KEY) && prefs.getBytesLength(FIRST_KEY_KEY) < sizeof(CredRecord)) {
        logger.logPrint(A_CREDSTORE_RESETTING_LEGACY_TEST_DATA_POOL, LOGAREA_ACCESS, SEVERITY_ERROR);
        resetPool();
    }
    garbageCollect();
    rebuildCache();
    return true;
}

bool CredStorage::refreshCache()
{
    return rebuildCache();
}

#ifdef BOX_SIMULATION
const std::vector<CacheRecord>& CredStorage::debugCache() const
{
    return credsCache;
}
#endif

bool CredStorage::writeData(const CredRecord& rec) {
    if (rec.recordType != RecordType::CredentialV1) {
        logger.logPrint(A_STORED_CREDENTIAL_INVALID, LOGAREA_ACCESS, SEVERITY_ERROR, rec.credId);
        return false;
    }
    char key[ID_LENGTH + 1];
    credKey(rec.credId, key);
    const bool written = prefs.putBytes(key, &rec, sizeof(rec)) == sizeof(rec);
    if (!written)
        logger.logPrint(A_NVS_OPERATION_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, "write", rec.credId);
    return written;
}

void CredStorage::resetPool() {
    if (!prefs.clear())
        logger.logPrint(A_NVS_OPERATION_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, "clear", FIRST_KEY);

    CredRecord head = {};
    head.credId = FIRST_KEY;
    head.prevId = FIRST_KEY;
    head.nextId = LAST_KEY;
    writeData(head);

    CredRecord tail = {};
    tail.credId = LAST_KEY;
    tail.prevId = FIRST_KEY;
    tail.nextId = 1;
    writeData(tail);

    credsCache.clear();
    maxCredId = 0;
    nextCredId = 1;
}

bool CredStorage::ensurePool() {
    CredRecord head = {};
    CredRecord tail = {};
    bool hasHead = readData(FIRST_KEY, head);
    bool hasTail = readData(LAST_KEY, tail);

    if (!hasHead && !hasTail) {
        logger.logPrint(A_CREDSTORE_GC_EMPTY_CREDENTIAL_POOL_CREATING_SENTINELS, LOGAREA_ACCESS, SEVERITY_INFO);
        resetPool();
        return false;
    }

    if (!hasHead || !hasTail || head.credId != FIRST_KEY || tail.credId != LAST_KEY) {
        logger.logPrint(A_CREDSTORE_GC_CORRUPTED_SENTINELS_RESETTING_CREDENTIAL_POOL, LOGAREA_ACCESS, SEVERITY_ERROR);
        resetPool();
        return false;
    }

    return true;
}

bool CredStorage::isStoredCredValid(const CredRecord& rec) {
    if (!isRealCredId(rec.credId)) {
        return false;
    }

    if (rec.recordType != RecordType::CredentialV1
        || !validateCredential(rec.credential(), rec.credType)
        || strnlen(rec.name, MAX_NAME_LENGTH + 1) > MAX_NAME_LENGTH) return false;
    CacheRecord cacheRec = toCacheRecord(rec);
    return validateCred(cacheRec);
}

// Going from HEAD to TAIL, find the last reachable record in the chain, return true the last valid points to TAIL, false if the chain is broken or exceeds the maximum number of steps
//bool CredStorage::findLastReachableFromHead(CredRecord& lastRec) {
//    CredRecord current = {};
//    if (!readData(FIRST_KEY, current) || current.credId != FIRST_KEY) {
//        return false;
//    }
//    for (uint32_t steps = 0; steps < CRED_GC_MAX_SCAN_ID; steps++) {
//        if (current.nextId == LAST_KEY) {
//            lastRec = current;
//            return true;
//        }
//
//        if (!isRealCredId(current.nextId)) {
//            return false;
//        }
//
//        CredRecord next = {};
//        if (!readData(current.nextId, next) || next.credId != current.nextId) {
//            return false;
//        }
//
//        if (!isStoredCredValid(next) || next.prevId != current.credId) {
//            return false;
//        }
//
//        current = next;
//    }
//
//    return false;
//} // findLastReachableFromHead

bool CredStorage::repairTailInsert() {
    CredRecord tail = {};
    if (!readData(LAST_KEY, tail) || tail.credId != LAST_KEY) {
        return false;
    }
    if (tail.prevId == FIRST_KEY) {         // empty pool, nothing to repair, at least form insertion point of view
        return true;
    }
    if (tail.prevId == LAST_KEY) {          // to avoid infinite loop, this is an invalid state, should not happen
        return false;
    }
    CredRecord tailPrev = {};
    if (!readData(tail.prevId, tailPrev) || tailPrev.credId != tail.prevId) {
        return false;
    }
    if (tailPrev.nextId == LAST_KEY) {      // no interrupted insert, everything is fine
        return true;
    }   

//    CredRecord lastReachable = {};
    CredRecord newRec = {};
//    if (!findLastReachableFromHead(lastReachable)) {
//        return false;
//    }

    if (!readData(tailPrev.nextId, newRec)) {  // after 1st step of insert ... rollback to last reachable record
//        if (!findLastReachableFromHead(lastReachable)) {
//            return false;
//        }
        logger.logPrint(A_CREDSTORE_GC_ROLLED_BACK_INTERRUPTED_INSERT_TAILPREV_NEXTID, LOGAREA_ACCESS, SEVERITY_ERROR, tailPrev.nextId);
        tailPrev.nextId = LAST_KEY;
        writeData(tailPrev);
    } else {
        logger.logPrint(A_CREDSTORE_GC_FOUND_INTERRUPTED_INSERT_TAILPREV_NEXTID, LOGAREA_ACCESS, SEVERITY_ERROR, tailPrev.nextId);
        if ( !isStoredCredValid(newRec) || newRec.nextId != LAST_KEY || newRec.prevId != tailPrev.credId) {
            return false;
        }
        tail.prevId = newRec.credId;
        tailPrev.nextId = newRec.credId + 1;
        writeData(tail);
    }
    return true;
} // repairTailInsert

bool CredStorage::repairDeleteGap(const CredRecord& prevRec, CredRecord& nextRec) {
    uint32_t deletedId = nextRec.prevId;
    if (!isRealCredId(deletedId) || deletedId == prevRec.credId || deletedId == nextRec.credId) {
        logger.logPrint(A_CREDSTORE_GC_TANGLED_CHAIN_RESETTING_CREDENTIAL_POOL, LOGAREA_ACCESS, SEVERITY_ERROR);
        return false;
    }

    CredRecord deleted = {};
    if (readData(deletedId, deleted)) {
        if (deleted.credId != deletedId
            || deleted.prevId != prevRec.credId
            || deleted.nextId != nextRec.credId
            || !isStoredCredValid(deleted)) {
                logger.logPrint(A_CREDSTORE_GC_CORRUPTED_DELETE_RECORD_CREDID_RESETTING_CREDENTIAL, LOGAREA_ACCESS, SEVERITY_ERROR, deletedId);
            return false;
        }
        char key[ID_LENGTH + 1];
        credKey(deletedId, key);
        if (!prefs.remove(key))
            logger.logPrint(A_NVS_OPERATION_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, "remove", deletedId);
        logger.logPrint(A_CREDSTORE_GC_REMOVED_INTERRUPTED_DELETE_RECORD_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, deletedId);
    } else {
        logger.logPrint(A_CREDSTORE_GC_COMPLETED_INTERRUPTED_DELETE_AFTER_REMOVED_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, deletedId);
    }

    nextRec.prevId = prevRec.credId;
    writeData(nextRec);
    return true;
} // repairDeleteGap

bool CredStorage::repairInterruptedDelete() {
    CredRecord current = {};
    if (!readData(FIRST_KEY, current) || current.credId != FIRST_KEY) {
        return false;
    } 

    for (uint32_t steps = 0; steps < CRED_GC_MAX_SCAN_ID; steps++) {
        if (current.nextId == FIRST_KEY || current.nextId == current.credId) {
            return false;
        }
        CredRecord next = {};
        if (!readData(current.nextId, next) || next.credId != current.nextId) {
            return false;
        }
        if (isRealCredId(next.credId) && !isStoredCredValid(next)) {
            return false;
        }
        if (next.prevId != current.credId) {
            if (!repairDeleteGap(current, next)) {
                return false;
            }
        }
        if (next.credId == LAST_KEY) {
            return true;
        }
        current = next;
    }

    return false;
} // repairInterruptedDelete

bool CredStorage::repairCredChain() {
    CredRecord current = {};
    CredRecord tail = {};
    CredRecord next = {};
    CredRecord deleted = {};
    if (!readData(LAST_KEY, tail) || tail.credId != LAST_KEY) {                    //tail present?
        return false;
    } 

    if (!readData(FIRST_KEY, current) || current.credId != FIRST_KEY) {              //head present?
        return false;
    } 

    for (uint32_t steps = 0; steps < CRED_GC_MAX_SCAN_ID; steps++) {
        logger.logPrint(A_CREDSTORE_GC_TRAVERSING_CHAIN_CURRENT_CREDID_NEXTID, LOGAREA_ACCESS, SEVERITY_DEBUG, current.credId, current.nextId);
        if (current.nextId == FIRST_KEY || current.nextId == current.credId) {
            return false;
        }
        if (!readData(current.nextId, next) ){
            if (current.credId == tail.prevId ) {                                    // interrupted insert in step 1, roll back it
                current.nextId = LAST_KEY;
                writeData(current);
                logger.logPrint(A_CREDSTORE_GC_ROLLED_BACK_BROKEN_INSERT, LOGAREA_ACCESS, SEVERITY_ERROR);
                return true;
            } else {                                                                // severely damaged chain, cannot repair
                logger.logPrint(A_CREDSTORE_GC_INTERRUPTED_CHAIN_RESETTING_CREDENTIAL_POOL, LOGAREA_ACCESS, SEVERITY_ERROR);
                return false;
            }
        }
        if( next.credId != current.nextId) {
            logger.logPrint(A_CREDSTORE_GC_UNEXPECTED_ID_IN_CHAIN_RESETTING_CREDENTIAL, LOGAREA_ACCESS, SEVERITY_ERROR);
            return false;
        }
        if (isRealCredId(next.credId) && !isStoredCredValid(next)) {
            logger.logPrint(A_CREDSTORE_GC_DUBIOUS_RECORD_IN_CHAIN_RESETTING_CREDENTIAL, LOGAREA_ACCESS, SEVERITY_ERROR);
            return false;
        }
        if (next.prevId != current.credId) {
            if (next.prevId == current.prevId) {                                    // interrupted insert in step 2, complete it
                if (next.credId == LAST_KEY) {
                    logger.logPrint(A_CREDSTORE_GC_REDOING_UNFINISHED_INSERT_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, current.credId);
                    tail.prevId = current.credId;
                    tail.nextId = current.credId + 1;
                    writeData(tail);
                    return true;
                }
            } 
            if (readData(next.prevId, deleted) && next.prevId == deleted.credId) {    // interrupted delete in step 1, complete it
                char key[ID_LENGTH + 1];
                credKey(deleted.credId, key);
                if (!prefs.remove(key))
                    logger.logPrint(A_NVS_OPERATION_FAILED, LOGAREA_ACCESS, SEVERITY_ERROR, "remove", deleted.credId);
                logger.logPrint(A_CREDSTORE_GC_REMOVED_INTERRUPTED_DELETE_RECORD_CREDID, LOGAREA_ACCESS, SEVERITY_ERROR, deleted.credId);
            }
            if (!readData(next.prevId, deleted)) {                                      // interrupted delete in step 2, complete it
                next.prevId = current.credId;
                writeData(next);
                logger.logPrint(A_CREDSTORE_GC_COMPLETED_INTERRUPTED_DELETE, LOGAREA_ACCESS, SEVERITY_ERROR);
                return true;
            }
            logger.logPrint(A_CREDSTORE_GC_BROKEN_CHAIN_RESETTING_CREDENTIAL_POOL, LOGAREA_ACCESS, SEVERITY_ERROR);
            return false;
            if (!repairDeleteGap(current, next)) {
                return false;
            }
        }
        if (next.credId == LAST_KEY) {
            return true;
        }
        current = next;
    }
    logger.logPrint(A_CREDSTORE_GC_UNABLE_TO_TRAVERSE_CHAIN_RESETTING_POOL, LOGAREA_ACCESS, SEVERITY_ERROR, current.credId);
    return false;
} // repairCredChain


void CredStorage::garbageCollect()
{
    if (!ensurePool()) {
        return;
    }
    if (!repairCredChain()) {
        logger.logPrint(A_CREDSTORE_GC_UNRECOVERABLE_CHAIN_STATE_RESETTING_CREDENTIAL_POOL, LOGAREA_ACCESS, SEVERITY_ERROR);
        resetPool();

    }

//    if (!repairTailInsert()) {
// Historical log site retired during the structured logging migration.
//        resetPool();
//        return;
//    }

//    if (!repairInterruptedDelete()) {
// Historical log site retired during the structured logging migration.
//        resetPool();
//        return;
//    }
    logger.logPrint(A_CREDSTORE_GC_COMPLETED, LOGAREA_ACCESS, SEVERITY_INFO);
}
