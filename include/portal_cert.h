#ifndef PORTAL_CERT_H
#define PORTAL_CERT_H

#include <Arduino.h>

// Generate a unique self-signed certificate and RSA key on first boot.
// Stores them in NVS so they persist across reboots.
// This prevents the security issue of a hardcoded private key in firmware.

bool portalCert_init();
const char* portalCert_getCert();
const char* portalCert_getKey();

#endif
