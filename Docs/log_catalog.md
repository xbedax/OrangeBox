# Katalog logovacích událostí firmware

Číselník hlavního firmware bez kamery a lokálních SSH knihoven. ID `Axxx` je označení pro člověka; po síti se posílá číslo `xxx`. Čísla zůstávají stabilní a nepřidělují se podle pořadí volání.

Zdrojem textů je [log_messages.def](../include/log_messages.def), číselné konstanty jsou v [logger.h](../include/logger.h). Export pro příjemce je [log_messages.json](log_messages.json); [CSV katalog](log_catalog.csv) umožňuje filtrování. [Mapování původních výpisů](log_migration.csv) zachovává vazbu na původní katalog.

Aktualizace: `python scripts/export_log_messages.py`. Kontrola bez zápisu: `python scripts/export_log_messages.py --check`.

`{p1}` odpovídá klíči `messageparams.p1`; pořadí atributů JSON nemá význam. Displej používá krátký text, pokud existuje. Serial přidává konec řádku. Limit počtu parametrů je `LOG_MAX_PARAMS` v `config.h` (12), limit celého bloku včetně typů a délek je `LOG_PARAM_DATA_MAX_LEN` (160 bajtů).

| Závažnost | Hodnota |
| --- | ---: |
| DEBUG | 1 |
| INFO | 2 |
| WARNING | 3 |
| ERROR | 4 |
| CRITICAL | 5 |

`LOGPRINT_MIN_SEVERITY` propouští tuto a vyšší závažnosti. Oblasti: COND 2, COMM 3, SYSTEM 4, ACCESS 5, OPERATION 6. Hodnota 1 je vyřazená.

Parametry PIN/kódu nejsou samotné přístupové hodnoty; zaznamenává se výsledek nebo ID credentialu. Diagnostický snapshot A019 obsahuje 11 hodnot: epochový čas, volnou interní paměť, největší interní blok, minimum volné interní paměti, stejné tři metriky PSRAM, minimum volného zásobníku sledované úlohy, velikost flash, velikost firmware a prostor pro OTA.

Počet hlášek: **150**.

| ID | Konstanta | Text | Krátký text | Parametrů | Volání a klasifikace |
| --- | --- | --- | --- | ---: | --- |
| A001 | `A_INVALID_ACTION_TYPE` | `Invalid actionType idx presented` | — | 0 | [src/box_display.cpp:190](../src/box_display.cpp#L190) `LOGAREA_SYSTEM`/`SEVERITY_ERROR`; [src/box_display.cpp:248](../src/box_display.cpp#L248) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A003 | `A_INVALID_INFO_TYPE` | `Invalid infoType idx presented` | — | 0 | [src/box_display.cpp:259](../src/box_display.cpp#L259) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A004 | `A_INVALID_RESPONSE_TYPE` | `Invalid responseType idx presented` | — | 0 | [src/box_display.cpp:270](../src/box_display.cpp#L270) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A005 | `A_DISPLAY_INITIALIZING` | `Initializing LCD display...` | — | 0 | [src/box_display.cpp:361](../src/box_display.cpp#L361) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A019 | `A_DIAGNOSTIC_SNAPSHOT` | `Diagnostics t={p1} ih=[{p2},{p3},{p4}] ps=[{p5},{p6},{p7}] stk={p8} fl=[{p9},{p10},{p11}]` | `Diagnostics snapshot` | 11 | [src/diagnostics.cpp:91](../src/diagnostics.cpp#L91) `logArea`/`SEVERITY_INFO` |
| A020 | `A_DOOR_STATE_READ` | `Reading state for logical door number: {p1}` | — | 1 | [src/gpio_hal.cpp:35](../src/gpio_hal.cpp#L35) `LOGAREA_OPERATION`/`SEVERITY_DEBUG` |
| A021 | `A_INVALID_LOCK_DOOR` | `Invalid door number in lockDeactivate: {p1}` | — | 1 | [src/gpio_hal.cpp:107](../src/gpio_hal.cpp#L107) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A022 | `A_GPIO_DOORS_LOADED` | `GPIO HAL loaded {p1} active doors:` | — | 1 | [src/gpio_hal.cpp:200](../src/gpio_hal.cpp#L200) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A023 | `A_GPIO_DOOR_MAPPING` | `Door {p1} (LogDoorNum: {p2}) - State Pin: {p3}, Last State: {p4}` | — | 4 | [src/gpio_hal.cpp:214](../src/gpio_hal.cpp#L214) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A032 | `A_INVALID_KEYBOARD_MODE` | `Invalid keyboard mode:{p1}` | — | 1 | [src/keyboard.cpp:99](../src/keyboard.cpp#L99) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A044 | `A_MISSING_DOOR_NUMBER` | `Missing door number in request` | — | 0 | [src/main.cpp:254](../src/main.cpp#L254) `LOGAREA_OPERATION`/`SEVERITY_WARNING`; [src/main.cpp:264](../src/main.cpp#L264) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A046 | `A_INVALID_DOOR_NUMBER_TEXT` | `Invalid door number: {p1}` | — | 1 | [src/main.cpp:271](../src/main.cpp#L271) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A047 | `A_INVALID_DOOR_FORMAT` | `Invalid door format` | — | 0 | [src/main.cpp:279](../src/main.cpp#L279) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A048 | `A_INVALID_DOOR_NUMBER` | `Invalid door number {p1}` | — | 1 | [src/main.cpp:286](../src/main.cpp#L286) `LOGAREA_OPERATION`/`SEVERITY_WARNING`; [src/main.cpp:302](../src/main.cpp#L302) `LOGAREA_OPERATION`/`SEVERITY_WARNING`; [src/main.cpp:363](../src/main.cpp#L363) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A049 | `A_DOOR_STATE_REQUEST` | `handleGetDoors called for door number: {p1}` | — | 1 | [src/main.cpp:292](../src/main.cpp#L292) `LOGAREA_OPERATION`/`SEVERITY_DEBUG` |
| A050 | `A_DOOR_MIXED_STATE` | `Mixed state detected for door {p1}` | — | 1 | [src/main.cpp:298](../src/main.cpp#L298) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A053 | `A_PRESENCE_CODE_REQUIRED` | `Presence code required but not provided` | — | 0 | [src/main.cpp:374](../src/main.cpp#L374) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A054 | `A_PRESENCE_CODE_INVALID` | `Invalid presence code` | — | 0 | [src/main.cpp:380](../src/main.cpp#L380) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A055 | `A_DOOR_OPEN_QUEUED` | `Door {p1} open request queued` | — | 1 | [src/main.cpp:392](../src/main.cpp#L392) `LOGAREA_OPERATION`/`SEVERITY_INFO` |
| A056 | `A_VPN_BEGIN_FAILED` | `VPN begin failed: {p1}` | — | 1 | [src/main.cpp:542](../src/main.cpp#L542) `LOGAREA_COMM`/`SEVERITY_ERROR` |
| A057 | `A_VPN_CONFIGURED` | `VPN configured: 127.0.0.1:{p1} -> {p2}:80` | — | 2 | [src/main.cpp:546](../src/main.cpp#L546) `LOGAREA_COMM`/`SEVERITY_INFO` |
| A058 | `A_VPN_SCHEDULED` | `VPN connection scheduled` | — | 0 | [src/main.cpp:548](../src/main.cpp#L548) `LOGAREA_COMM`/`SEVERITY_INFO` |
| A059 | `A_RESTART` | `Firmware restart` | `Restart` | 0 | [src/main.cpp:575](../src/main.cpp#L575) `LOGAREA_SYSTEM`/`SEVERITY_CRITICAL` |
| A060 | `A_RTC_OSF` | `RTC OSF flag: {p1}` | — | 1 | [src/main.cpp:582](../src/main.cpp#L582) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A062 | `A_SYSTEM_TIME_LOADED_FROM_RTC_WITH_OSF_SET` | `System time loaded from RTC with OSF set; waiting for NTP correction` | — | 0 | [src/main.cpp:586](../src/main.cpp#L586) `LOGAREA_SYSTEM`/`SEVERITY_WARNING` |
| A063 | `A_SYSTEM_TIME_LOADED_FROM_RTC` | `System time loaded from RTC` | — | 0 | [src/main.cpp:588](../src/main.cpp#L588) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A064 | `A_RTC_AVAILABLE_BUT_TIME_IS_NOT_TRUSTED_YET` | `RTC available, but time is not trusted yet` | — | 0 | [src/main.cpp:592](../src/main.cpp#L592) `LOGAREA_SYSTEM`/`SEVERITY_WARNING` |
| A065 | `A_RTC_NOT_FOUND_AT_X` | `RTC not found at 0x{p1}` | — | 1 | [src/main.cpp:595](../src/main.cpp#L595) `LOGAREA_SYSTEM`/`SEVERITY_WARNING` |
| A066 | `A_CURRENT_EPOCH` | `Box current time epoch: {p1}` | — | 1 | [src/main.cpp:599](../src/main.cpp#L599) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A068 | `A_CURRENT_TIME` | `Box current time: {p1}` | — | 1 | [src/main.cpp:606](../src/main.cpp#L606) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A070 | `A_DISPLAY_INITIALIZED` | `Display initialized` | — | 0 | [src/main.cpp:612](../src/main.cpp#L612) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A071 | `A_BOX_PROTOTYPE` | `BOX prototype` | — | 0 | [src/main.cpp:614](../src/main.cpp#L614) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A072 | `A_HAL_INITIALIZED` | `HAL initialized` | — | 0 | [src/main.cpp:620](../src/main.cpp#L620) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A073 | `A_KEYBOARD_INITIALIZED` | `Keyboard initialized` | — | 0 | [src/main.cpp:627](../src/main.cpp#L627) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A074 | `A_SCANNER_COMMAND_CONFIGURATION_IS_INVALID` | `Scanner command configuration is invalid` | — | 0 | [src/main.cpp:630](../src/main.cpp#L630) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A075 | `A_STATE_MACHINE_INITIALIZED` | `State machine initialized` | — | 0 | [src/main.cpp:635](../src/main.cpp#L635) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A078 | `A_STARTING_WIFI` | `Starting WiFi` | — | 0 | [src/main.cpp:653](../src/main.cpp#L653) `LOGAREA_COMM`/`SEVERITY_INFO` |
| A079 | `A_WIFI_CONNECTING` | `Waiting for WiFi` | — | 0 | [src/main.cpp:658](../src/main.cpp#L658) `LOGAREA_COMM`/`SEVERITY_DEBUG` |
| A080 | `A_WIFI_CONNECTED` | `WiFi connected!` | — | 0 | [src/main.cpp:663](../src/main.cpp#L663) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A081 | `A_WIFI_ADDRESS` | `WiFi address: {p1}` | — | 1 | [src/main.cpp:669](../src/main.cpp#L669) `LOGAREA_COMM`/`SEVERITY_DEBUG` |
| A083 | `A_WEB_URL` | `Open this URL: http://{p1}` | — | 1 | [src/main.cpp:671](../src/main.cpp#L671) `LOGAREA_COMM`/`SEVERITY_DEBUG` |
| A085 | `A_WIFI_FAILED` | `WiFi FAILED!` | — | 0 | [src/main.cpp:677](../src/main.cpp#L677) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A086 | `A_NTP_SYNCHRONIZATION_SCHEDULED` | `NTP synchronization scheduled` | — | 0 | [src/main.cpp:681](../src/main.cpp#L681) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A087 | `A_NTP_SYNCHRONIZATION_NOT_CONFIGURED` | `NTP synchronization not configured` | — | 0 | [src/main.cpp:683](../src/main.cpp#L683) `LOGAREA_SYSTEM`/`SEVERITY_WARNING` |
| A088 | `A_CREDENTIAL_STORAGE_INIT` | `Credential storage init` | — | 0 | [src/main.cpp:690](../src/main.cpp#L690) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A092 | `A_WEBSOCKET_INITIALIZED` | `WebSocket Initialized` | — | 0 | [src/main.cpp:718](../src/main.cpp#L718) `LOGAREA_COMM`/`SEVERITY_INFO` |
| A093 | `A_WEBSERVER_STARTED` | `Webserver started!` | — | 0 | [src/main.cpp:721](../src/main.cpp#L721) `LOGAREA_COMM`/`SEVERITY_INFO` |
| A094 | `A_SETUP_COMPLETE` | `SETUP COMPLETE!` | — | 0 | [src/main.cpp:730](../src/main.cpp#L730) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A095 | `A_DOOR_MIXED_NOTIFICATION` | `Mixed state detected for door {p1} during notification` | — | 1 | [src/main.cpp:768](../src/main.cpp#L768) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A096 | `A_INVALID_DOOR_NUMBER_IN_NOTIFICATION` | `Invalid door number {p1} in notification` | — | 1 | [src/main.cpp:771](../src/main.cpp#L771) `LOGAREA_OPERATION`/`SEVERITY_WARNING` |
| A097 | `A_LOCK_TIMER_PAYLOAD` | `Lock timer payload: {p1}` | — | 1 | [src/main.cpp:785](../src/main.cpp#L785) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A098 | `A_LOCK_DEACTIVATED` | `Lock {p1} deactivated` | — | 1 | [src/main.cpp:789](../src/main.cpp#L789) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A099 | `A_INVALID_LOCK_NUMBER_IN_TIMER_CALLBACK` | `Invalid lock number in timer callback: {p1}` | — | 1 | [src/main.cpp:791](../src/main.cpp#L791) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A100 | `A_DEACTIVATING_AMBIENT_LIGHT` | `Deactivating ambient light` | — | 0 | [src/main.cpp:797](../src/main.cpp#L797) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A101 | `A_PASSWORD_TIMEOUT_PASSED` | `Password timeout passed` | — | 0 | [src/main.cpp:802](../src/main.cpp#L802) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A102 | `A_UNKNOWN_DUE_ACTION_CALLED` | `Unknown due action called` | — | 0 | [src/main.cpp:811](../src/main.cpp#L811) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A103 | `A_WIFI_LOST` | `WiFi lost` | — | 0 | [src/main.cpp:821](../src/main.cpp#L821) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A104 | `A_RESTARTING_WIFI_ASSOCIATION` | `Restarting WiFi association` | — | 0 | [src/main.cpp:834](../src/main.cpp#L834) `LOGAREA_COMM`/`SEVERITY_INFO` |
| A105 | `A_WIFI_RESTORED` | `WiFi restored` | — | 0 | [src/main.cpp:844](../src/main.cpp#L844) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A106 | `A_CREDSTORE_UPDATECRED_INVALID_CREDENTIAL` | `[CredStore] updateCred: invalid credential` | — | 0 | [src/pass_store.cpp:95](../src/pass_store.cpp#L95) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A107 | `A_CREDSTORE_ADDCRED_INVALID_CREDENTIAL` | `[CredStore] addCred: invalid credential` | — | 0 | [src/pass_store.cpp:115](../src/pass_store.cpp#L115) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A108 | `A_CREDSTORE_ADDCRED_FAILED_TO_WRITE_NEW_CREDENTIAL` | `[CredStore] addCred: failed to write new credential` | — | 0 | [src/pass_store.cpp:130](../src/pass_store.cpp#L130) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A109 | `A_CREDENTIAL_VERIFY_STARTED` | `Credential verification at {p1}` | — | 1 | [src/pass_store.cpp:142](../src/pass_store.cpp#L142) `LOGAREA_OPERATION`/`SEVERITY_DEBUG` |
| A110 | `A_CREDENTIAL_VERIFY_CANDIDATE` | `Checking credential id={p1}, name={p2}, validFrom={p3}, validTo={p4}, remaining={p5}` | — | 5 | [src/pass_store.cpp:144](../src/pass_store.cpp#L144) `LOGAREA_OPERATION`/`SEVERITY_DEBUG` |
| A111 | `A_CREDSTORE_REMOVECRED_RECORD_WITH_CREDID_NOT_FOUND` | `[CredStore] removeCred: record with credId={p1} not found` | — | 1 | [src/pass_store.cpp:196](../src/pass_store.cpp#L196) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A112 | `A_CREDSTORE_UPDATE_CREDID_NOT_PROPERLY_CHAINED` | `[CredStore] update: credId={p1} not properly chained` | — | 1 | [src/pass_store.cpp:200](../src/pass_store.cpp#L200) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:389](../src/pass_store.cpp#L389) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A113 | `A_CREDSTORE_REMOVECRED_NAME_OR_CREDENTIAL_MISMATCH_FOR_CREDID` | `[CredStore] removeCred: name or credential mismatch for credId={p1}` | — | 1 | [src/pass_store.cpp:207](../src/pass_store.cpp#L207) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A114 | `A_CREDSTORE_REMOVECRED_FAILED_TO_READ_PREV_RECORD_WITH` | `[CredStore] removeCred: failed to read prev record with id={p1}` | — | 1 | [src/pass_store.cpp:214](../src/pass_store.cpp#L214) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A115 | `A_CREDSTORE_REMOVECRED_FAILED_TO_READ_NEXT_RECORD_WITH` | `[CredStore] removeCred: failed to read next record with id={p1}` | — | 1 | [src/pass_store.cpp:218](../src/pass_store.cpp#L218) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A116 | `A_CREDSTORE_UPDATE_RECORD_WITH_CREDID_NOT_FOUND` | `[CredStore] update: record with credId={p1} not found` | — | 1 | [src/pass_store.cpp:286](../src/pass_store.cpp#L286) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A117 | `A_CREDSTORE_UPDATE_MISMATCH` | `Credential name or value mismatch for id={p1}` | — | 1 | [src/pass_store.cpp:292](../src/pass_store.cpp#L292) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A118 | `A_CREDSTORE_UPDATE_WRITE_FAILED_FOR_CREDID` | `[CredStore] update: write failed for credId={p1}` | — | 1 | [src/pass_store.cpp:301](../src/pass_store.cpp#L301) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A119 | `A_CREDENTIAL_WRITE` | `Writing credential id={p1}, name={p2}, door={p3}, validFrom={p4}, validTo={p5}, remaining={p6}` | — | 6 | [src/pass_store.cpp:357](../src/pass_store.cpp#L357) `LOGAREA_ACCESS`/`SEVERITY_DEBUG` |
| A120 | `A_CREDSTORE_WRITECRED_FAILED_TO_READ_LAST_RECORD` | `[CredStore] writeCred: failed to read last record` | — | 0 | [src/pass_store.cpp:360](../src/pass_store.cpp#L360) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A121 | `A_CREDSTORE_WRITECRED_FAILED_TO_READ_PREV_RECORD_WITH` | `[CredStore] writeCred: failed to read prev record with id={p1}` | — | 1 | [src/pass_store.cpp:365](../src/pass_store.cpp#L365) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A122 | `A_CREDSTORE_UPDATE_FAILED_TO_WRITE_CREDID` | `[CredStore] update: failed to write credId={p1}` | — | 1 | [src/pass_store.cpp:380](../src/pass_store.cpp#L380) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:396](../src/pass_store.cpp#L396) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A123 | `A_CREDSTORE_UPDATE_PARTIAL_WRITE_FOR_CREDID_WRITTEN_OF` | `[CredStore] update: partial write for credId={p1}, written={p2}   (of {p3})` | — | 3 | [src/pass_store.cpp:383](../src/pass_store.cpp#L383) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:399](../src/pass_store.cpp#L399) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A127 | `A_CREDSTORE_REBUILDCACHE_HEAD_UNREADABLE` | `[CredStore] rebuildCache: HEAD unreadable` | — | 0 | [src/pass_store.cpp:440](../src/pass_store.cpp#L440) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A128 | `A_CREDSTORE_REBUILDCACHE_BROKEN_LINK_AT_ID` | `[CredStore] rebuildCache: broken link at id={p1}` | — | 1 | [src/pass_store.cpp:449](../src/pass_store.cpp#L449) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A129 | `A_CREDSTORE_CACHE_ACTIVE_RECORD_S_MAXCREDID` | `[CredStore] Cache: {p1} active record(s), maxCredId={p2}` | — | 2 | [src/pass_store.cpp:467](../src/pass_store.cpp#L467) `LOGAREA_ACCESS`/`SEVERITY_INFO` |
| A130 | `A_CREDSTORE_RESETTING_LEGACY_TEST_DATA_POOL` | `[CredStore] resetting legacy test-data pool` | — | 0 | [src/pass_store.cpp:480](../src/pass_store.cpp#L480) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A131 | `A_CREDSTORE_GC_EMPTY_CREDENTIAL_POOL_CREATING_SENTINELS` | `[CredStore] GC: empty credential pool, creating sentinels` | — | 0 | [src/pass_store.cpp:541](../src/pass_store.cpp#L541) `LOGAREA_ACCESS`/`SEVERITY_INFO` |
| A132 | `A_CREDSTORE_GC_CORRUPTED_SENTINELS_RESETTING_CREDENTIAL_POOL` | `[CredStore] GC: corrupted sentinels, resetting credential pool` | — | 0 | [src/pass_store.cpp:547](../src/pass_store.cpp#L547) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A133 | `A_CREDSTORE_GC_ROLLED_BACK_INTERRUPTED_INSERT_TAILPREV_NEXTID` | `[CredStore] GC: rolled back interrupted insert, tailPrev.nextId={p1}` | — | 1 | [src/pass_store.cpp:627](../src/pass_store.cpp#L627) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A134 | `A_CREDSTORE_GC_FOUND_INTERRUPTED_INSERT_TAILPREV_NEXTID` | `[CredStore] GC: found interrupted insert, tailPrev.nextId={p1}` | — | 1 | [src/pass_store.cpp:631](../src/pass_store.cpp#L631) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A135 | `A_CREDSTORE_GC_TANGLED_CHAIN_RESETTING_CREDENTIAL_POOL` | `[CredStore] GC: tangled chain, resetting credential pool` | — | 0 | [src/pass_store.cpp:645](../src/pass_store.cpp#L645) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A136 | `A_CREDSTORE_GC_CORRUPTED_DELETE_RECORD_CREDID_RESETTING_CREDENTIAL` | `[CredStore] GC: corrupted delete record credId={p1}, resetting credential pool` | — | 1 | [src/pass_store.cpp:655](../src/pass_store.cpp#L655) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A137 | `A_CREDSTORE_GC_REMOVED_INTERRUPTED_DELETE_RECORD_CREDID` | `[CredStore] GC: removed interrupted delete record credId={p1}` | — | 1 | [src/pass_store.cpp:662](../src/pass_store.cpp#L662) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:755](../src/pass_store.cpp#L755) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A138 | `A_CREDSTORE_GC_COMPLETED_INTERRUPTED_DELETE_AFTER_REMOVED_CREDID` | `[CredStore] GC: completed interrupted delete after removed credId={p1}` | — | 1 | [src/pass_store.cpp:664](../src/pass_store.cpp#L664) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A139 | `A_CREDSTORE_GC_TRAVERSING_CHAIN_CURRENT_CREDID_NEXTID` | `[CredStore] GC: traversing chain, current credId={p1}, nextId={p2}` | — | 2 | [src/pass_store.cpp:717](../src/pass_store.cpp#L717) `LOGAREA_ACCESS`/`SEVERITY_DEBUG` |
| A140 | `A_CREDSTORE_GC_ROLLED_BACK_BROKEN_INSERT` | `[CredStore] GC: rolled back broken insert` | — | 0 | [src/pass_store.cpp:725](../src/pass_store.cpp#L725) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A141 | `A_CREDSTORE_GC_INTERRUPTED_CHAIN_RESETTING_CREDENTIAL_POOL` | `[CredStore] GC: interrupted chain, resetting credential pool` | — | 0 | [src/pass_store.cpp:728](../src/pass_store.cpp#L728) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A142 | `A_CREDSTORE_GC_UNEXPECTED_ID_IN_CHAIN_RESETTING_CREDENTIAL` | `[CredStore] GC: unexpected ID in chain, resetting credential pool` | — | 0 | [src/pass_store.cpp:733](../src/pass_store.cpp#L733) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A143 | `A_CREDSTORE_GC_DUBIOUS_RECORD_IN_CHAIN_RESETTING_CREDENTIAL` | `[CredStore] GC: dubious record in chain, resetting credential pool` | — | 0 | [src/pass_store.cpp:737](../src/pass_store.cpp#L737) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A144 | `A_CREDSTORE_GC_REDOING_UNFINISHED_INSERT_CREDID` | `[CredStore] GC: redoing unfinished insert, credId {p1}` | — | 1 | [src/pass_store.cpp:743](../src/pass_store.cpp#L743) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A146 | `A_CREDSTORE_GC_COMPLETED_INTERRUPTED_DELETE` | `[CredStore] GC: completed interrupted delete` | — | 0 | [src/pass_store.cpp:760](../src/pass_store.cpp#L760) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A147 | `A_CREDSTORE_GC_BROKEN_CHAIN_RESETTING_CREDENTIAL_POOL` | `[CredStore] GC: broken chain, resetting credential pool` | — | 0 | [src/pass_store.cpp:763](../src/pass_store.cpp#L763) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A148 | `A_CREDSTORE_GC_UNABLE_TO_TRAVERSE_CHAIN_RESETTING_POOL` | `[CredStore] GC: unable to traverse chain, resetting pool, last deleted credId={p1}` | — | 1 | [src/pass_store.cpp:774](../src/pass_store.cpp#L774) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A149 | `A_CREDSTORE_GC_UNRECOVERABLE_CHAIN_STATE_RESETTING_CREDENTIAL_POOL` | `[CredStore] GC: unrecoverable chain state, resetting credential pool` | — | 0 | [src/pass_store.cpp:785](../src/pass_store.cpp#L785) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A152 | `A_CREDSTORE_GC_COMPLETED` | `[CredStore] GC: completed` | — | 0 | [src/pass_store.cpp:801](../src/pass_store.cpp#L801) `LOGAREA_ACCESS`/`SEVERITY_INFO` |
| A153 | `A_DOOR_OPEN_TIMEOUT` | `door {p1} not opened` | `Door {p1} timeout` | 1 | [src/state_machine.cpp:164](../src/state_machine.cpp#L164) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A154 | `A_KEYBOARD_EVENT` | `>>>>Keyboard event in state: {p1}, event: {p2}` | — | 2 | [src/state_machine.cpp:337](../src/state_machine.cpp#L337) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A155 | `A_SENSOR_EVENT` | `>>>>Sensor event in state: {p1}, event: {p2}, door: {p3}` | — | 3 | [src/state_machine.cpp:449](../src/state_machine.cpp#L449) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A156 | `A_DOOR_OPEN_FAILED` | `door {p1} cannot be opened` | `Door {p1} failed` | 1 | [src/state_machine.cpp:593](../src/state_machine.cpp#L593) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A157 | `A_CREDENTIAL_CONSUME_FAILED` | `Verified credential for door {p1} could not be consumed` | — | 1 | [src/state_machine.cpp:610](../src/state_machine.cpp#L610) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A158 | `A_REGISTERING_COMMAND_HANDLER_FOR_COMMAND` | `Registering command handler for command: {p1}` | — | 1 | [src/websocket.cpp:55](../src/websocket.cpp#L55) `LOGAREA_COMM`/`SEVERITY_DEBUG` |
| A159 | `A_FAILED_TO_PARSE_JSON` | `Failed to parse JSON` | — | 0 | [src/websocket.cpp:73](../src/websocket.cpp#L73) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A160 | `A_INVALID_MESSAGE_ENVELOPE` | `Invalid message envelope` | — | 0 | [src/websocket.cpp:78](../src/websocket.cpp#L78) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A161 | `A_RECEIVED_COMMAND` | `.. Received command: {p1}` | — | 1 | [src/websocket.cpp:81](../src/websocket.cpp#L81) `LOGAREA_COMM`/`SEVERITY_DEBUG` |
| A162 | `A_FOUND_HANDLER_FOR_COMMAND` | `.. Found handler for command: {p1}` | — | 1 | [src/websocket.cpp:87](../src/websocket.cpp#L87) `LOGAREA_COMM`/`SEVERITY_DEBUG` |
| A163 | `A_RECEIVED_MESSAGE_WITH_UNKNOWN_COMMAND` | `Received message with unknown command` | — | 0 | [src/websocket.cpp:95](../src/websocket.cpp#L95) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A164 | `A_CLIENT_CONNECTED` | `WebSocket client #{p1} connected from {p2}` | `Client {p1} online` | 2 | [src/websocket.cpp:102](../src/websocket.cpp#L102) `LOGAREA_COMM`/`SEVERITY_CRITICAL` |
| A165 | `A_CLIENT_DISCONNECTED` | `WebSocket client #{p1} disconnected` | `Client {p1} offline` | 1 | [src/websocket.cpp:106](../src/websocket.cpp#L106) `LOGAREA_COMM`/`SEVERITY_CRITICAL` |
| A167 | `A_INVALID_CLIENT_INDEX_FOR_WATCHDOG_REMOVAL` | `Invalid client index {p1} for watchdog removal` | — | 1 | [src/websocket.cpp:147](../src/websocket.cpp#L147) `LOGAREA_COMM`/`SEVERITY_ERROR` |
| A168 | `A_REMOVED_WATCHDOG_ENTRY_FOR_CLIENT` | `Removed watchdog entry for client #{p1}` | — | 1 | [src/websocket.cpp:152](../src/websocket.cpp#L152) `LOGAREA_COMM`/`SEVERITY_INFO` |
| A169 | `A_WATCHDOG_LIMIT` | `Watchdog entry limit reached, cannot track new client` | — | 0 | [src/websocket.cpp:175](../src/websocket.cpp#L175) `LOGAREA_COMM`/`SEVERITY_ERROR` |
| A170 | `A_CLIENT_TIMEOUT` | `Client #{p1} timed out, closing connection` | — | 1 | [src/websocket.cpp:184](../src/websocket.cpp#L184) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A171 | `A_DOOR_OPENED` | `Door {p1} opened` | `Door {p1} open` | 1 | [src/state_machine.cpp:447](../src/state_machine.cpp#L447) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL`; [src/state_machine.cpp:604](../src/state_machine.cpp#L604) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A172 | `A_DOOR_CLOSED` | `Door {p1} closed` | `Door {p1} closed` | 1 | [src/state_machine.cpp:443](../src/state_machine.cpp#L443) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A173 | `A_PASSWORD_STARTED` | `Password entry started` | `Enter password` | 0 | [src/state_machine.cpp:534](../src/state_machine.cpp#L534) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A174 | `A_PASSWORD_CANCELED` | `Password entry canceled` | `Password canceled` | 0 | [src/state_machine.cpp:391](../src/state_machine.cpp#L391) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A175 | `A_PASSWORD_ENTERED` | `Password entered` | — | 0 | [src/state_machine.cpp:363](../src/state_machine.cpp#L363) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A176 | `A_PASSWORD_VERIFIED` | `Password verified for door {p1}` | `PIN accepted` | 1 | [src/state_machine.cpp:367](../src/state_machine.cpp#L367) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A177 | `A_PASSWORD_BAD` | `Password rejected` | `Bad password` | 0 | [src/state_machine.cpp:374](../src/state_machine.cpp#L374) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A178 | `A_CODE_SCANNED` | `Code scanned` | — | 0 | [src/state_machine.cpp:411](../src/state_machine.cpp#L411) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A179 | `A_CODE_VERIFIED` | `Code verified for door {p1}` | `Code accepted` | 1 | [src/state_machine.cpp:424](../src/state_machine.cpp#L424) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A180 | `A_CODE_UNKNOWN` | `Scanned code rejected` | `Unknown code` | 0 | [src/state_machine.cpp:418](../src/state_machine.cpp#L418) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A181 | `A_PRESENCE_STARTED` | `Presence confirmation started` | `Presence started` | 0 | [src/state_machine.cpp:572](../src/state_machine.cpp#L572) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A182 | `A_PRESENCE_CANCELED` | `Presence confirmation canceled` | `Presence canceled` | 0 | [src/state_machine.cpp:389](../src/state_machine.cpp#L389) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A183 | `A_OPEN_COMMAND_RECEIVED` | `Open command received` | — | 0 | [src/main.cpp:354](../src/main.cpp#L354) `LOGAREA_OPERATION`/`SEVERITY_CRITICAL` |
| A184 | `A_PIN_EDIT` | `PIN {p1}: credential {p2}` | — | 2 | [src/cred_protocol.cpp:229](../src/cred_protocol.cpp#L229) `LOGAREA_ACCESS`/`SEVERITY_CRITICAL` |
| A185 | `A_PIN_EDIT_ERROR` | `PIN edit rejected: {p1}` | — | 1 | [src/cred_protocol.cpp:232](../src/cred_protocol.cpp#L232) `LOGAREA_ACCESS`/`SEVERITY_CRITICAL` |
| A186 | `A_CODE_EDIT` | `Code {p1}: credential {p2}` | — | 2 | [src/cred_protocol.cpp:229](../src/cred_protocol.cpp#L229) `LOGAREA_ACCESS`/`SEVERITY_CRITICAL` |
| A187 | `A_CODE_EDIT_ERROR` | `Code edit rejected: {p1}` | — | 1 | [src/cred_protocol.cpp:232](../src/cred_protocol.cpp#L232) `LOGAREA_ACCESS`/`SEVERITY_CRITICAL` |
| A188 | `A_VPN_CONNECTED` | `VPN connected` | — | 0 | [src/main.cpp:889](../src/main.cpp#L889) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A189 | `A_VPN_DISCONNECTED` | `VPN disconnected` | — | 0 | [src/main.cpp:889](../src/main.cpp#L889) `LOGAREA_COMM`/`SEVERITY_WARNING` |
| A190 | `A_LOG_DROPPED` | `Dropped {p1} log event(s) while remote logging was unavailable` | `Logs dropped: {p1}` | 1 | Interní událost loggeru |
| A191 | `A_LOG_REJECTED` | `Log event A{p1} rejected: {p2}` | `Invalid log event` | 2 | Interní událost loggeru |
| A192 | `A_DOOR_QUEUE_FAILED` | `Could not queue door {p1} open request` | — | 1 | [src/main.cpp:389](../src/main.cpp#L389) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A193 | `A_STATE_CHANGED` | `State changed from {p1} to {p2}` | — | 2 | [src/state_machine.cpp:293](../src/state_machine.cpp#L293) `LOGAREA_SYSTEM`/`SEVERITY_DEBUG` |
| A194 | `A_TIMER_EVENT` | `Timer event {p1}` | — | 1 | [src/state_machine.cpp:474](../src/state_machine.cpp#L474) `LOGAREA_SYSTEM`/`SEVERITY_INFO` |
| A195 | `A_PASSWORD_TIMEOUT` | `Password entry timed out` | — | 0 | [src/state_machine.cpp:126](../src/state_machine.cpp#L126) `LOGAREA_OPERATION`/`SEVERITY_INFO`; [src/state_machine.cpp:478](../src/state_machine.cpp#L478) `LOGAREA_OPERATION`/`SEVERITY_INFO` |
| A196 | `A_PRESENCE_TIMEOUT` | `Presence confirmation timed out` | — | 0 | [src/state_machine.cpp:151](../src/state_machine.cpp#L151) `LOGAREA_OPERATION`/`SEVERITY_INFO`; [src/state_machine.cpp:492](../src/state_machine.cpp#L492) `LOGAREA_OPERATION`/`SEVERITY_INFO` |
| A197 | `A_SCAN_STARTED` | `Code scan started` | — | 0 | [src/state_machine.cpp:554](../src/state_machine.cpp#L554) `LOGAREA_OPERATION`/`SEVERITY_INFO` |
| A198 | `A_SCAN_FAILED` | `Code scan failed or timed out` | — | 0 | [src/state_machine.cpp:134](../src/state_machine.cpp#L134) `LOGAREA_OPERATION`/`SEVERITY_INFO`; [src/state_machine.cpp:407](../src/state_machine.cpp#L407) `LOGAREA_OPERATION`/`SEVERITY_INFO`; [src/state_machine.cpp:485](../src/state_machine.cpp#L485) `LOGAREA_OPERATION`/`SEVERITY_INFO` |
| A199 | `A_STATE_READ` | `State read: {p1}` | — | 1 | [src/main.cpp:240](../src/main.cpp#L240) `LOGAREA_OPERATION`/`SEVERITY_DEBUG`; [src/main.cpp:309](../src/main.cpp#L309) `LOGAREA_OPERATION`/`SEVERITY_DEBUG`; [src/main.cpp:318](../src/main.cpp#L318) `LOGAREA_OPERATION`/`SEVERITY_DEBUG`; [src/main.cpp:341](../src/main.cpp#L341) `LOGAREA_OPERATION`/`SEVERITY_DEBUG` |
| A200 | `A_CREDENTIAL_INPUT_INVALID` | `Invalid credential type in request` | — | 0 | [src/cred_protocol.cpp:225](../src/cred_protocol.cpp#L225) `LOGAREA_ACCESS`/`SEVERITY_WARNING` |
| A201 | `A_SCANNER_BUFFER_OVERFLOW` | `Scanner receive buffer overflow` | — | 0 | [src/box_scanner.cpp:71](../src/box_scanner.cpp#L71) `LOGAREA_SYSTEM`/`SEVERITY_ERROR` |
| A202 | `A_NVS_OPERATION_FAILED` | `NVS {p1} failed for credential {p2}` | — | 2 | [src/pass_store.cpp:228](../src/pass_store.cpp#L228) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:324](../src/pass_store.cpp#L324) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:475](../src/pass_store.cpp#L475) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:509](../src/pass_store.cpp#L509) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:515](../src/pass_store.cpp#L515) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:661](../src/pass_store.cpp#L661) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:754](../src/pass_store.cpp#L754) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
| A203 | `A_STORED_CREDENTIAL_INVALID` | `Malformed stored credential {p1}` | — | 1 | [src/pass_store.cpp:319](../src/pass_store.cpp#L319) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:328](../src/pass_store.cpp#L328) `LOGAREA_ACCESS`/`SEVERITY_ERROR`; [src/pass_store.cpp:502](../src/pass_store.cpp#L502) `LOGAREA_ACCESS`/`SEVERITY_ERROR` |
