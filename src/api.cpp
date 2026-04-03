#ifdef HAS_WIFI

#include "api.h"
#include "apiAuth.h"
#include "auth.h"
#include "config.h"
#include "settings.h"
#include "peer.h"
#include "routing.h"
#include "main.h"
#include "helperFunctions.h"
#include "hal.h"
#include "logging.h"

#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include <Preferences.h>

#include "wifiFunctions.h"
#include "serial.h"

// ── NTP sync tracking ──────────────────────────────────────────────────────
static uint32_t lastNtpSyncTime = 0;

static void onNtpSync(struct timeval *tv) {
    lastNtpSyncTime = (uint32_t)time(nullptr);
}

// ── Reset counter (NVS) ────────────────────────────────────────────────────
static uint32_t nvsResetCount = 0;

// ── LoRa frame counters ─────────────────────────────────────────────────────
uint32_t apiTxTotal = 0;
uint32_t apiRxTotal = 0;

// ── Message ring buffer ─────────────────────────────────────────────────────
static ApiMessage msgBuffer[API_MSG_BUFFER_SIZE];
static uint8_t msgHead = 0;
static uint8_t msgCount = 0;

// ── Event ring buffer ───────────────────────────────────────────────────────
static ApiEvent evtBuffer[API_EVT_BUFFER_SIZE];
static uint8_t evtHead = 0;
static uint8_t evtCount = 0;

// ── JSON escape helper (String-based) ───────────────────────────────────────
static void appendJsonStr(String &out, const char *str) {
    out += '"';
    for (const char *p = str; *p; p++) {
        char c = *p;
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((uint8_t)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
            out += buf;
        }
        else out += c;
    }
    out += '"';
}

// ── Helper: send pre-built JSON String with correct Content-Length ──────────
static void sendJsonResponse(AsyncWebServerRequest *request, const String &json) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

// ── Ring buffer record functions ────────────────────────────────────────────

void apiRecordMessage(const Frame &f, bool isTx) {
    ApiMessage &m = msgBuffer[msgHead];
    memset(&m, 0, sizeof(m));
    m.id = f.id;
    m.time = (uint32_t)f.timestamp;
    strlcpy(m.src, f.srcCall, sizeof(m.src));
    strlcpy(m.group, f.dstGroup, sizeof(m.group));
    if (f.messageLength > 0 && (f.messageType == Frame::MessageTypes::TEXT_MESSAGE ||
                                 f.messageType == Frame::MessageTypes::TRACE_MESSAGE)) {
        size_t len = f.messageLength < sizeof(m.text) - 1 ? f.messageLength : sizeof(m.text) - 1;
        memcpy(m.text, f.message, len);
        m.text[len] = '\0';
    }
    if (!isTx) {
        strlcpy(m.via, f.nodeCall, sizeof(m.via));
    }
    m.hops = f.hopCount;
    m.rssi = isTx ? 0 : (int16_t)f.rssi;
    m.snr = isTx ? 0 : (int8_t)f.snr;
    m.dir = isTx ? 1 : 0;
    m.acked = false;

    msgHead = (msgHead + 1) % API_MSG_BUFFER_SIZE;
    if (msgCount < API_MSG_BUFFER_SIZE) msgCount++;
}

static void pushEvent(const ApiEvent &e) {
    evtBuffer[evtHead] = e;
    evtHead = (evtHead + 1) % API_EVT_BUFFER_SIZE;
    if (evtCount < API_EVT_BUFFER_SIZE) evtCount++;
}

void apiRecordRxEvent(const Frame &f) {
    apiRxTotal++;
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)f.timestamp;
    strlcpy(e.event, "rx", sizeof(e.event));
    e.frameType = f.frameType;
    strlcpy(e.nodeCall, f.nodeCall, sizeof(e.nodeCall));
    strlcpy(e.viaCall, f.viaCall, sizeof(e.viaCall));
    strlcpy(e.srcCall, f.srcCall, sizeof(e.srcCall));
    e.id = f.id;
    e.rssi = (int16_t)f.rssi;
    e.snr = (int8_t)f.snr;
    e.port = f.port;
    pushEvent(e);
}

void apiRecordTxEvent(const Frame &f) {
    apiTxTotal++;
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)time(nullptr);
    strlcpy(e.event, "tx", sizeof(e.event));
    e.frameType = f.frameType;
    strlcpy(e.nodeCall, f.nodeCall, sizeof(e.nodeCall));
    strlcpy(e.viaCall, f.viaCall, sizeof(e.viaCall));
    strlcpy(e.srcCall, f.srcCall, sizeof(e.srcCall));
    e.id = f.id;
    e.port = f.port;
    pushEvent(e);
}

void apiRecordAckEvent(const Frame &f) {
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)f.timestamp;
    strlcpy(e.event, "ack", sizeof(e.event));
    strlcpy(e.srcCall, f.srcCall, sizeof(e.srcCall));
    strlcpy(e.nodeCall, f.nodeCall, sizeof(e.nodeCall));
    e.id = f.id;
    pushEvent(e);
}

void apiRecordRoutingEvent(const char* action, const char* dest, const char* via, uint8_t hops) {
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)time(nullptr);
    strlcpy(e.event, "routing", sizeof(e.event));
    strlcpy(e.action, action, sizeof(e.action));
    strlcpy(e.dest, dest, sizeof(e.dest));
    strlcpy(e.viaCall, via, sizeof(e.viaCall));
    e.hops = hops;
    pushEvent(e);
}

void apiRecordErrorEvent(const char* source, const char* text) {
    ApiEvent e;
    memset(&e, 0, sizeof(e));
    e.time = (uint32_t)time(nullptr);
    strlcpy(e.event, "error", sizeof(e.event));
    strlcpy(e.source, source, sizeof(e.source));
    strlcpy(e.text, text, sizeof(e.text));
    pushEvent(e);
}

void apiMarkMessageAcked(const char* srcCall, uint32_t id) {
    // Scan the ring buffer for matching message
    for (uint8_t i = 0; i < msgCount; i++) {
        uint8_t idx = (msgHead - msgCount + i + API_MSG_BUFFER_SIZE) % API_MSG_BUFFER_SIZE;
        if (msgBuffer[idx].id == id && strcmp(msgBuffer[idx].src, srcCall) == 0) {
            msgBuffer[idx].acked = true;
            return;
        }
    }
}

// ── ACK-based purge functions ──────────────────────────────────────────────

static void purgeMessages(uint32_t ackTime) {
    int purged = 0;
    // Remove oldest entries where time <= ackTime
    while (msgCount > 0) {
        uint8_t tailIdx = (msgHead - msgCount + API_MSG_BUFFER_SIZE) % API_MSG_BUFFER_SIZE;
        if (msgBuffer[tailIdx].time > ackTime) break;
        msgCount--;
        purged++;
    }
    if (purged > 0) {
        logPrintf(LOG_DEBUG, "API", "Purged %d acked messages, heap: %u", purged, ESP.getFreeHeap());
    }
}

static void purgeEvents(uint32_t ackTime) {
    int purged = 0;
    // Remove oldest entries where time <= ackTime
    while (evtCount > 0) {
        uint8_t tailIdx = (evtHead - evtCount + API_EVT_BUFFER_SIZE) % API_EVT_BUFFER_SIZE;
        if (evtBuffer[tailIdx].time > ackTime) break;
        evtCount--;
        purged++;
    }
    if (purged > 0) {
        logPrintf(LOG_DEBUG, "API", "Purged %d acked events, heap: %u", purged, ESP.getFreeHeap());
    }
}

// ── Helper: get board name string ───────────────────────────────────────────
static const char* getBoardName() {
    return PIO_ENV_NAME;
}

// ── Helper: get reset reason (ESP32 only) ───────────────────────────────────
#ifndef NRF52_PLATFORM
static const char* apiGetResetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_SW:        return "software";
        case ESP_RST_PANIC:     return "panic/crash";
        case ESP_RST_INT_WDT:   return "interrupt-watchdog";
        case ESP_RST_TASK_WDT:  return "task-watchdog";
        case ESP_RST_WDT:       return "other-watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT:  return "brownout";
        default:                return "unknown";
    }
}
#endif

// ── Heap guard ──────────────────────────────────────────────────────────────
static bool heapGuard(AsyncWebServerRequest *request) {
    if (ESP.getFreeHeap() < 15000) {
        request->send(503, "application/json", "{\"error\":\"heap too low\"}");
        return false;
    }
    return true;
}

// ── Endpoint handlers ───────────────────────────────────────────────────────

static void handleStatus(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    String json;
    json.reserve(512);

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"call\":\"%s\",\"version\":\"%s\",\"board\":\"%s\"",
             settings.mycall, VERSION, getBoardName());
    json += buf;
    snprintf(buf, sizeof(buf), ",\"uptime\":%lu,\"heap\":%u",
             millis() / 1000, ESP.getFreeHeap());
    json += buf;

#ifndef NRF52_PLATFORM
    snprintf(buf, sizeof(buf), ",\"resetReason\":\"%s\"", apiGetResetReason());
    json += buf;
#endif

    // WiFi info
    snprintf(buf, sizeof(buf), ",\"wifi\":{\"connected\":%s", WiFi.isConnected() ? "true" : "false");
    json += buf;
    if (WiFi.isConnected()) {
        snprintf(buf, sizeof(buf), ",\"rssi\":%d,\"ip\":\"%s\"", WiFi.RSSI(), WiFi.localIP().toString().c_str());
        json += buf;
    }
    json += "}";

    // LoRa info
    snprintf(buf, sizeof(buf), ",\"lora\":{\"txQueue\":%u,\"txTotal\":%lu,\"rxTotal\":%lu}",
             (unsigned)txBuffer.size(), (unsigned long)apiTxTotal, (unsigned long)apiRxTotal);
    json += buf;

    // Peers and routes count
    snprintf(buf, sizeof(buf), ",\"peers\":%u,\"routes\":%u", (unsigned)peerList.size(), (unsigned)routingList.size());
    json += buf;

    // Current time
    snprintf(buf, sizeof(buf), ",\"time\":%ld}", (long)time(nullptr));
    json += buf;

    sendJsonResponse(request, json);
}

static void handlePeers(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    String json;
    json.reserve(2048);
    json = "{\"peers\":[";

    char buf[256];
    for (size_t i = 0; i < peerList.size(); i++) {
        if (i > 0) json += ",";
        const Peer &p = peerList[i];

        // Check if same callsign exists on a different port (dual-path node)
        bool dualPath = false;
        for (size_t j = 0; j < peerList.size(); j++) {
            if (i != j && strcmp(peerList[i].nodeCall, peerList[j].nodeCall) == 0
                       && peerList[i].port != peerList[j].port) {
                dualPath = true;
                break;
            }
        }

        json += "{\"call\":";
        appendJsonStr(json, p.nodeCall);
        snprintf(buf, sizeof(buf),
            ",\"port\":%u,\"timestamp\":%ld,\"rssi\":%.1f,\"snr\":%.1f,"
            "\"frqError\":%.1f,\"available\":%s",
            p.port, (long)p.timestamp, p.rssi, p.snr, p.frqError,
            p.available ? "true" : "false");
        json += buf;
        if (dualPath) {
            snprintf(buf, sizeof(buf), ",\"preferred\":%s", p.available ? "true" : "false");
            json += buf;
        }
        json += "}";

        if (ESP.getFreeHeap() < 20000) break;
    }

    json += "]}";
    sendJsonResponse(request, json);
}

// Chunked streaming state for /api/routes (avoids building full JSON in heap)
struct RouteChunkState {
    size_t routeIdx;
    bool headerSent;
    bool footerSent;
};

static void handleRoutes(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    RouteChunkState *state = new RouteChunkState{0, false, false};
    if (!state) {
        request->send(503, "application/json", "{\"error\":\"heap too low\"}");
        return;
    }

    AsyncWebServerResponse *response = request->beginChunkedResponse(
        "application/json",
        [state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            if (!state->headerSent) {
                size_t w = snprintf((char*)buffer, maxLen, "{\"routes\":[");
                state->headerSent = true;
                return w;
            }

            if (state->routeIdx < routingList.size()) {
                const Route &rt = routingList[state->routeIdx];
                char entry[160];
                // Manually escape callsigns (they're alphanumeric, no escaping needed)
                int len = snprintf(entry, sizeof(entry),
                    "%s{\"srcCall\":\"%s\",\"viaCall\":\"%s\",\"timestamp\":%ld,\"hopCount\":%u}",
                    state->routeIdx > 0 ? "," : "",
                    rt.srcCall, rt.viaCall, (long)rt.timestamp, rt.hopCount);
                if (len > 0 && (size_t)len < maxLen) {
                    memcpy(buffer, entry, len);
                    state->routeIdx++;
                    return len;
                }
                return 0;  // buffer too small, try again
            }

            if (!state->footerSent) {
                size_t w = snprintf((char*)buffer, maxLen, "]}");
                state->footerSent = true;
                return w;
            }

            delete state;
            return 0;  // done
        }
    );
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

static void handleGetMessages(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    // Parse query parameters
    const char* groupFilter = nullptr;
    uint32_t since = 0;
    int limit = 20;
    uint32_t ack = 0;

    if (request->hasParam("group")) groupFilter = request->getParam("group")->value().c_str();
    if (request->hasParam("since")) since = request->getParam("since")->value().toInt();
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        if (limit < 1) limit = 1;
        if (limit > API_MSG_BUFFER_SIZE) limit = API_MSG_BUFFER_SIZE;
    }
    if (request->hasParam("ack")) {
        ack = request->getParam("ack")->value().toInt();
    }

    // FIRST: purge acknowledged messages to free heap before JSON serialization
    if (ack > 0) {
        purgeMessages(ack);
    }

    String json;
    json.reserve(4096);
    json = "{\"messages\":[";

    char buf[128];
    int count = 0;
    // Iterate from newest to oldest
    for (int i = msgCount - 1; i >= 0 && count < limit; i--) {
        uint8_t idx = (msgHead - msgCount + i + API_MSG_BUFFER_SIZE) % API_MSG_BUFFER_SIZE;
        const ApiMessage &m = msgBuffer[idx];

        if (m.time <= since) continue;
        if (groupFilter && strcmp(m.group, groupFilter) != 0) continue;

        if (count > 0) json += ",";
        snprintf(buf, sizeof(buf), "{\"id\":%lu,\"time\":%lu,\"src\":", (unsigned long)m.id, (unsigned long)m.time);
        json += buf;
        appendJsonStr(json, m.src);
        json += ",\"group\":";
        appendJsonStr(json, m.group);
        json += ",\"text\":";
        appendJsonStr(json, m.text);
        if (m.via[0] != '\0') {
            json += ",\"via\":";
            appendJsonStr(json, m.via);
        } else {
            json += ",\"via\":null";
        }
        snprintf(buf, sizeof(buf), ",\"hops\":%u,\"rssi\":%d,\"snr\":%d,\"dir\":\"%s\",\"ack\":%s}",
                 m.hops, m.rssi, (int)m.snr,
                 m.dir == 1 ? "tx" : "rx",
                 m.acked ? "true" : "false");
        json += buf;
        count++;

        if (ESP.getFreeHeap() < 20000) break;
    }

    json += "]}";
    sendJsonResponse(request, json);
}

static void handlePostMessage(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkApiAuth(request)) return;

    // ArduinoJson to parse the body
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }

    const char* group = doc["group"] | "";
    const char* text = doc["text"] | "";

    if (strlen(group) == 0 || strlen(text) == 0) {
        request->send(400, "application/json", "{\"error\":\"group and text required\"}");
        return;
    }

    // Build and send the frame
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = Frame::MessageTypes::TEXT_MESSAGE;
    strlcpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    safeUtf8Copy((char*)f.dstGroup, (const uint8_t*)group, MAX_CALLSIGN_LENGTH, sizeof(f.dstGroup));
    safeUtf8Copy((char*)f.message, (const uint8_t*)text, sizeof(f.message), sizeof(f.message));
    f.messageLength = strlen((char*)f.message);
    f.tx = true;

    // Record in API ring buffer before sending
    apiRecordMessage(f, true);

    sendFrame(f);

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%lu}", (unsigned long)f.id);
    request->send(200, "application/json", resp);
}

static void handleGroups(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    String json;
    json.reserve(512);
    json = "{\"groups\":[";

    char buf[32];
    int count = 0;
    for (int i = 1; i <= MAX_CHANNELS; i++) {
        if (groupNames[i][0] == '\0') continue;
        if (count > 0) json += ",";
        json += "{\"name\":";
        appendJsonStr(json, groupNames[i]);
        snprintf(buf, sizeof(buf), ",\"id\":%d,\"mode\":\"rw\"}", i);
        json += buf;
        count++;
    }

    json += "]}";
    sendJsonResponse(request, json);
}

static void handleEvents(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    // Parse query parameters
    const char* typeFilter = nullptr;
    uint32_t since = 0;
    int limit = 50;
    uint32_t ack = 0;

    if (request->hasParam("type")) typeFilter = request->getParam("type")->value().c_str();
    if (request->hasParam("since")) since = request->getParam("since")->value().toInt();
    if (request->hasParam("limit")) {
        limit = request->getParam("limit")->value().toInt();
        if (limit < 1) limit = 1;
        if (limit > API_EVT_BUFFER_SIZE) limit = API_EVT_BUFFER_SIZE;
    }
    if (request->hasParam("ack")) {
        ack = request->getParam("ack")->value().toInt();
    }

    // FIRST: purge acknowledged events to free heap before JSON serialization
    if (ack > 0) {
        purgeEvents(ack);
    }

    String json;
    json.reserve(4096);
    json = "{\"events\":[";

    char buf[128];
    int count = 0;
    // Iterate from newest to oldest
    for (int i = evtCount - 1; i >= 0 && count < limit; i--) {
        uint8_t idx = (evtHead - evtCount + i + API_EVT_BUFFER_SIZE) % API_EVT_BUFFER_SIZE;
        const ApiEvent &e = evtBuffer[idx];

        if (e.time <= since) continue;
        if (typeFilter && strcmp(e.event, typeFilter) != 0) continue;

        if (count > 0) json += ",";
        snprintf(buf, sizeof(buf), "{\"time\":%lu,\"event\":\"%s\"", (unsigned long)e.time, e.event);
        json += buf;

        if (strcmp(e.event, "rx") == 0 || strcmp(e.event, "tx") == 0) {
            snprintf(buf, sizeof(buf), ",\"frameType\":%u", e.frameType);
            json += buf;
            if (e.nodeCall[0]) { json += ",\"nodeCall\":"; appendJsonStr(json, e.nodeCall); }
            if (e.viaCall[0])  { json += ",\"viaCall\":";  appendJsonStr(json, e.viaCall); }
            if (e.srcCall[0])  { json += ",\"srcCall\":";  appendJsonStr(json, e.srcCall); }
            snprintf(buf, sizeof(buf), ",\"id\":%lu", (unsigned long)e.id);
            json += buf;
            if (strcmp(e.event, "rx") == 0) {
                snprintf(buf, sizeof(buf), ",\"rssi\":%d,\"snr\":%d", e.rssi, (int)e.snr);
                json += buf;
            }
            snprintf(buf, sizeof(buf), ",\"port\":%u", e.port);
            json += buf;
        } else if (strcmp(e.event, "ack") == 0) {
            if (e.srcCall[0])  { json += ",\"srcCall\":"; appendJsonStr(json, e.srcCall); }
            if (e.nodeCall[0]) { json += ",\"nodeCall\":"; appendJsonStr(json, e.nodeCall); }
            snprintf(buf, sizeof(buf), ",\"id\":%lu", (unsigned long)e.id);
            json += buf;
        } else if (strcmp(e.event, "routing") == 0) {
            snprintf(buf, sizeof(buf), ",\"action\":\"%s\"", e.action);
            json += buf;
            if (e.dest[0])    { json += ",\"dest\":"; appendJsonStr(json, e.dest); }
            if (e.viaCall[0]) { json += ",\"via\":";  appendJsonStr(json, e.viaCall); }
            snprintf(buf, sizeof(buf), ",\"hops\":%u", e.hops);
            json += buf;
        } else if (strcmp(e.event, "error") == 0) {
            json += ",\"source\":";
            appendJsonStr(json, e.source);
            json += ",\"text\":";
            appendJsonStr(json, e.text);
        }

        json += "}";
        count++;

        if (ESP.getFreeHeap() < 20000) break;
    }

    json += "]}";
    sendJsonResponse(request, json);
}

// ── Settings endpoint ───────────────────────────────────────────────────────

static void handleSettings(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;

    if (ESP.getFreeHeap() < 15000) {
        request->send(503, "application/json", "{\"error\":\"heap too low\"}");
        return;
    }

    String json;
    json.reserve(4096);
    json = "{\"settings\":{";

    char buf[256];

    // Basic info
    json += "\"mycall\":";
    appendJsonStr(json, settings.mycall);
    json += ",\"position\":";
    appendJsonStr(json, settings.position);
    json += ",\"ntp\":";
    appendJsonStr(json, settings.ntpServer);

    // Device info
    json += ",\"version\":";
    appendJsonStr(json, VERSION);
    json += ",\"name\":";
    appendJsonStr(json, NAME);
    json += ",\"hardware\":";
    appendJsonStr(json, PIO_ENV_NAME);

    // Chip ID
    {
        uint64_t mac = ESP.getEfuseMac();
        char chipId[13];
        snprintf(chipId, sizeof(chipId), "%02X%02X%02X%02X%02X%02X",
            (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
            (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)(mac));
        json += ",\"chipId\":";
        appendJsonStr(json, chipId);
    }

    snprintf(buf, sizeof(buf), ",\"webPasswordSet\":%s", !webPasswordHash.isEmpty() ? "true" : "false");
    json += buf;

    // WiFi settings
    snprintf(buf, sizeof(buf), ",\"dhcpActive\":%s,\"apMode\":%s",
             settings.dhcpActive ? "true" : "false",
             settings.apMode ? "true" : "false");
    json += buf;

    json += ",\"wifiSSID\":";
    appendJsonStr(json, settings.wifiSSID);
    json += ",\"wifiPassword\":";
    appendJsonStr(json, (settings.wifiPassword[0] != '\0') ? "***" : "");

    json += ",\"apName\":";
    appendJsonStr(json, apName.c_str());
    json += ",\"apPassword\":";
    appendJsonStr(json, apPassword.c_str());

    // WiFi networks array
    json += ",\"wifiNetworks\":[";
    for (size_t i = 0; i < wifiNetworks.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":";
        appendJsonStr(json, wifiNetworks[i].ssid);
        json += ",\"password\":";
        appendJsonStr(json, (wifiNetworks[i].password[0] != '\0') ? "***" : "");
        snprintf(buf, sizeof(buf), ",\"favorite\":%s}", wifiNetworks[i].favorite ? "true" : "false");
        json += buf;
    }
    json += "]";

    // Static IP addresses
    snprintf(buf, sizeof(buf), ",\"wifiIP\":[%u,%u,%u,%u]",
             settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"wifiNetMask\":[%u,%u,%u,%u]",
             settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"wifiGateway\":[%u,%u,%u,%u]",
             settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"wifiDNS\":[%u,%u,%u,%u]",
             settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"wifiBrodcast\":[%u,%u,%u,%u]",
             settings.wifiBrodcast[0], settings.wifiBrodcast[1], settings.wifiBrodcast[2], settings.wifiBrodcast[3]);
    json += buf;

    // Current IP (if connected)
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), ",\"currentIP\":[%u,%u,%u,%u]",
                 WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
        json += buf;
        snprintf(buf, sizeof(buf), ",\"currentNetMask\":[%u,%u,%u,%u]",
                 WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]);
        json += buf;
        snprintf(buf, sizeof(buf), ",\"currentGateway\":[%u,%u,%u,%u]",
                 WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);
        json += buf;
        snprintf(buf, sizeof(buf), ",\"currentDNS\":[%u,%u,%u,%u]",
                 WiFi.dnsIP()[0], WiFi.dnsIP()[1], WiFi.dnsIP()[2], WiFi.dnsIP()[3]);
        json += buf;
    }

    // LoRa settings
    snprintf(buf, sizeof(buf), ",\"loraFrequency\":%.4f,\"loraOutputPower\":%d,\"loraBandwidth\":%.1f",
             settings.loraFrequency, (int)settings.loraOutputPower, settings.loraBandwidth);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"loraSyncWord\":%u,\"loraCodingRate\":%u,\"loraSpreadingFactor\":%u",
             (unsigned)settings.loraSyncWord, (unsigned)settings.loraCodingRate, (unsigned)settings.loraSpreadingFactor);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"loraPreambleLength\":%d,\"loraRepeat\":%s,\"loraMaxMessageLength\":%u",
             (int)settings.loraPreambleLength, settings.loraRepeat ? "true" : "false",
             (unsigned)settings.loraMaxMessageLength);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"loraEnabled\":%s", loraEnabled ? "true" : "false");
    json += buf;

    // UDP peers array
    json += ",\"udpPeers\":[";
    for (size_t i = 0; i < udpPeers.size(); i++) {
        if (i > 0) json += ",";
        snprintf(buf, sizeof(buf), "{\"ip\":[%u,%u,%u,%u],\"legacy\":%s,\"enabled\":%s,\"call\":",
                 udpPeers[i][0], udpPeers[i][1], udpPeers[i][2], udpPeers[i][3],
                 udpPeerLegacy[i] ? "true" : "false",
                 udpPeerEnabled[i] ? "true" : "false");
        json += buf;
        appendJsonStr(json, (i < udpPeerCall.size()) ? udpPeerCall[i].c_str() : "");
        json += "}";
    }
    json += "]";

    // Hop limits and SNR
    snprintf(buf, sizeof(buf), ",\"maxHopMessage\":%u,\"maxHopPosition\":%u,\"maxHopTelemetry\":%u,\"minSnr\":%d",
             (unsigned)extSettings.maxHopMessage, (unsigned)extSettings.maxHopPosition,
             (unsigned)extSettings.maxHopTelemetry, (int)extSettings.minSnr);
    json += buf;

    // Misc settings
    snprintf(buf, sizeof(buf), ",\"updateChannel\":%u", (unsigned)updateChannel);
    json += buf;

#ifdef HAS_BATTERY_ADC
    json += ",\"hasBattery\":true";
#else
    json += ",\"hasBattery\":false";
#endif
    snprintf(buf, sizeof(buf), ",\"batteryEnabled\":%s,\"batteryFullVoltage\":%.1f",
             batteryEnabled ? "true" : "false", batteryFullVoltage);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"wifiTxPower\":%u,\"wifiMaxTxPower\":%u",
             (unsigned)wifiTxPower, (unsigned)WIFI_MAX_TX_POWER_DBM);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"displayBrightness\":%u,\"cpuFrequency\":%u",
             (unsigned)displayBrightness, (unsigned)cpuFrequency);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"oledEnabled\":%s,\"serialDebug\":%s",
             oledEnabled ? "true" : "false", serialDebug ? "true" : "false");
    json += buf;
    json += ",\"oledDisplayGroup\":";
    appendJsonStr(json, oledDisplayGroup);

    // Group names
    json += ",\"groupNames\":{";
    bool firstGroup = true;
    for (int i = 3; i <= MAX_CHANNELS; i++) {
        if (!firstGroup) json += ",";
        snprintf(buf, sizeof(buf), "\"%d\":", i);
        json += buf;
        appendJsonStr(json, groupNames[i]);
        firstGroup = false;
    }
    json += "}";

    json += "}}";
    sendJsonResponse(request, json);
}

// ── Diagnostics endpoint ────────────────────────────────────────────────────

static void handleDiagnostics(AsyncWebServerRequest *request) {
    if (!checkApiAuth(request)) return;
    if (!heapGuard(request)) return;

    String json;
    json.reserve(2048);
    char buf[256];

    // Heap section
    json = "{\"heap\":{";
    snprintf(buf, sizeof(buf), "\"free\":%u,\"minEver\":%u,\"maxBlock\":%u",
             ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    json += buf;
    json += "}";

    // WiFi section
    json += ",\"wifi\":{";
    snprintf(buf, sizeof(buf), "\"connected\":%s,\"rssi\":%d",
             WiFi.isConnected() ? "true" : "false", WiFi.RSSI());
    json += buf;
    json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += ",\"mac\":\"" + WiFi.macAddress() + "\"";
    json += ",\"ssid\":";
    appendJsonStr(json, WiFi.SSID().c_str());
    snprintf(buf, sizeof(buf), ",\"channel\":%d,\"disconnects\":%lu,\"lastDisconnectReason\":%u,\"lastDisconnectTime\":%lu",
             WiFi.channel(), (unsigned long)wifiDisconnectCount,
             lastWifiDisconnectReason, (unsigned long)lastWifiDisconnectTime);
    json += buf;
    json += "}";

    // LoRa section
    json += ",\"lora\":{";
    snprintf(buf, sizeof(buf), "\"txQueue\":%u,\"txTotal\":%lu,\"rxTotal\":%lu",
             (unsigned)txBuffer.size(), (unsigned long)apiTxTotal, (unsigned long)apiRxTotal);
    json += buf;
    json += "}";

    // Mesh section
    json += ",\"mesh\":{";
    snprintf(buf, sizeof(buf), "\"peerCount\":%u,\"routeCount\":%u,\"msgBufferUsed\":%u,\"msgBufferMax\":%u,\"eventBufferUsed\":%u,\"eventBufferMax\":%u",
             (unsigned)peerList.size(), (unsigned)routingList.size(),
             (unsigned)msgCount, (unsigned)API_MSG_BUFFER_SIZE,
             (unsigned)evtCount, (unsigned)API_EVT_BUFFER_SIZE);
    json += buf;
    json += "}";

    // System section
    json += ",\"system\":{";
    json += "\"version\":\"" + String(VERSION) + "\"";
    json += ",\"board\":\"" + String(getBoardName()) + "\"";
    snprintf(buf, sizeof(buf), ",\"uptime\":%lu", millis() / 1000);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"resetReason\":\"%s\"", apiGetResetReason());
    json += buf;
    snprintf(buf, sizeof(buf), ",\"resetCount\":%lu", (unsigned long)nvsResetCount);
    json += buf;
    snprintf(buf, sizeof(buf), ",\"cpuFreqMHz\":%u", (unsigned)ESP.getCpuFreqMHz());
    json += buf;
    snprintf(buf, sizeof(buf), ",\"flashSizeKB\":%lu", (unsigned long)(ESP.getFlashChipSize() / 1024));
    json += buf;
    json += ",\"sdkVersion\":\"" + String(ESP.getSdkVersion()) + "\"";
    json += ",\"compileTime\":\"" + String(__DATE__) + " " + String(__TIME__) + "\"";
    json += "}";

    // NTP section
    json += ",\"ntp\":{";
    time_t now = time(nullptr);
    bool ntpSynced = now > 1700000000;
    snprintf(buf, sizeof(buf), "\"synced\":%s,\"lastSyncTime\":%lu",
             ntpSynced ? "true" : "false", (unsigned long)lastNtpSyncTime);
    json += buf;
    json += ",\"server\":";
    appendJsonStr(json, settings.ntpServer);
    json += "}";

    // Tasks section — report stack high watermark for the main loop task
    json += ",\"tasks\":[";
    if (mainLoopTaskHandle) {
        uint32_t hwm = uxTaskGetStackHighWaterMark(mainLoopTaskHandle) * sizeof(StackType_t);
        json += "{\"name\":\"loopTask\"";
        snprintf(buf, sizeof(buf), ",\"stackHighWater\":%lu,\"priority\":%lu,\"core\":%d}",
                 (unsigned long)hwm,
                 (unsigned long)uxTaskPriorityGet(mainLoopTaskHandle),
                 (int)xTaskGetAffinity(mainLoopTaskHandle));
        json += buf;
    }
    json += "]";

    json += "}";
    sendJsonResponse(request, json);
}

// ── Register endpoints ──────────────────────────────────────────────────────

void setupApiEndpoints(AsyncWebServer &server) {
    // Read and increment reset counter from NVS
    Preferences diagPrefs;
    diagPrefs.begin("rmesh_diag", false);
    nvsResetCount = diagPrefs.getUInt("resets", 0) + 1;
    diagPrefs.putUInt("resets", nvsResetCount);
    diagPrefs.end();

    // Register NTP sync callback
    sntp_set_time_sync_notification_cb(onNtpSync);

    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/peers", HTTP_GET, handlePeers);
    server.on("/api/routes", HTTP_GET, handleRoutes);
    server.on("/api/messages", HTTP_GET, handleGetMessages);
    server.on("/api/groups", HTTP_GET, handleGroups);
    server.on("/api/events", HTTP_GET, handleEvents);
    server.on("/api/settings", HTTP_GET, handleSettings);
    server.on("/api/diagnostics", HTTP_GET, handleDiagnostics);

    // POST /api/messages with body handler
    server.on("/api/messages", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        nullptr,
        handlePostMessage
    );

    logPrintf(LOG_INFO, "API", "REST API endpoints registered");
}

#endif
