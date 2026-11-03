# Frame-Format v2 – Designkonzept (RME-3, nicht implementiert)

Status: **Konzept**. Dieses Dokument beschreibt eine mögliche kompakte Frame-Variante.
In der Firmware ist davon nichts umgesetzt; das On-Air-Format v1 bleibt unverändert.

## Ausgangslage (v1)

Ein v1-Frame (`Frame::exportBinary()`) besteht aus:

| Byte | Inhalt |
|------|--------|
| 0 | Frame-Typ (Bit 0–3), Hop-Count (Bit 4–7) |
| n | Header-Byte: Typ (Bit 4–7), Länge bzw. Message-Typ (Bit 0–3), danach die Nutzdaten |

Header-Typen: `SRC_CALL`, `DST_GROUP`, `MESSAGE`, `NODE_CALL`, `VIA_CALL`, `DST_CALL` (0–5).
Callsigns stehen als ASCII im Klartext, je Feld 1 Byte Header + bis zu 9 Byte.

Typischer Aufwand für eine geroutete Textnachricht `DL1ABC-12 → DB0XYZ-1 via DO2QQ-7`:

| Feld | Bytes |
|------|-------|
| Typ/Hop | 1 |
| SRC + NODE + VIA + DST (je 1 + 6…9) | ~36 |
| MESSAGE-Header + ID | 5 |
| **Overhead** | **~42** |

Bei SF11/BW62,5 („Weit“) kostet jedes Byte rund 26 ms Airtime – der Header allein
liegt damit über einer Sekunde.

## Ziele

1. Weniger Airtime pro Frame, vor allem in den Profilen „Robust“ und „Weit“.
2. Optional eine eigene Prüfsumme, die **Packet Combining** ermöglicht
   (mehrere fehlerhafte Kopien desselben Frames zu einer korrekten zusammensetzen).
3. Volle Rückwärtskompatibilität: v1-Nodes müssen weiter funktionieren, ein Netz
   darf gemischt sein. v2 wird nur zwischen Nachbarn benutzt, die es beide können.

## 1. Kompakte Callsigns („packed calls“)

Amateurfunk-Callsigns nutzen nur `A–Z`, `0–9`, `/` und `-`. Mit einem 40-Zeichen-
Alphabet (Leerzeichen/Ende, A–Z, 0–9, `/`, `-`, Reserve) lassen sich 3 Zeichen in
16 Bit kodieren (40³ = 64000 < 65536), ähnlich wie bei AX.25/APRS-Varianten.

* 9 Zeichen (Maximum `MAX_CALLSIGN_LENGTH`) → 6 Byte statt 9.
* Die Länge ist implizit (Füllzeichen 0), das Längen-Nibble im Header wird frei.
* Callsigns mit anderen Zeichen (Kleinbuchstaben, Umlaute, Gruppen-Namen) bleiben
  im v1-Klartextfeld; der Encoder wählt pro Feld.

### Header-Byte v2

| Bit 7–4 | Bit 3 | Bit 2–0 |
|---------|-------|---------|
| Header-Typ (wie v1) | 1 = packed | Anzahl 16-Bit-Wörter (1–3) |

Bei `packed = 0` bleibt Bit 3–0 die v1-Länge (max. 7 Zeichen im v2-Rahmen, längere
Felder zwingen den Encoder auf v1). Für das Beispiel oben sinkt der Overhead von ~42
auf ~33 Byte; mit der Option, `NODE_CALL` bei Direktverbindungen wegzulassen (= SRC),
auf ~26 Byte.

## 2. Optionale eigene CRC für Packet Combining

Die LoRa-PHY-CRC verwirft fehlerhafte Pakete im Chip bzw. markiert sie
(`RADIOLIB_ERR_CRC_MISMATCH`, Zähler `crcErr` in der Radio-Statistik). Für Packet
Combining müssen beschädigte Kopien aber aufgehoben werden:

* Sender hängt eine **CRC-16/CCITT** über das gesamte v2-Frame an (2 Byte).
* Empfänger nimmt bei aktivem v2 auch Pakete mit PHY-CRC-Fehler an und puffert die
  letzten N (z. B. 4) Kopien gleicher Länge für ein kurzes Zeitfenster (Retry-Fenster).
* Zusammensetzen per Mehrheitsentscheid je Bit (ab 3 Kopien) oder durch gezieltes
  Durchprobieren der unsicheren Bits (2 Kopien, begrenzt auf wenige abweichende Bits);
  angenommen wird nur, wenn die eigene CRC passt.
* Die PHY-CRC bleibt eingeschaltet; die eigene CRC ersetzt sie nicht, sondern dient
  als Prüfung des kombinierten Ergebnisses.

Kosten: +2 Byte pro Frame, RAM für den Kombinationspuffer (4 × 255 Byte), CPU vernachlässigbar.
Nutzen: Retries, die heute einzeln verloren gehen, ergeben zusammen ein gültiges Frame –
vor allem an der Reichweitengrenze, wo die Radio-Statistik viele CRC-Fehler zeigt.

## 3. Aushandlung über ANNOUNCE

v2 darf nur gesendet werden, wenn **alle** möglichen Empfänger es verstehen. Da
Floods (Gruppen, Routing-Beacons) jeden Nachbarn erreichen, gilt:

* **Unicast an einen Nachbarn** (`VIA_CALL` gesetzt): v2, wenn dieser Peer die
  Fähigkeit angekündigt hat.
* **Floods und Broadcasts**: immer v1, solange nicht alle verfügbaren LoRa-Peers v2
  können (gemischte Netze bleiben voll funktionsfähig).

### Capability-Header

Neuer Header-Typ `CAPS_HEADER = 6` in `ANNOUNCE`/`ANNOUNCE_ACK`, **als letztes Feld**:

| Byte | Inhalt |
|------|--------|
| 0 | `0x6` << 4 \| Länge (1) |
| 1 | Bitfeld: Bit 0 = packed calls, Bit 1 = eigene CRC/Combining, Bit 2–7 reserviert |

Warum das abwärtskompatibel ist: `Frame::importBinary()` bricht bei einem unbekannten
Header-Typ ab (`default: i = length`). Alle vorher stehenden Felder sind dann schon
gelesen, eine v1-Firmware ignoriert die Capability also nur. Deshalb muss der Header am
Ende stehen.

Die Fähigkeiten landen im `Peer`-Eintrag (Laufzeitfeld, nicht persistiert) und
verfallen mit dem Peer-Timeout.

### Erkennung v2 beim Empfang

Byte 0 bleibt in v2 gleich (Frame-Typ/Hop). Frame-Typen 0–4 sind belegt; v2-Frames
nutzen **Frame-Typ + 8** (8–12). Eine v1-Firmware kennt diese Typen nicht und verwirft
das Frame, statt es falsch zu lesen. Das ist gewollt, weil v2 nur an v2-fähige
Empfänger geht.

## 4. Offene Punkte

* Hop-Count: 4 Bit reichen; in v2 könnte Bit 3 des Typs als „CRC vorhanden“ dienen,
  dann reichen Frame-Typen 8–12 + Flag.
* Message-ID: 4 Byte sind für Duplikaterkennung reichlich; 3 Byte würden genügen,
  verkürzen aber die Kollisionsfreiheit im Netz – erst mit Messdaten entscheiden.
* WebUI/Monitor zeigen weiterhin die dekodierten Felder; der Monitor sollte die
  Version als Feld (`"fv":2`) ausgeben.
* Tests: Roundtrip v1↔v2 in `test/test_frame`, Fuzzing von `importBinary` mit
  v2-Typen auf v1-Code (Abbruch, kein Überlauf).

## 5. Reihenfolge der Umsetzung (Vorschlag)

1. `CAPS_HEADER` senden und auswerten (ohne v2 zu nutzen) – ein Release Vorlauf,
   damit Peers die Fähigkeit schon kennen.
2. Packed calls für Unicast.
3. Eigene CRC + Combining-Puffer, gemessen über die Radio-Statistik (`crcErr` vs. `rxOk`).
