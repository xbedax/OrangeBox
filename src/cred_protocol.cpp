#include "cred_protocol.h"
#include <cstring>
#include <cstdio>

namespace {
struct Fields {
    const char* id;
    const char* name;
    const char* value;
    const char* from;
    const char* to;
};

Fields fieldsFor(CredType type) {
    if (type == CredType::Password)
        return {MSG_PINID, MSG_PINNAME, MSG_PINVALUE, MSG_PINVALIDFROM, MSG_PINVALIDTO};
    return {MSG_CODEID, MSG_CODENAME, MSG_CODEVALUE, MSG_CODEVALIDFROM, MSG_CODEVALIDTO};
}

bool parseType(JsonVariantConst value, CredType& type) {
    const char* text = value.as<const char*>();
    if (!text) return false;
    if (strcmp(text, "ctPin") == 0) type = CredType::Password;
    else if (strcmp(text, "ctCode") == 0) type = CredType::PacketNumber;
    else return false;
    return true;
}

bool fail(JsonDocument& response, const char* reason) {
    response["success"] = false;
    response["error"] = reason;
    response[MSG_LASTRESULT] = reason;
    return false;
}

// Form fields arrive as strings; native clients may send JSON integers.
bool integer(JsonVariantConst value, int64_t minimum, int64_t maximum, int64_t& result) {
    if (value.is<bool>()) return false;
    if (value.is<int64_t>()) {
        result = value.as<int64_t>();
        return result >= minimum && result <= maximum;
    }
    const char* text = value.as<const char*>();
    if (!text || !*text) return false;
    bool negative = *text == '-';
    if (negative) ++text;
    if (!*text) return false;
    int64_t number = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        // All protocol numeric fields fit in uint32_t (or signed int32_t).
        number = number * 10 + (*text - '0');
        if (number > UINT32_MAX) return false;
    }
    result = negative ? -number : number;
    return result >= minimum && result <= maximum;
}

bool leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
int monthDays(int year, int month) {
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return lengths[month - 1] + (month == 2 && leap(year) ? 1 : 0);
}
int64_t yearDays(int year) {
    const int64_t y = year - 1;
    return y * 365 + y / 4 - y / 100 + y / 400;
}

// Calendar dates use UTC, independently of host timezone and 32-bit time_t.
bool parseDate(JsonVariantConst value, bool endOfDay, uint64_t& timestamp) {
    const char* text = value.as<const char*>();
    if (!text) return false;
    if (*text == '\0' || strcmp(text, endOfDay ? "9999-12-31" : "0001-01-01") == 0) {
        timestamp = 0;
        return true;
    }
    if (strlen(text) != 10 || text[4] != '-' || text[7] != '-') return false;
    for (int i = 0; i < 10; ++i)
        if (i != 4 && i != 7 && (text[i] < '0' || text[i] > '9')) return false;
    const int year = (text[0] - '0') * 1000 + (text[1] - '0') * 100 + (text[2] - '0') * 10 + text[3] - '0';
    const int month = (text[5] - '0') * 10 + text[6] - '0';
    const int day = (text[8] - '0') * 10 + text[9] - '0';
    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > monthDays(year, month)) return false;
    int64_t days = yearDays(year) - yearDays(1970) + day - 1;
    for (int m = 1; m < month; ++m) days += monthDays(year, m);
    timestamp = static_cast<uint64_t>(days) * 86400 + (endOfDay ? 86399 : 0);
    return true;
}

std::string formatDate(uint64_t timestamp, bool endOfDay) {
    if (timestamp == 0) return endOfDay ? "9999-12-31" : "0001-01-01";
    uint64_t days = timestamp / 86400;
    if (days >= static_cast<uint64_t>(yearDays(10000) - yearDays(1970))) return "9999-12-31";
    int year = 1970 + static_cast<int>(days / 366);
    while (yearDays(year + 1) - yearDays(1970) <= static_cast<int64_t>(days)) ++year;
    days -= yearDays(year) - yearDays(1970);
    int month = 1;
    while (days >= static_cast<uint64_t>(monthDays(year, month))) days -= monthDays(year, month++);
    char text[11];
    snprintf(text, sizeof(text), "%04d-%02d-%02u", year, month, static_cast<unsigned>(days + 1));
    return std::string(text);
}

bool copyText(JsonVariantConst value, char* target, size_t capacity) {
    const char* text = value.as<const char*>();
    if (!text || !*text || strlen(text) >= capacity) return false;
    strcpy(target, text);
    return true;
}
} // namespace

const char* credTypeName(CredType type) {
    return type == CredType::Password ? "ctPin" : type == CredType::PacketNumber ? "ctCode" : "all";
}

bool buildCredResponse(CredStorage& storage, JsonObjectConst request, JsonDocument& response) {
    response.clear();
    CredType type;
    if (!parseType(request["credType"], type)) return fail(response, "invalid_cred_type");
    response["credType"] = credTypeName(type);
    int64_t first = 0, count = 15;
    if ((!request["credId"].isNull() && !integer(request["credId"], 0, LAST_KEY - 1, first))
        || (!request["credCount"].isNull() && !integer(request["credCount"], 0, 100, count)))
        return fail(response, "invalid_pagination");
    const Fields fields = fieldsFor(type);
    JsonObject rows = response["rows"].to<JsonObject>();
    JsonArray ids = rows[fields.id].to<JsonArray>();
    JsonArray names = rows[fields.name].to<JsonArray>();
    JsonArray values = rows[fields.value].to<JsonArray>();
    JsonArray from = rows[fields.from].to<JsonArray>();
    JsonArray to = rows[fields.to].to<JsonArray>();
    JsonArray amounts;
    if (type == CredType::Password) amounts = rows[MSG_REMAINING].to<JsonArray>();
    std::vector<CacheRecord> creds;
    storage.getCreds(static_cast<uint32_t>(first), static_cast<uint32_t>(count), creds, type);
    for (const auto& cred : creds) {
        ids.add(cred.credId);
        names.add(std::string(cred.name));
        values.add(std::string(cred.credential()));
        from.add(formatDate(cred.validFrom, false));
        to.add(formatDate(cred.validTo, true));
        if (type == CredType::Password) amounts.add(std::to_string(cred.remaining));
    }
    response["rowCount"] = creds.size();
    return true;
}

bool applyCredChange(CredStorage& storage, JsonObjectConst request, JsonDocument& response) {
    response.clear();
    CredType type;
    if (!parseType(request["credType"], type)) return fail(response, "invalid_cred_type");
    response["credType"] = credTypeName(type);
    const Fields fields = fieldsFor(type);
    const char* action = request["clicked"].as<const char*>();
    if (!action || (strcmp(action, CRED_SAVE_BEACON) != 0 && strcmp(action, CRED_DELETE_BEACON) != 0))
        return fail(response, "invalid_action");
    const bool deleting = strcmp(action, CRED_DELETE_BEACON) == 0;
    int64_t id;
    if (!integer(request[fields.id], 0, LAST_KEY - 1, id) || (deleting && id == 0))
        return fail(response, "invalid_cred_id");
    CacheRecord cred = {};
    cred.credType = type;
    cred.credId = static_cast<uint32_t>(id);
    cred.remaining = type == CredType::PacketNumber ? 1 : -1;
    if (id && !storage.getCred(cred.credId, cred, type)) return fail(response, "cred_not_found");
    if (!copyText(request[fields.name], cred.name, sizeof(cred.name))) return fail(response, "invalid_cred_name");
    if (type == CredType::Password) {
        if (!copyText(request[fields.value], cred.pin, sizeof(cred.pin))) return fail(response, "invalid_cred_value");
    } else {
        if (!copyText(request[fields.value], cred.packetNumber, sizeof(cred.packetNumber)))
            return fail(response, "invalid_cred_value");
    }
    if (!deleting) {
        if ((!request[fields.from].isNull() && !parseDate(request[fields.from], false, cred.validFrom))
            || (!request[fields.to].isNull() && !parseDate(request[fields.to], true, cred.validTo))
            || (cred.validFrom && cred.validTo && cred.validFrom > cred.validTo))
            return fail(response, "invalid_date_range");
        if (!request[MSG_DOORNUM].isNull()) {
            int64_t door;
            if (!integer(request[MSG_DOORNUM], 0, 254, door)) return fail(response, "invalid_door");
            cred.doorNum = static_cast<uint8_t>(door);
        }
        if (type == CredType::Password) {
            JsonVariantConst unlimited = request[MSG_PINUNLIMITED];
            if (!unlimited.isNull() && !unlimited.is<bool>()) return fail(response, "invalid_unlimited");
            if (unlimited.is<bool>() && unlimited.as<bool>()) cred.remaining = -1;
            else if (!request[MSG_REMAINING].isNull()) {
                int64_t amount;
                if (!integer(request[MSG_REMAINING], -1, INT32_MAX, amount)
                    || (unlimited.is<bool>() && amount < 0)) return fail(response, "invalid_amount");
                cred.remaining = static_cast<int32_t>(amount);
            } else if (unlimited.is<bool>()) return fail(response, "invalid_amount");
        }
    }
    bool changed;
    if (deleting) changed = storage.removeCred(cred) == cred.credId;
    else if (id) changed = storage.updateCred(cred);
    else {
        cred.credId = storage.addCred(cred);
        changed = cred.credId != 0;
    }
    if (!changed) return fail(response, "cred_change_failed");
    response["success"] = true;
    response["credId"] = cred.credId;
    response["action"] = deleting ? "deleted" : id ? "updated" : "created";
    response[MSG_LASTRESULT] = deleting ? "Credential deleted" : "Credential saved";
    return true;
}
