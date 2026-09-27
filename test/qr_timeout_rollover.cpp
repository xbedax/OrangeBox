#include <cassert>
#include <iostream>
#include "qr_scanner.h"
int main()
{
    Stream uart;
    QrScanner scanner;
    QrScannerConfig config;
    config.activationResponseWindowMs = 5;
    config.frameIdleMs = 10;
    scanner.begin(uart, config);
    testStubMillis = UINT32_MAX - 9;
    assert(scanner.startScan(10)); // Scan expires exactly at zero.
    assert(scanner.update().type == QrScannerEventType::Activated);
    assert(scanner.update(UINT32_MAX - 5).type == QrScannerEventType::None);
    assert(scanner.state() == QrScannerState::WaitingActivationResponse);
    scanner.update(UINT32_MAX - 4);
    assert(scanner.state() == QrScannerState::WaitingCode);
    assert(scanner.update(UINT32_MAX).type == QrScannerEventType::None);
    assert(scanner.update(0).type == QrScannerEventType::ScanTimeout);
    assert(!scanner.isActive());

    assert(scanner.sendCommand("test", 20));
    assert(scanner.update(9).type == QrScannerEventType::None);
    assert(scanner.update(10).type == QrScannerEventType::CommandResponseTimeout);
    assert(scanner.update(11).type == QrScannerEventType::None);

    config.activationResponseWindowMs = 0;
    config.defaultScanTimeoutMs = 0; // Zero duration intentionally disables the scan timeout.
    scanner.begin(uart, config);
    assert(scanner.startScan());
    scanner.update();
    assert(scanner.update(100).type == QrScannerEventType::None);
    assert(scanner.isActive());
    uart.input.push_back('X');
    assert(scanner.update(UINT32_MAX - 4).type == QrScannerEventType::None);
    assert(scanner.update(4).type == QrScannerEventType::None);
    assert(scanner.update(5).type == QrScannerEventType::CodeReceived);
    assert(!scanner.isActive());
    std::cout << "QR scanner rollover tests passed\n";
}
