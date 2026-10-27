#include "util/fsCheck.h"
#include "util/logging.h"

const char EMERGENCY_HELP[] =
    "Notfall-Hilfe (help.txt fehlt, Dateisystem leer):\n"
    "  settings                     Einstellungen anzeigen\n"
    "  call <rufzeichen>            Rufzeichen setzen\n"
    "  pos <position>               Position setzen (z.B. JN48mw)\n"
    "  wifi add <SSID> [<PW>]       WLAN hinzufuegen\n"
    "  wifi list                    WLAN-Netzwerke anzeigen\n"
    "  updf <0/1>                   Firmware + WebUI/Hilfe neu laden (0=Release, 1=Dev)\n"
    "  reboot                       Neustart\n";

#ifndef NRF52_PLATFORM
#include <LittleFS.h>
#include <esp_partition.h>

bool fsWasFormatted = false;

bool webUiFilesPresent() {
    return LittleFS.exists("/index.html") || LittleFS.exists("/index.html.gz");
}

uint32_t fsPartitionOffset() {
    const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    return p ? p->address : 0;
}

void logFsMissingHint() {
    logPrintf(LOG_WARN, "FS", "Filesystem empty%s - WebUI and help.txt missing.",
              fsWasFormatted ? " (was formatted)" : "");
    logPrintf(LOG_WARN, "FS", "littlefs.bin belongs at 0x%X. Run 'updf' (needs WiFi) or reflash it.",
              (unsigned)fsPartitionOffset());
}
#endif
