#pragma once
#include "config.h"

// Test both production and keep-credential behavior independently of private box_setup.h.
#undef QR_SCANNER_TEST_KEEP_CRED
#ifdef TEST_SCAN_KEEP
#define QR_SCANNER_TEST_KEEP_CRED
#endif
