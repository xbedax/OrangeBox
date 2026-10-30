# Provisioning konfigurace boxu

Firmware při startu připojí LittleFS a načte `/box_config.json`. JSON má kořenový
objekt `sandbox`; jeho položky odpovídají názvům maker v `box_setup.h`.
Přítomné hodnoty přepisují výchozí konfiguraci z firmware, chybějící zůstávají
výchozí. Nesprávný typ, rozsah nebo struktura odmítne celý soubor bez částečné
změny. Důvod se vypíše na Serial bez obsahu hesel a klíčů.

Úplná šablona se zástupnými přihlašovacími údaji je v
[`examples/box_config.sandbox.json`](../examples/box_config.sandbox.json).
Nepoužívat zástupné VPN klíče v provozu. `DISP_LANG` je 0 (EN) nebo 1 (CS);
německé texty v současném displeji nejsou implementované.
`QR_SCANNER_TEST_KEEP_CRED` je JSON boolean (přijímá se také 0/1), takže i
testovací chování čtečky lze přepsat bez rekompilace.
`INITIAL_DOOR_MAPPING` je pole pětic `[logDoorNum, gpioPin, statePin, extenderNum,
lastState]`; počet musí odpovídat `ACTIVE_DOOR_NUMBERS` (1–20).
Současný GPIO HAL podporuje pouze `extenderNum = 0`. Pin 255 znamená nepoužitý
pin. Čísla pinů musí být platná pro cílové ESP32. Nastavení I2C displeje,
klávesnice a UART čtečky zůstávají ve firmware, aby šlo zařízení inicializovat
i bez vlastní konfigurace HW.

Po načtení se vyhodnotí běhová hodnota `BOX_PROVISIONED`. Při 0 se ihned po
inicializaci displeje zastaví běžný start. Nespouští se obsluha dveří,
provozní stavový automat, webserver ani VPN. Displej žádá inicializační heslo.

- `BOX_INIT_PASSWORD` v `include/config.h`: přesně osm číslic, výchozí
  vývojová hodnota `12345678`. Pro nasazení změnit.
- Potvrzení `#`, mazání `*`, timeout zadávání 30 sekund. Chybné heslo má
  prodlevu podle `PASS_ERR_DELAY`.
- Po správném hesle se aktivuje čtečka. Chybný kód nebo síťová chyba umožní
  další sken; běžný provoz se při chybě nespustí.

Pro test továrního startu nastavit `BOX_PROVISIONED` v `box_setup.h` na 0
a použít LittleFS bez existující provisionované konfigurace. Hodnota 1
v uloženém JSON má přednost i před továrním firmware s hodnotou 0.

## Inicializační QR

QR musí obsahovat jednořádkový JSON v UTF-8, nejvýše 1024 bajtů. Čtečka
ukončuje rámce CR/LF; nepoužívat víceřádkový, odsazený JSON.

```json
{"ID":"pastika-nemrcouch-zazpivat","NAME":"Box-089","W_SSID":"MojeWifi","W_PASS":"replace-me","SRV":"prov.inforoom.cz","PORT":8085,"NONCE":"09745697489465"}
```

Všechny položky jsou povinné. `PORT` je číslo 1–65535, ostatní jsou řetězce.
`W_PASS` smí být prázdné pro otevřenou Wi-Fi. `SRV` obsahuje pouze hostname
bez schématu, portu a cesty. `ID`, `NAME` a `NONCE` nesmějí obsahovat `|`.
Wi-Fi z QR slouží ke stažení; po restartu platí Wi-Fi ze stažené konfigurace.

## HTTPS rozhraní serveru

Firmware provede GET na:

```text
https://<SRV>:<PORT>/<sha256(NONCE + "|" + ID + "|" + NAME)>
```

Hash je SHA-256 bajtů UTF-8 bez koncového newline/NUL, zapsaný jako 64 hex
znaků malými písmeny. Například v Pythonu:

```python
import hashlib
token = hashlib.sha256(f"{nonce}|{box_id}|{name}".encode("utf-8")).hexdigest()
```

Server musí vrátit HTTP 200, `Content-Length` 1–32768 bajtů a úplný JSON
podle šablony včetně `BOX_PROVISIONED: 1`. Přesměrování, odpovědi bez délky
a chunked odpovědi se odmítají. Všechny položky šablony jsou při provisioningu
povinné, aby se nepřevzaly cizí vývojové přihlašovací údaje a mapování HW.
U běžného lokálního souboru jsou naopak povolené částečné přepisy.

TLS ověřuje řetězec certifikátů, hostname i platnost proti dodané autoritě
IRCA v `include/provisioning_ca.h`. SHA-256 otisk jejího DER certifikátu:

```text
34e8e5688e48591f33db51e41c8be4e0b5256edc7ae898f3a822e32d3571320d
```

Platnost kořenového certifikátu: 2025-10-09 až 2035-10-09. Pokud server používá
intermediate CA, musí posílat odpovídající řetězec. `BOX_PROVISIONING_CA` lze
nahradit při sestavení; prázdná CA provisioning zablokuje. Nepoužívá se
`setInsecure()`. Před HTTPS se použije čas z RTC nebo se počká na NTP
(`pool.ntp.org`, `time.cloudflare.com`). Síť musí umožnit DNS a v případě
nenastavených hodin také NTP. Wi-Fi má timeout 30 s, NTP 20 s,
HTTPS spojení 10 s, čtení 15 s a TLS handshake 15 s.

Stažený soubor se zapisuje do `/box_config.pending`, ověří se délka, JSON,
typy, rozsahy, úplnost a provisionovaný stav. Teprve potom LittleFS rename
atomicky nahradí `/box_config.json`. Box zobrazí úspěch a restartuje se.
Nedokončený pending soubor se při startu nepoužívá.

PlatformIO používá `board_build.filesystem = littlefs`. Zcela vymazaná partition
`spiffs` se při prvním startu inicializuje automaticky, po ověření všech jejích
bajtů. Existující ani poškozená partition se automaticky neformátuje.
Při chybě připojení LittleFS se vypíše diagnostika na Serial; před dalším
provisioningem je potřeba opravit souborový systém.

## Ověření

```powershell
pio run -e esp32-s3-n16r16
powershell -ExecutionPolicy Bypass -File test/run_box_config.ps1
powershell -ExecutionPolicy Bypass -File test/run_scanner_integration.ps1
```

Nativní testy pokrývají prioritu JSON, zachování výchozích hodnot, odmítnutí
chybných položek bez částečné změny, validaci mapování, kompletní odpověď serveru,
čtení LittleFS, neúplný pending soubor, selhání rename i inicializaci pouze
prokazatelně prázdné partition. Testy čtečky kontrolují i běžný provoz a oba
režimy spotřebovávání přihlašovacích údajů.

Ruční HW test: tovární start → chybné/správné heslo → chybný/platný QR →
nedostupná Wi-Fi → nedůvěryhodný HTTPS certifikát → neúplný JSON → úspěšné
uložení a restart. Po restartu ověřit hodnoty Wi-Fi, VPN, jazyka a mapování
z načteného souboru. Tyto síťové/HW scénáře vyžadují box a provisioning server.
