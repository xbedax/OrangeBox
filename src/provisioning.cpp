#include "provisioning.h"
#include "provisioning_payload.h"
#include "box_config.h"
#include "box_display.h"
#include "keyboard.h"
#include "qr_scanner.h"
#include "qr_payload.h"
#include "timeout.h"
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>

namespace {
String downloadUrl(const ProvisioningPayload& bootstrap) {
    String input = bootstrap.tokenInput();
    uint8_t digest[32];
    if (mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(input.c_str()), input.length(), digest, 0) != 0) return "";
    char hex[65];
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    return "https://" + bootstrap.host + ":" + String(bootstrap.port) + "/" + hex;
}

bool download(const ProvisioningPayload& bootstrap, BoxConfigStore& store, String& error) {
    if (!strlen(BOX_PROVISIONING_CA)) { error = "Chybi CA serveru"; return false; }
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.setHostname(bootstrap.name.c_str());
    WiFi.begin(bootstrap.ssid.c_str(), bootstrap.password.c_str());
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && !timeoutElapsed(started, 30000, millis())) delay(20);
    if (WiFi.status() != WL_CONNECTED) { error = "WiFi nepripojena"; return false; }
    // Certificate dates need valid time even on a new device without a set RTC.
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    started = millis();
    while (time(nullptr) < RTC_MIN_VALID_UNIX_TIME && !timeoutElapsed(started, 20000, millis())) delay(20);
    if (time(nullptr) < RTC_MIN_VALID_UNIX_TIME) { error = "Nelze overit cas"; return false; }
    WiFiClientSecure tls;
    tls.setCACert(BOX_PROVISIONING_CA);
    tls.setHandshakeTimeout(15);
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    String url = downloadUrl(bootstrap);
    if (url.isEmpty() || !http.begin(tls, url)) { error = "Chyba HTTPS"; return false; }
    int status = http.GET();
    int size = http.getSize();
    if (status != HTTP_CODE_OK || size <= 0 || size > static_cast<int>(BoxConfigStore::MaxBytes)) {
        http.end(); error = "Chyba odpovedi HTTP"; return false;
    }
    File file = LittleFS.open(BoxConfigStore::PendingPath, "w");
    if (!file) { http.end(); error = "Chyba zapisu LittleFS"; return false; }
    int written = http.writeToStream(&file);
    file.flush();
    size_t stored = file.size();
    file.close();
    http.end();
    bool ok = written == size && stored == static_cast<size_t>(size);
    if (ok) ok = store.commitDownloaded(error);
    else error = "Neuplna odpoved HTTP";
    if (!ok) LittleFS.remove(BoxConfigStore::PendingPath);
    return ok;
}
}

void runProvisioning(BoxConfigStore& store, BoxDisplay& display, BoxKeyboard& keyboard, HardwareSerial& uart) {
    keyboard.keyboardInit();
    keyboardStatus keys = {};
    keys.keyboardMode = KEYBOARD_MODE_PASSWORD;
    display.writeProvisioning("Chybi konfigurace", "Inicializacni heslo");
    if (strlen(BOX_INIT_PASSWORD) != 8 || strlen(BOX_INIT_PASSWORD) > PASS_MAX
        || strspn(BOX_INIT_PASSWORD, "0123456789") != strlen(BOX_INIT_PASSWORD)) {
        display.writeProvisioning("Chybi konfigurace", "Nastavte heslo FW");
        while (true) delay(100);
    }
    if (!strlen(BOX_PROVISIONING_CA)) {
        display.writeProvisioning("Chybi konfigurace", "Nastavte CA ve FW");
        while (true) delay(100);
    }
    uint32_t lastInput = millis();
    while (true) {
        uint8_t previous = keys.currentPasswordLen;
        keyboard.pollProvisioningPassword(&keys);
        if (keys.currentPasswordLen != previous) {
            lastInput = millis();
            char mask[PASS_MAX + 1] = {};
            memset(mask, '*', keys.currentPasswordLen);
            display.writeProvisioning("Chybi konfigurace", "Inicializacni heslo", mask);
        }
        if (keys.passwordComplete) {
            bool accepted = strcmp(keys.currentPassword, BOX_INIT_PASSWORD) == 0;
            memset(keys.currentPassword, 0, sizeof(keys.currentPassword));
            keys.passwordComplete = false;
            keys.currentPasswordLen = 0;
            if (accepted) break;
            display.writeProvisioning("Chybi konfigurace", "Spatne heslo");
            delay(PASS_ERR_DELAY * 1000UL);
            lastInput = millis();
            display.writeProvisioning("Chybi konfigurace", "Inicializacni heslo");
        }
        if (keys.cancelPressed || timeoutElapsed(lastInput, PASS_ENTRY_TIMEOUT * 1000UL, millis())) {
            memset(&keys, 0, sizeof(keys));
            keys.keyboardMode = KEYBOARD_MODE_PASSWORD;
            display.writeProvisioning("Chybi konfigurace", "Inicializacni heslo");
            lastInput = millis();
        }
        delay(10);
    }
    QrScanner scanner;
    uint8_t activate[160], deactivate[160];
    size_t activateLength, deactivateLength;
    if (!QrPayload::parse(QR_SCANNER_ACTIVATE_COMMAND, activate, sizeof(activate), activateLength)
        || !QrPayload::parse(QR_SCANNER_DEACTIVATE_COMMAND, deactivate, sizeof(deactivate), deactivateLength)) {
        display.writeProvisioning("Chyba ctecky", "Opravte nastaveni FW");
        while (true) delay(100);
    }
    uart.setRxBufferSize(2048);
    uart.begin(QR_SCANNER_BAUD, SERIAL_8N1, QR_SCANNER_RX_PIN, QR_SCANNER_TX_PIN);
    QrScannerConfig cfg;
    cfg.activateCommand = {activate, activateLength};
    cfg.deactivateCommand = {deactivate, deactivateLength};
    cfg.switchPin = QR_SCANNER_SWITCH_PIN;
    cfg.switchActiveHigh = QR_SCANNER_SWITCH_ACTIVE_HIGH != 0;
    cfg.activationMode = activateLength
        ? (cfg.switchPin == QR_SCANNER_PIN_UNUSED ? QrScannerActivationMode::Command : QrScannerActivationMode::CommandAndSwitch)
        : (cfg.switchPin == QR_SCANNER_PIN_UNUSED ? QrScannerActivationMode::Manual : QrScannerActivationMode::SwitchPin);
    cfg.activationResponseWindowMs = QR_SCANNER_RESPONSE_WINDOW_MS;
    cfg.frameIdleMs = QR_SCANNER_FRAME_IDLE_MS;
    // A QR must contain compact JSON on a single line; CR/LF delimit frames.
    scanner.begin(uart, cfg);
    while (true) {
        while (uart.available()) uart.read();
        display.writeProvisioning("Inicializace boxu", "Naskenujte QR kod");
        scanner.startScan(60000);
        while (scanner.isActive()) {
            QrScannerEvent event = scanner.update();
            if (event.type == QrScannerEventType::RxOverflow) {
                scanner.deactivate();
                display.writeProvisioning("Chyba QR kodu", "Prilis dlouhy kod");
                delay(2000); break;
            }
            if (event.type == QrScannerEventType::CodeReceived
                || (event.type == QrScannerEventType::ActivationResponse && event.length && event.data[0] == '{')) {
                ProvisioningPayload bootstrap;
                bool valid = ProvisioningPayload::parse(event.data, event.length, bootstrap);
                scanner.deactivate();
                String error;
                if (valid) {
                    display.writeProvisioning("Inicializace boxu", "Stahuji konfiguraci");
                    if (download(bootstrap, store, error)) {
                        display.writeProvisioning("Konfigurace ulozena", "Restartuji box");
                        delay(1000);
                        ESP.restart();
                        while (true) delay(100);
                    }
                } else error = "Neplatny QR kod";
                WiFi.disconnect(true, false);
                display.writeProvisioning("Inicializace selhala", error.c_str());
                delay(3000);
                break;
            }
            delay(5);
        }
    }
}
