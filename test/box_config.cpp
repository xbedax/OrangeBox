#include "box_config.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <LittleFS.h>
#include <esp_partition.h>
#include "provisioning_payload.h"
#include "qr_scanner.h"

int main() {
    ProvisioningPayload payload;
    auto qr = [&](const char* json) {
        return ProvisioningPayload::parse(reinterpret_cast<const uint8_t*>(json), strlen(json), payload);
    };
    const char* validQr = R"({"ID":"pastika-nemrcouch-zazpivat","NAME":"Box-089","W_SSID":"MojeWifi","W_PASS":"tajneHeslo7","SRV":"prov.inforoom.cz","PORT":8085,"NONCE":"09745697489465"})";
    assert(qr(validQr));
    assert(payload.port == 8085);
    assert(payload.tokenInput() == String("09745697489465|pastika-nemrcouch-zazpivat|Box-089"));
    JsonDocument bootstrapDoc;
    assert(!deserializeJson(bootstrapDoc, validQr));
    auto invalidQr = [&]() {
        std::string serialized;
        serializeJson(bootstrapDoc, serialized);
        return !qr(serialized.c_str());
    };
    bootstrapDoc["PORT"] = "8085"; assert(invalidQr());
    bootstrapDoc["PORT"] = 65536; assert(invalidQr());
    bootstrapDoc["PORT"] = 0; assert(invalidQr());
    bootstrapDoc["PORT"] = 8085;
    bootstrapDoc["SRV"] = "https://prov.inforoom.cz"; assert(invalidQr());
    bootstrapDoc["SRV"] = "prov.inforoom.cz/path"; assert(invalidQr());
    bootstrapDoc["SRV"] = "prov..inforoom.cz"; assert(invalidQr());
    bootstrapDoc["SRV"] = "prov.inforoom.cz";
    bootstrapDoc["NONCE"] = nullptr; assert(invalidQr());
    bootstrapDoc["NONCE"] = "has|separator"; assert(invalidQr());
    assert(payload.port == 8085 && payload.host == String("prov.inforoom.cz"));
    assert(!qr("not JSON"));
    // Provisioning payloads may exceed the previous scanner limit of 160 bytes.
    bootstrapDoc["NONCE"] = "09745697489465";
    bootstrapDoc["ID"] = std::string(100, 'a');
    std::string longQr;
    serializeJson(bootstrapDoc, longQr);
    assert(longQr.size() > 160);
    Stream uart;
    QrScanner scanner;
    QrScannerConfig scannerConfig;
    scannerConfig.activationResponseWindowMs = 0;
    scanner.begin(uart, scannerConfig);
    assert(scanner.startScan(60000));
    assert(scanner.update().type == QrScannerEventType::Activated);
    for (char c : longQr) uart.input.push_back(c);
    uart.input.push_back('\r');
    auto frame = scanner.update();
    assert(frame.type == QrScannerEventType::CodeReceived && frame.length == longQr.size());
    assert(ProvisioningPayload::parse(frame.data, frame.length, payload));
    BoxConfigStore store;
    BoxConfig config;
    const String defaultSsid = config.wifiSsid;
    String error;
    JsonDocument doc;
    auto decode = [&](const char* json, bool complete = false) {
        error = "";
        assert(!deserializeJson(doc, json));
        return store.decode(doc.as<JsonVariantConst>(), config, error, complete);
    };
    assert(decode(R"({"sandbox":{"BOX_HOST_NAME":"override","BOX_PROVISIONED":0,"QR_SCANNER_TEST_KEEP_CRED":false}})"));
    assert(config.hostName == String("override"));
    assert(config.wifiSsid == defaultSsid);
    assert(!config.provisioned && !config.scannerTestKeepCred);
    assert(!decode(R"({"sandbox":{"BOX_HOST_NAME":"must-not-apply","VPN_GATEWAY_PORT":70000}})"));
    assert(config.hostName == String("override"));
    assert(!decode(R"({"sandbox":{"WIFI_SSID":null}})"));
    assert(!decode(R"({"sandbox":{"BOX_PROVISIONED":"1"}})"));
    assert(!decode(R"({"sandbox":{"DISP_LANG":2}})"));
    assert(!decode(R"({"sandbox":{"WIFI_SSID":"abc\u0000def"}})"));
    assert(!decode(R"({"sandbox":{"ACTIVE_DOOR_NUMBERS":2}})"));
    assert(!decode(R"({"sandbox":{"INITIAL_DOOR_MAPPING":[[0,1,39,0]]}})"));
    assert(!decode(R"({"sandbox":{"INITIAL_DOOR_MAPPING":[[0,250,39,0,0]]}})"));
    assert(!decode(R"({"sandbox":{"INITIAL_DOOR_MAPPING":[[0,1,39,1,0]]}})"));
    assert(!decode(R"({"tombox":{"BOX_PROVISIONED":1}})"));
    assert(!decode(R"({"sandbox":{"BOX_PROVISIONED":1}})", true));
    assert(decode(R"({"sandbox":{"ACTIVE_DOOR_NUMBERS":2,"INITIAL_DOOR_MAPPING":[[0,1,39,0,0],[1,2,40,0,1]],"WIFI_PASSWD":""}})"));
    assert(config.doorCount == 2 && config.doors[1].statePin == 40);
    std::ifstream example("examples/box_config.sandbox.json");
    assert(example && !deserializeJson(doc, example));
    error = "";
    assert(store.decode(doc.as<JsonVariantConst>(), config, error, true));
    assert(config.provisioned && config.hostName == String("Box-089"));
    doc["sandbox"]["BOX_PROVISIONED"] = 0;
    assert(!store.decode(doc.as<JsonVariantConst>(), config, error, true));
    assert(config.provisioned); // failed update remains transactional
    assert(store.begin());
    assert(store.load(config, error)); // absent file preserves compiled/runtime values
    LittleFS.files[BoxConfigStore::Path] = "{";
    assert(!store.load(config, error));
    assert(config.provisioned);
    LittleFS.files[BoxConfigStore::Path] = R"({"sandbox":{"BOX_HOST_NAME":"persisted"}})";
    assert(store.load(config, error));
    assert(config.hostName == String("persisted"));
    const std::string original = LittleFS.files[BoxConfigStore::Path];
    LittleFS.files[BoxConfigStore::PendingPath] = R"({"sandbox":{"BOX_PROVISIONED":1}})";
    assert(!store.commitDownloaded(error));
    assert(LittleFS.files[BoxConfigStore::Path] == original);
    doc["sandbox"]["BOX_PROVISIONED"] = 1;
    std::string serialized;
    serializeJson(doc, serialized);
    LittleFS.files[BoxConfigStore::PendingPath] = serialized;
    LittleFS.failRename = true;
    assert(!store.commitDownloaded(error));
    assert(LittleFS.files[BoxConfigStore::Path] == original);
    LittleFS.failRename = false;
    assert(store.commitDownloaded(error));
    assert(LittleFS.files[BoxConfigStore::Path] == serialized);
    assert(store.load(config, error));
    assert(config.hostName == String("Box-089"));
    LittleFS.files[BoxConfigStore::Path] = std::string(BoxConfigStore::MaxBytes + 1, ' ');
    assert(!store.load(config, error));
    LittleFS.mountOk = false;
    assert(!store.begin()); // no partition found: never format
    testPartitionBytes.assign(4096, 0xff);
    testPartitionBytes.back() = 0; // even one non-erased byte must prevent formatting
    assert(!store.begin());
    assert(LittleFS.formatCount == 0);
    testPartitionBytes.back() = 0xff;
    assert(store.begin());
    assert(LittleFS.formatCount == 1);
    std::cout << "Configuration, storage and provisioning QR tests passed\n";
}
