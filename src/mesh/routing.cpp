#include <Arduino.h>
#include <ArduinoJson.h>

#include "main.h"
#include "mesh/routing.h"
#include "mesh/reporting.h"
#include "network/webFunctions.h"
#include "mesh/peer.h"
#include "util/helperFunctions.h"
#include "config.h"
#include "hal/settings.h"
#include "util/persistence.h"
#include "util/logging.h"
#include "network/api.h"
#include "util/link_math.h"

//Routing list
std::vector<Route> routingList;

void getRoute(const char* dstCall, char* viaCall, size_t len) {
    //Serial.printf("dst:%s via:%s\r\n", dstCall, viaCall);
    viaCall[0] = '\0';
    //Search routing list
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) { return (strcmp(r.srcCall, dstCall) == 0); });

    if (it != routingList.end()) {
        //Check if call is still in peer list
        auto it2 = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (strcmp(peer.nodeCall, it->viaCall) == 0) && (peer.available == true) ; });
        if (it2 != peerList.end()) {
            strncpy(viaCall, it->viaCall, len - 1);
            viaCall[len - 1] = '\0';
        } 
    } 
}

void removeDirectRoute(const char* call) {
    ListLock _ll;  // guard routingList against concurrent bgWorker reads
    // Remove a direct-neighbor route entry (srcCall == viaCall == call, hopCount 0)
    // so that an alternative multi-hop route can be learned via another peer.
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) {
        return (strcmp(r.srcCall, call) == 0) && (strcmp(r.viaCall, call) == 0);
    });
    if (it != routingList.end()) {
        logPrintf(LOG_INFO, "Route", "Removing direct route to %s (SNR below threshold)", call);
        routingList.erase(it);
        routesDirty = true;
        sendRoutingList();
        markTopologyChanged();
    }
}

bool checkRoute(char* srcCall, char* viaCall) {
    //Search routing list
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) { return (strcmp(r.srcCall, srcCall) == 0) && (strcmp(r.viaCall, viaCall) == 0); });
    if (it != routingList.end()) { return true; }
    return false;
}

void sendRoutingList() {
    // Replaced: no longer serializes full JSON on heap.
    // Sends lightweight notification; WebUI fetches /api/routes instead.
    notifyRoutingChanged();
}

void addRoutingList(const char* srcCall, const char* viaCall, uint8_t hopCount) {
    ListLock _ll;  // guard routingList against concurrent bgWorker reads
    if (strlen(srcCall) == 0 || strlen(viaCall) == 0) return;
    if (strcmp(settings.mycall, srcCall) == 0) return;
    // Loop detection: reject routes that point back to ourselves.
    // viaCall == srcCall is the legitimate "direct neighbor" case (hopCount must be 0);
    // only reject when the hopCount is inconsistent with that.
    if (strcmp(viaCall, srcCall) == 0 && hopCount != 0) return;
    if (strcmp(viaCall, settings.mycall) == 0) return;

    // 1. Check if viaCall (the next hop) is in the peer list
    auto itt = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { 
        return (strcmp(peer.nodeCall, viaCall) == 0) && (peer.available == true); 
    });
    if (itt == peerList.end()) return;

    // 2. Search routing list for the destination call (srcCall)
    auto it = std::find_if(routingList.begin(), routingList.end(), [&](const Route& r) {
        return (strcmp(r.srcCall, srcCall) == 0);
    });

    bool isNew = (it == routingList.end());

    if (isNew) {
        // Case A: Destination unknown -> Create new (evict oldest if full)
        if (routingList.size() >= ROUTING_BUFFER_SIZE) {
            // Evict oldest entry by timestamp
            auto oldest = std::min_element(routingList.begin(), routingList.end(),
                [](const Route& a, const Route& b) { return a.timestamp < b.timestamp; });
            if (oldest != routingList.end()) {
                logPrintf(LOG_WARN, "Route", "Table full, evicting %s", oldest->srcCall);
                routingList.erase(oldest);
            }
        }
        Route r;
        strncpy(r.srcCall, srcCall, MAX_CALLSIGN_LENGTH);
        r.srcCall[MAX_CALLSIGN_LENGTH] = '\0';
        strncpy(r.viaCall, viaCall, MAX_CALLSIGN_LENGTH);
        r.viaCall[MAX_CALLSIGN_LENGTH] = '\0';
        r.timestamp = time(NULL);
        r.hopCount = hopCount;
        routingList.push_back(r);
        routesDirty = true;
        logPrintf(LOG_INFO, "Route", "New route: %s via %s (%d hops)", srcCall, viaCall, hopCount);
    } else {
        // Case B: Destination exists
        if (strcmp(it->viaCall, viaCall) == 0) {
            // Same via node -> refresh. A shorter hop count via the same node is
            // taken over; otherwise this is the common steady-state case (every
            // overheard frame refreshes an existing route). Return early then:
            // re-sorting the whole table and firing a routes WS-notify (→ every WebUI
            // re-GETs /api/routes) on an unchanged topology is pure CPU/heap/REST churn.
            it->timestamp = time(NULL);
            if (hopCount >= it->hopCount) return;
            routesDirty = true;
            it->hopCount = hopCount;
        } else {
            // Different via node -> switch only if the metric (hops + link penalty
            // of the next hop) is better by more than the hysteresis, so a 1-hop
            // route over a link at the SF limit can lose against 2 clean hops.
            float newMetric = routeMetric(hopCount, peerLinkPenalty(viaCall));
            float oldMetric = routeMetric(it->hopCount, peerLinkPenalty(it->viaCall));
            if (!routeShouldSwitch(newMetric, oldMetric)) return;
            routesDirty = true;
            logPrintf(LOG_INFO, "Route", "Updated route: %s via %s (%d hops, metric %.1f, was %s/%d hops, %.1f)",
                      srcCall, viaCall, hopCount, newMetric, it->viaCall, it->hopCount, oldMetric);
            strncpy(it->viaCall, viaCall, MAX_CALLSIGN_LENGTH);
            it->viaCall[MAX_CALLSIGN_LENGTH] = '\0';
            it->timestamp = time(NULL);
            it->hopCount = hopCount;
        }
    }

    // 3. Sort (shortest hops first)
    std::sort(routingList.begin(), routingList.end(), [](const Route& a, const Route& b) {
        if (a.hopCount != b.hopCount) return a.hopCount < b.hopCount;
        return a.timestamp > b.timestamp;
    });

    sendRoutingList();
    if (isNew) markTopologyChanged();
}
