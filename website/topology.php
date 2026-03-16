<?php
/**
 * rMesh Topology Query Endpoint
 * Gibt die bekannte Netz-Topologie als JSON zurück.
 *
 * GET Parameter:
 *   max_age  (Sekunden, default 7200)
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

require_once __DIR__ . '/db_config.php';

$maxAge = max(600, min(86400, (int)(isset($_GET['max_age']) ? $_GET['max_age'] : 7200)));
$cutoff = time() - $maxAge;

try {
    $dsn = 'mysql:host=' . DB_HOST . ';dbname=' . DB_NAME . ';charset=' . DB_CHARSET;
    $db  = new PDO($dsn, DB_USER, DB_PASS, array(PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION));

    // Nodes
    $nodes = array();
    $stmt  = $db->prepare("SELECT `call`, `position`, `last_seen` FROM rmesh_nodes WHERE `last_seen` >= :cutoff ORDER BY `call`");
    $stmt->execute(array(':cutoff' => $cutoff));
    foreach ($stmt->fetchAll(PDO::FETCH_ASSOC) as $row) {
        $node = array(
            'call'      => $row['call'],
            'position'  => $row['position'],
            'last_seen' => (int)$row['last_seen'],
        );
        $pos = parsePosition($row['position']);
        if ($pos !== null) {
            $node['lat'] = $pos[0];
            $node['lon'] = $pos[1];
        }
        $nodes[] = $node;
    }

    // Edges: deduplizierte direkte Peer-Verbindungen
    $edgeMap = array();
    $stmt = $db->prepare("
        SELECT `reporter_call`, `peer_call`, `rssi`, `snr`, `port`, `last_seen`
        FROM rmesh_peers
        WHERE `last_seen` >= :cutoff AND `available` = 1
        ORDER BY `last_seen` DESC
    ");
    $stmt->execute(array(':cutoff' => $cutoff));
    foreach ($stmt->fetchAll(PDO::FETCH_ASSOC) as $row) {
        $a = $row['reporter_call'];
        $b = $row['peer_call'];
        $key = (strcmp($a, $b) <= 0 ? "$a|$b" : "$b|$a") . '|' . $row['port'];
        if (!isset($edgeMap[$key])) {
            $edgeMap[$key] = array(
                'from'      => $a,
                'to'        => $b,
                'rssi'      => (float)$row['rssi'],
                'snr'       => (float)$row['snr'],
                'port'      => (int)$row['port'],
                'last_seen' => (int)$row['last_seen'],
            );
        }
    }

    // Route-Hints
    $routeEdges = array();
    $stmt = $db->prepare("
        SELECT `reporter_call`, `src_call`, `via_call`, `hop_count`, `last_seen`
        FROM rmesh_routes
        WHERE `last_seen` >= :cutoff
        ORDER BY `hop_count` ASC
    ");
    $stmt->execute(array(':cutoff' => $cutoff));
    foreach ($stmt->fetchAll(PDO::FETCH_ASSOC) as $row) {
        $key = $row['reporter_call'] . '|' . $row['src_call'];
        $routeEdges[$key] = array(
            'reporter'  => $row['reporter_call'],
            'src'       => $row['src_call'],
            'via'       => $row['via_call'],
            'hops'      => (int)$row['hop_count'],
            'last_seen' => (int)$row['last_seen'],
        );
    }

    echo json_encode(array(
        'nodes'       => array_values($nodes),
        'edges'       => array_values($edgeMap),
        'route_hints' => array_values($routeEdges),
        'generated'   => time(),
        'max_age'     => $maxAge,
    ));

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(array('error' => 'Database error', 'detail' => $e->getMessage()));
    error_log('rMesh topology.php: ' . $e->getMessage());
}

function parsePosition($pos) {
    $pos = trim($pos);
    if ($pos === '') return null;

    // lat,lon Format
    if (strpos($pos, ',') !== false) {
        $parts = explode(',', $pos, 2);
        $lat = (float)trim($parts[0]);
        $lon = (float)trim($parts[1]);
        if ($lat >= -90 && $lat <= 90 && $lon >= -180 && $lon <= 180) {
            return array($lat, $lon);
        }
        return null;
    }

    // Maidenhead-Locator (JN48 oder JN48mw)
    if (preg_match('/^([A-R]{2})(\d{2})([A-X]{2})?$/i', $pos, $m)) {
        $lon = (ord(strtoupper($m[1][0])) - ord('A')) * 20 - 180;
        $lat = (ord(strtoupper($m[1][1])) - ord('A')) * 10 - 90;
        $lon += (int)$m[2][0] * 2;
        $lat += (int)$m[2][1];
        if (!empty($m[3])) {
            $lon += (ord(strtolower($m[3][0])) - ord('a')) * (2.0 / 24) + (1.0 / 24);
            $lat += (ord(strtolower($m[3][1])) - ord('a')) * (1.0 / 24) + (0.5 / 24);
        } else {
            $lon += 1.0;
            $lat += 0.5;
        }
        return array(round($lat, 5), round($lon, 5));
    }

    return null;
}
