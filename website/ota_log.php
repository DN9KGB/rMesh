<?php
/**
 * rMesh OTA Update Log Endpoint
 * Direkte POST-Meldungen vom Node (z.B. update_success / update_failed).
 *
 * POST Body (JSON):
 * {
 *   "call":         "DN9KGB",
 *   "device":       "LILYGO_T-LoraPager",
 *   "event":        "update_success|update_failed|...",
 *   "version_from": "v1.0.23",
 *   "version_to":   "v1.0.24",
 *   "error":        ""
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

if (!$data || empty($data['call']) || empty($data['event'])) {
    http_response_code(400);
    echo json_encode(array('error' => 'Invalid payload'));
    exit;
}

$allowed_events = array(
    'update_found', 'update_start', 'update_success', 'update_failed', 'version_check_failed'
);

$call         = strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($data['call'])), 0, 16));
$device       = isset($data['device'])       ? substr($data['device'],       0, 64)  : '';
$event        = substr(preg_replace('/[^a-z_]/', '', strtolower($data['event'])), 0, 32);
$version_from = isset($data['version_from']) ? substr($data['version_from'], 0, 32)  : '';
$version_to   = isset($data['version_to'])   ? substr($data['version_to'],   0, 32)  : '';
$error        = isset($data['error'])        ? substr($data['error'],        0, 512) : '';

if (empty($call) || !in_array($event, $allowed_events)) {
    http_response_code(400);
    echo json_encode(array('error' => 'Invalid callsign or event'));
    exit;
}

require_once __DIR__ . '/ota_log_helper.php';
logOtaEvent($call, $device, $event, $version_from, $version_to, $error);
echo json_encode(array('ok' => true));
