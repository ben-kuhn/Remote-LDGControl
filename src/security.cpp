#include "security.h"
#include "config_manager.h"
#include <cstring>

// Static member initialization
RateLimiter* Security::s_rateLimiter = nullptr;
LoginTracker* Security::s_loginTracker = nullptr;
String Security::s_currentToken = "";
uint32_t Security::s_tokenExpiry = 0;

// ============================================================
// RateLimiter Implementation
// ============================================================

RateLimiter::RateLimiter(uint16_t maxRequests, uint32_t windowMs)
    : m_maxRequests(maxRequests), m_windowMs(windowMs) {
    memset(m_clients, 0, sizeof(m_clients));
}

RateLimiter::ClientRecord* RateLimiter::findClient(const IPAddress& ip) {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (m_clients[i].ip == ip && m_clients[i].windowStart > 0) {
            return &m_clients[i];
        }
    }
    return nullptr;
}

RateLimiter::ClientRecord* RateLimiter::getFreeSlot() {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (m_clients[i].windowStart == 0) {
            return &m_clients[i];
        }
    }
    // Evict oldest
    return &m_clients[0];
}

bool RateLimiter::allowRequest(const IPAddress& clientIP) {
    uint32_t now = millis();
    ClientRecord* client = findClient(clientIP);

    if (!client) {
        client = getFreeSlot();
        client->ip = clientIP;
        client->count = 0;
        client->windowStart = now;
    }

    // Reset window if expired
    if (now - client->windowStart > m_windowMs) {
        client->count = 0;
        client->windowStart = now;
    }

    if (client->count >= m_maxRequests) {
        return false;
    }

    client->count++;
    return true;
}

void RateLimiter::reset() {
    memset(m_clients, 0, sizeof(m_clients));
}

// ============================================================
// LoginTracker Implementation
// ============================================================

LoginTracker::LoginTracker(uint8_t maxAttempts, uint32_t lockoutMs)
    : m_maxAttempts(maxAttempts), m_lockoutMs(lockoutMs) {
    memset(m_records, 0, sizeof(m_records));
}

LoginTracker::LoginRecord* LoginTracker::findRecord(const IPAddress& ip) {
    for (uint8_t i = 0; i < MAX_RECORDS; i++) {
        if (m_records[i].ip == ip) {
            return &m_records[i];
        }
    }
    return nullptr;
}

LoginTracker::LoginRecord* LoginTracker::getFreeSlot() {
    for (uint8_t i = 0; i < MAX_RECORDS; i++) {
        if (m_records[i].lockoutUntil == 0 && m_records[i].failures == 0) {
            return &m_records[i];
        }
    }
    return &m_records[0];
}

bool LoginTracker::canAttempt(const IPAddress& clientIP) {
    uint32_t now = millis();
    LoginRecord* record = findRecord(clientIP);

    if (!record) {
        return true;
    }

    // Check if lockout has expired
    if (record->lockoutUntil > 0 && now < record->lockoutUntil) {
        return false;
    }

    // Lockout expired, reset
    if (record->lockoutUntil > 0 && now >= record->lockoutUntil) {
        record->failures = 0;
        record->lockoutUntil = 0;
    }

    return true;
}

void LoginTracker::recordSuccess(const IPAddress& clientIP) {
    LoginRecord* record = findRecord(clientIP);
    if (record) {
        record->failures = 0;
        record->lockoutUntil = 0;
    }
}

void LoginTracker::recordFailure(const IPAddress& clientIP) {
    uint32_t now = millis();
    LoginRecord* record = findRecord(clientIP);

    if (!record) {
        record = getFreeSlot();
        record->ip = clientIP;
        record->failures = 0;
        record->lockoutUntil = 0;
    }

    record->failures++;

    if (record->failures >= m_maxAttempts) {
        record->lockoutUntil = now + m_lockoutMs;
    }
}

void LoginTracker::reset() {
    memset(m_records, 0, sizeof(m_records));
}

// ============================================================
// Security Implementation
// ============================================================

void Security::begin() {
    s_rateLimiter = new RateLimiter(WEB_RATE_LIMIT_REQUESTS, WEB_RATE_LIMIT_WINDOW_MS);
    s_loginTracker = new LoginTracker(MAX_LOGIN_ATTEMPTS, LOGIN_LOCKOUT_MS);
    generateToken();
}

bool Security::verifyAuth(const String& username, const String& password) {
    const DeviceConfig& cfg = configManager.get();
    return (username == cfg.webUsername && password == cfg.webPassword);
}

String Security::generateToken() {
    s_currentToken = String(random(0xFFFFFF), HEX);
    s_currentToken += String(millis(), HEX);
    s_tokenExpiry = millis() + 3600000; // 1 hour
    return s_currentToken;
}

bool Security::validateToken(const String& token) {
    if (millis() > s_tokenExpiry) {
        generateToken();
        return false;
    }
    return (token == s_currentToken);
}

RateLimiter& Security::getRateLimiter() {
    return *s_rateLimiter;
}

LoginTracker& Security::getLoginTracker() {
    return *s_loginTracker;
}

bool Security::isAuthorized(const IPAddress& clientIP, const String& username,
                           const String& password, const String& csrfToken) {
    // Check rate limit
    if (!s_rateLimiter->allowRequest(clientIP)) {
        return false;
    }

    // Check login lockout
    if (!s_loginTracker->canAttempt(clientIP)) {
        return false;
    }

    // Verify credentials
    if (!verifyAuth(username, password)) {
        s_loginTracker->recordFailure(clientIP);
        return false;
    }

    // Validate CSRF token if provided
    if (!csrfToken.isEmpty() && !validateToken(csrfToken)) {
        return false;
    }

    s_loginTracker->recordSuccess(clientIP);
    return true;
}
