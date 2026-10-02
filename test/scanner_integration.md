# Integrace čtečky

V hlavním firmware klávesa `3` ve stavu Home spustí Scan, zobrazí
`ACTIONLINE_TXT_SCAN` a odpočet na response line. Délku pokusu určuje
`QR_SCANNER_SCAN_TIMEOUT_MS` (aktuálně 5000 ms). Hvězdička pokus zruší.
Neplatné číslo zakázky nebo timeout vrací Home; platné číslo spustí Opening.
Ověření používá `CredType::PacketNumber` a existující porovnání prefixu
`PACKET_NUMBER_MATCH`. Heslem načteným čtečkou se dveře neotevřou.

`BoxScanner` převádí hexadecimální zápis aktivačního a deaktivačního příkazu
z config.h na bajty UART. Po odchodu ze Scan se čtečka deaktivuje a další
data se nepoužijí k otevření dveří. Aktuálně je deaktivační příkaz prázdný
a spínací pin nepoužitý: fyzické ukončení svícení/skenování tedy závisí na
vlastním nastavení čtečky. Pro řízené vypnutí lze doplnit příkaz nebo pin.

Kód se standardně spotřebuje až při potvrzení otevření příslušných dveří
kontaktem, stejně jako heslo. V lokálním box_setup.h je pro sandbox zapnutý
`QR_SCANNER_TEST_KEEP_CRED`: ověření proběhne, ale `usedCred()` se pro
načtený kód přeskočí. Pro běžný provoz define zakomentujte nebo odstraňte.
Spotřebování hesel tímto přepínačem není ovlivněno.

## Automatické ověření

Spusťte `powershell -NoProfile -ExecutionPolicy Bypass -File test/run_scanner_integration.ps1`.
Test se sestavuje s přepínačem i bez něj a používá skutečný stavový automat,
klávesnici, parser čtečky a úložiště s náhradami hardwaru. Pokrývá aktivaci
binárním UART příkazem, odpočet, přetečení millis, zrušení, neplatné a pozdní
kódy, rychlé načtení během okna odpovědi, rámce ukončené nečinností,
spotřebování po otevření a zachování kódu při neotevření dveří.

Na zařízení zbývá ověřit UART zapojení, odpověď konkrétní čtečky a celý
průchod od klávesy 3 přes načtení štítku po kontakt otevřených dveří.
