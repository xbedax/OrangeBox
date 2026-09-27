#pragma once

#include <ArduinoJson.h>
#include "pass_store.h"

// Transport-independent handlers; main.cpp supplies WebSocket delivery.
bool buildCredResponse(CredStorage& storage, JsonObjectConst request, JsonDocument& response);
bool applyCredChange(CredStorage& storage, JsonObjectConst request, JsonDocument& response);
const char* credTypeName(CredType type);
