#pragma once
#include "config.h"

// Test both production and keep-credential behavior independently of private box_setup.h.
#undef QR_SCANNER_TEST_KEEP_CRED
#ifdef TEST_SCAN_KEEP
#define QR_SCANNER_TEST_KEEP_CRED
#undef QR_SCANNER_ACTIVATE_COMMAND
#define QR_SCANNER_ACTIVATE_COMMAND "GO\\r\\x00"
#undef QR_SCANNER_DEACTIVATE_COMMAND
#define QR_SCANNER_DEACTIVATE_COMMAND "STOP\\n"
#endif
