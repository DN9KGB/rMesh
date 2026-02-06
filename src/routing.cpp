#include <Arduino.h>
#include <ArduinoJson.h>

#include "routing.h"
#include "webFunctions.h"

//Routing Liste
std::vector<Route> routingList;

void sendRoutingList() {
    JsonDocument doc;
    doc["routingList"]["routes"] = JsonArray();
    for (int i = 0; i < routingList.size(); i++) {
        JsonObject route = doc["routingList"]["routes"].add<JsonObject>();
        route["srcCall"] = routingList[i].srcCall;
        route["viaCall"] = routingList[i].viaCall;
    }
    char* jsonBuffer = (char*)malloc(2048);
    size_t len = serializeJson(doc, jsonBuffer, 2048);
    ws.textAll(jsonBuffer, len);
    free(jsonBuffer);
}

void addRoutingList(const char* srcCall, const char* viaCall) {
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) {
        return (strcmp(r.srcCall, srcCall) == 0) && (strcmp(r.viaCall, viaCall) == 0);
    });

    if (it == routingList.end()) {
        Route r;
        memcpy(r.srcCall, srcCall, MAX_CALLSIGN_LENGTH + 1);
        memcpy(r.viaCall, viaCall, MAX_CALLSIGN_LENGTH + 1);
        routingList.push_back(r);
        sendRoutingList();
    }
}
