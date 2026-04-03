#ifdef HAS_WIFI

#include "apiAuth.h"
#include "auth.h"
#include "logging.h"
#include <mbedtls/md.h>
#include <time.h>

// Convert hex string to byte array
static void hexToBytes(const char* hex, uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char h[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        bytes[i] = (uint8_t)strtoul(h, nullptr, 16);
    }
}

// Constant-time comparison of two hex strings (prevents timing attacks)
static bool constantTimeCompare(const char* a, const char* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        // Normalize to lowercase for case-insensitive compare
        uint8_t ca = (a[i] >= 'A' && a[i] <= 'F') ? (a[i] | 0x20) : a[i];
        uint8_t cb = (b[i] >= 'A' && b[i] <= 'F') ? (b[i] | 0x20) : b[i];
        diff |= ca ^ cb;
    }
    return diff == 0;
}

static bool verifyHmac(const String& authHeader, const String& method, const String& path) {
    // Parse "HMAC timestamp:signature"
    if (!authHeader.startsWith("HMAC ")) return false;
    String payload = authHeader.substring(5);
    int colonPos = payload.indexOf(':');
    if (colonPos < 0) return false;

    String timestamp = payload.substring(0, colonPos);
    String clientSig = payload.substring(colonPos + 1);

    if (clientSig.length() != 64) return false;

    // Check timestamp window (+-60 seconds)
    long ts = timestamp.toInt();
    long now = (long)time(nullptr);
    long delta = now - ts;
    if (delta < 0) delta = -delta;
    if (now > 1000000000L && delta > 60) return false;

    // Compute expected signature: HMAC-SHA256(webPasswordHash bytes, "timestamp:METHOD:path")
    String message = timestamp + ":" + method + ":" + path;

    if (webPasswordHash.length() < 64) return false;
    uint8_t key[32];
    hexToBytes(webPasswordHash.c_str(), key, 32);

    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, key, 32);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)message.c_str(), message.length());
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // Convert to hex
    char expected[65];
    for (int i = 0; i < 32; i++) sprintf(expected + 2 * i, "%02x", hmacResult[i]);
    expected[64] = '\0';

    return constantTimeCompare(expected, clientSig.c_str(), 64);
}

static bool verifyBearer(const String& authHeader) {
    if (!authHeader.startsWith("Bearer ")) return false;
    String token = authHeader.substring(7);
    if (token.length() != webPasswordHash.length()) return false;
    // Constant-time compare for bearer token too
    return constantTimeCompare(token.c_str(), webPasswordHash.c_str(), token.length());
}

bool checkApiAuth(AsyncWebServerRequest *request) {
    // No password set -> open access
    if (webPasswordHash.isEmpty()) return true;

    // Allow requests from IPs that have an authenticated WebSocket session
    // (the WebUI fetches API data after authenticating via WebSocket)
    uint32_t reqIP = (uint32_t)request->client()->remoteIP();
    for (int i = 0; i < MAX_AUTH_SESSIONS; i++) {
        if (authSessions[i].clientId != 0 && authSessions[i].authenticated
            && authSessions[i].ipAddr == reqIP) {
            return true;
        }
    }

    if (!request->hasHeader("Authorization")) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return false;
    }

    String auth = request->header("Authorization");

    bool ok = false;
    if (auth.startsWith("HMAC ")) {
        String method = request->methodToString();
        String path = request->url();
        ok = verifyHmac(auth, method, path);
    } else if (auth.startsWith("Bearer ")) {
        ok = verifyBearer(auth);
    }

    if (!ok) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    }
    return ok;
}

#endif
