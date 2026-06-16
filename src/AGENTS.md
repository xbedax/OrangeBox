# AGENTS.md

## Projekt
Firmware a testovací nástroje pro domácí doručovací box.
Cílová platforma: ESP32 / Arduino framework.
Komunikace s UI: WebSocket, do budoucna MQTT přes WiFi nebo Cat-M/LTE.
Apache2: internet proxy a prezentace UI html5/javascript
Keycloak: IAM, integrace s externími AM providery (v plánu)

## Architektura
- zprávy  JSON UI <-> esp32 ve formátu:
  {
    "timestamp": "...",
    "_command_": "...",
    "scope": "...",
    "data": { ... }
  }
- Příchozí zpráva se nejdřív deserializuje přes ArduinoJson.
- Obecná knihovna kontroluje strukturu, timestamp a _command_.
- Vlastní obsluha příkazů se volá přes registrované callbacky.
- Watchdog/logiku běhu řešit v hlavním loopu, ne v parseru zpráv.

## C++ pravidla
- Preferuj malé třídy s jasnou odpovědností.
- Nepoužívej dynamickou alokaci, pokud není nutná.
- Na ESP32 šetři RAM.
- Nepřidávej těžké knihovny bez výslovného důvodu.
- Kód piš kompatibilně s Arduino frameworkem.

## JSON pravidla
- Povinné položky: timestamp, _command_.
- Volitelná položka: scope.
- Uživatelská data patří do data.
- Chyby vracej jako strukturovaný stav, ne jen jako text.

## Testování
- Pro logiku nezávislou na HW piš malé testovací stuby spustitelné mimo ESP32.
- Při změně parseru ukaž příklady validní a nevalidní zprávy.
- Neměň protokol bez upozornění.

## Co nedělat
- Neměnit význam existujících _command_ hodnot bez migrace.
- Nemíchat obsluhu konkrétního příkazu do generického WebSocket handleru.
- Nevkládat tajné klíče, hesla ani tokeny do repozitáře.
- Nikdy nenahrazovat starší verzi kódu novou bez vytvoření zálohy