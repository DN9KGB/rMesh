## rMesh
... ist ein spezialisiertes Messenger-Protokoll. Es verfolgt ein einziges, kompromissloses Ziel: **Die sichere Zustellung von Textnachrichten über HF-Verbindungen bei maximaler Effizienz.**

## Die Philosophie: "Pure Messaging"

Während andere Mesh-Projekte zu aufgeblähten "Schweizer Taschenmessern" verkommen sind, bleibt **rMesh** radikal minimalistisch. 

Alles, was nicht direkt zur Übermittlung einer Nachricht dient, macht das System schlechter.

rMesh erzwingt eine strikte Trennung zwischen nützlicher Information und technischem Ballast. Folgende Datenströme sind auf der HF-Schnittstelle **strikt untersagt**:

* **KEINE Positionen:** GPS-Koordinaten haben in einem reinen Messenger nichts zu suchen.
* **KEINE Telemetrie:** Batteriespannungen, RSSI-Graphen oder Temperaturen gehören nicht in den Nutzdatenkanal.
* **KEINE Fernsteuerung:** Keine Remote-Admin-Befehle, die wertvolle Bandbreite fressen.
* **KEINE Debug-Daten:** Fehlerprotokolle gehören an den USB-Port, nicht in die Luft.
* **MINIMALER Routing-Overhead:** Nur das absolut notwendige Minimum an Headern, um ein Paket von A nach B zu bringen.

Wer bunte Karten, Tracking und Telemetrie-Dashboards sucht, ist bei anderen Projekten besser aufgehoben. rMesh ist für diejenigen, die eine Nachricht durchbringen wollen, wenn der Rest im Rauschen untergeht.

## Anforderungen
* ESP32 / ESP8266
* LoRa-Modul (oder vergleichbare HF-Hardware)

## Lizenz: GNU GPLv3
**rMesh** ist unter der **GNU General Public License v3.0 (GPLv3)** lizenziert.

