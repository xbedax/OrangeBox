# Revize timeoutů

Patche porovnávají **aktuální pracovní soubory před touto úpravou**, nikoli HEAD Gitu. Změny již jsou v pracovních souborech; patche slouží ke kontrole, znovu je neaplikovat.
Původní soubory jsou v `backups/timeout-review-before/` (lokální záloha ignorovaná Gitem). Nic nebylo commitováno ani nahráno do zařízení.

- [ ] [01 – společná funkce a správce časovačů](01-helper-and-timers.patch)
- [ ] [02 – stavový automat, displej, presence kód a hlavní smyčka](02-state-and-main.patch)
- [ ] [03 – logger, WebSocket, VPN, QR a testovací firmware](03-network-and-qr.patch)
- [ ] [04 – regresní testy](04-tests.patch)

## Přehled kontroly

| Oblast | Výsledek |
| --- | --- |
| Společná funkce | `timeoutElapsed(start, length)` počítá unsigned 32bitové `millis() - start >= length`. Varianta se třetím parametrem používá předaný čas. `timeoutRemaining` vrací zbytek intervalu, po vypršení nulu. |
| TimerManager | Začátek a délka místo absolutního termínu. Odstraněno řazení přes `firstTimer` a nulový sentinel. Kontroluje maximálně 10 slotů. Opakování zachovává interval od zpracování bez dohánění zmeškaných period. Explicitní `update(0)` znamená skutečnou nulu, `update()` načte millis. |
| Stavový automat | Heslo, penalizace, presence, otevření dveří, ambient a obnovování displeje používají uplynulý interval. Odpočet vychází z rozdílu časů. Nezávislé intervaly mají příznaky aktivity; vypršelý presence kód ani penalizace se při dalším přetečení neoživí, pokud běží pravidelné update. |
| Presence API | `getPresenceCodeExpiration()` nahrazeno `isPresenceCodeValid()`, volající ve webovém otevření boxu upraven. Nepoužívané gettery absolutních časů penalizace a ambientu odstraněny. JSON protokol se nemění. |
| Hlavní smyčka | WiFi recovery, kontrola spojení a status displeje používají začátky intervalů. Po klávesnici se obnovuje čas, aby update nedostal čas starší než právě založený timeout. |
| Logger | Opakování připojení používá začátek intervalu a příznak prvního pokusu. Dosavadní rozpracované změny zachovány. |
| WebSocket | Plánování pingů převedeno na interval. `currentTime - lastPongTime > watchdogFeedInterval` již používá unsigned odečtení a zůstává beze změny, včetně původní délky a ostré nerovnosti. |
| WebSocket log transport | `currentMillis - lastActivityMillis` u handshake i nečinnosti je již unsigned a odolné vůči přetečení. Zkontrolována přiřazení v callbackách; kód ponechán. |
| VPN | Reconnect a zbývající čekání používají začátek a délku. Nulová délka znamená okamžitý pokus, nulový timestamp není sentinel. |
| QR | Scan, odpověď na aktivaci/příkaz a mezera mezi bajty používají unsigned intervaly. Nulová délka skenu stále znamená vypnutý timeout; termín vycházející na nulu už timeout nevypíná. |
| Testovací firmware | Opravena perioda výpisu VPN a čekání na Serial u QR. Konzolový test předává aktuální millis bez přímého porovnávání termínů. |
| Diagnostika | `timestampMillis` jde pouze do metriky uptime a JSON timestamp_ms, neslouží k plánování. Hodnota jako doposud přeteče po 49,7 dnech. |
| RTC a PINy | `lastRtcSyncMillis` se pouze ukládá, není čteno pro rozhodování. Kalendářní čas `time(nullptr)`/RTC a platnost PINů nejsou millis, jejich porovnání zůstávají. |
| Ostatní | `startMillis` a `switchTime` v main jsou nepoužité deklarace. Callback změny stavu předaný čas ignoruje. |

Rozsah: vlastní aktivní kód projektu **box_initial**, hlavičky, testy a navazující použití. Archivní `.old` soubory a externí knihovny v `.pio` se nemění; sousední projekt Box_wiegand není součástí této revize.

## Ověření a hranice

- **Prošlo:** `powershell -NoProfile -ExecutionPolicy Bypass -File test/run_timeout_rollover.ps1` – helper, jednorázové/opakované/zrušené časovače, stavový automat, procenta odpočtu, presence kontrola a QR. Začátky v nule i před přetečením, hranice před/při/po vypršení, vypršení přesně v nule.
- **Prošlo:** `pio run -e esp32-s3-n16r16` – sestavení finálního firmwaru.
- Testy běží na PC se simulovaným časem a periferiemi; fyzické zařízení nebylo testováno ani flashováno.
- Porovnání nově vyprší přesně při dosažení délky (`>=`), ne až následující milisekundu.
- 32bitové millis nerozliší celý oběh čítače navíc. Aktivní intervaly musí být kontrolovány tak, aby se vypršení zachytilo před celým dalším oběhem; nelze tím měřit libovolně dlouhé intervaly přes více přetečení. Předaný `now` musí být načten až po začátku intervalu.
