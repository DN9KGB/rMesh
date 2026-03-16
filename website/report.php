<?php
/**
 * rMesh Topology Report Endpoint
 * Nodes mit Internet-Uplink POSTen hier ihre Topologie-Daten.
 *
 * POST Body (JSON):
 * {
 *   "call":      "DN9KGB",
 *   "position":  "JN48mw",
 *   "timestamp": 1710000000,
 *   "peers": [
 *     {"call": "OE3XYZ", "rssi": -85.0, "snr": 5.2, "port": 0, "available": true}
 *   ],
 *   "routes": [
 *     {"src": "DB0XYZ", "via": "OE3XYZ", "hops": 2}
 *   ]
 * }
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    echo json_encode(array('error' => 'Method Not Allowed'));
    exit;
}

$body = file_get_contents('php://input');
$data = json_decode($body, true);

if (!$data || empty($data['call'])) {
    http_response_code(400);
    echo json_encode(array('error' => 'Invalid payload'));
    exit;
}

$call      = strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($data['call'])), 0, 16));
$position  = isset($data['position'])  ? substr($data['position'], 0, 23) : '';
$timestamp = isset($data['timestamp']) ? (int)$data['timestamp'] : time();
$peers     = isset($data['peers'])     ? $data['peers']  : array();
$routes    = isset($data['routes'])    ? $data['routes'] : array();

if (empty($call)) {
    http_response_code(400);
    echo json_encode(array('error' => 'Invalid callsign'));
    exit;
}

require_once __DIR__ . '/db_config.php';

try {
    $dsn = 'mysql:host=' . DB_HOST . ';dbname=' . DB_NAME . ';charset=' . DB_CHARSET;
    $db  = new PDO($dsn, DB_USER, DB_PASS, array(PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION));

    // Schema anlegen (einmalig)
    $db->exec("CREATE TABLE IF NOT EXISTS rmesh_nodes (
        `call`     VARCHAR(16)  NOT NULL,
        `position` VARCHAR(23)  NOT NULL DEFAULT '',
        `last_seen` INT UNSIGNED NOT NULL,
        PRIMARY KEY (`call`)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    $db->exec("CREATE TABLE IF NOT EXISTS rmesh_peers (
        `reporter_call` VARCHAR(16)   NOT NULL,
        `peer_call`     VARCHAR(16)   NOT NULL,
        `rssi`          FLOAT         NOT NULL DEFAULT 0,
        `snr`           FLOAT         NOT NULL DEFAULT 0,
        `port`          TINYINT       NOT NULL DEFAULT 0,
        `available`     TINYINT       NOT NULL DEFAULT 1,
        `last_seen`     INT UNSIGNED  NOT NULL,
        PRIMARY KEY (`reporter_call`, `peer_call`, `port`)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    $db->exec("CREATE TABLE IF NOT EXISTS rmesh_routes (
        `reporter_call` VARCHAR(16)   NOT NULL,
        `src_call`      VARCHAR(16)   NOT NULL,
        `via_call`      VARCHAR(16)   NOT NULL,
        `hop_count`     TINYINT       NOT NULL DEFAULT 0,
        `last_seen`     INT UNSIGNED  NOT NULL,
        PRIMARY KEY (`reporter_call`, `src_call`)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    $db->beginTransaction();

    // Node eintragen / aktualisieren
    $stmt = $db->prepare("INSERT INTO rmesh_nodes (`call`, `position`, `last_seen`)
        VALUES (:call, :pos, :ts)
        ON DUPLICATE KEY UPDATE `position`=VALUES(`position`), `last_seen`=VALUES(`last_seen`)");
    $stmt->execute(array(':call' => $call, ':pos' => $position, ':ts' => $timestamp));

    // Peers: alle als nicht verfügbar markieren, dann gemeldete eintragen
    $db->prepare("UPDATE rmesh_peers SET `available`=0 WHERE `reporter_call`=:r")
       ->execute(array(':r' => $call));

    $stmtPeer = $db->prepare("INSERT INTO rmesh_peers
        (`reporter_call`, `peer_call`, `rssi`, `snr`, `port`, `available`, `last_seen`)
        VALUES (:r, :p, :rssi, :snr, :port, :avail, :ts)
        ON DUPLICATE KEY UPDATE
            `rssi`=VALUES(`rssi`), `snr`=VALUES(`snr`),
            `available`=VALUES(`available`), `last_seen`=VALUES(`last_seen`)");

    foreach ($peers as $peer) {
        if (empty($peer['call'])) continue;
        $peerCall = strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($peer['call'])), 0, 16));
        if (empty($peerCall)) continue;
        $stmtPeer->execute(array(
            ':r'     => $call,
            ':p'     => $peerCall,
            ':rssi'  => (float)(isset($peer['rssi'])      ? $peer['rssi']      : 0),
            ':snr'   => (float)(isset($peer['snr'])       ? $peer['snr']       : 0),
            ':port'  => (int)(isset($peer['port'])        ? $peer['port']      : 0),
            ':avail' => (int)(bool)(isset($peer['available']) ? $peer['available'] : true),
            ':ts'    => $timestamp,
        ));
    }

    // Routen
    $stmtRoute = $db->prepare("INSERT INTO rmesh_routes
        (`reporter_call`, `src_call`, `via_call`, `hop_count`, `last_seen`)
        VALUES (:r, :src, :via, :hops, :ts)
        ON DUPLICATE KEY UPDATE
            `via_call`=VALUES(`via_call`), `hop_count`=VALUES(`hop_count`), `last_seen`=VALUES(`last_seen`)");

    foreach ($routes as $route) {
        if (empty($route['src']) || empty($route['via'])) continue;
        $srcCall = strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($route['src'])), 0, 16));
        $viaCall = strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($route['via'])), 0, 16));
        if (empty($srcCall) || empty($viaCall)) continue;
        $stmtRoute->execute(array(
            ':r'    => $call,
            ':src'  => $srcCall,
            ':via'  => $viaCall,
            ':hops' => (int)(isset($route['hops']) ? $route['hops'] : 0),
            ':ts'   => $timestamp,
        ));
    }

    $db->commit();
    echo json_encode(array('ok' => true));

} catch (Exception $e) {
    if (isset($db) && $db->inTransaction()) {
        $db->rollBack();
    }
    http_response_code(500);
    echo json_encode(array('error' => 'Database error'));
    error_log('rMesh report.php: ' . $e->getMessage());
}
