<?php
header('Content-Type: application/json');
header('Cache-Control: no-cache, no-store, must-revalidate');

$call    = isset($_GET['call'])    ? strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($_GET['call'])), 0, 16)) : '';
$device  = isset($_GET['device'])  ? substr($_GET['device'],  0, 64) : '';
$version = isset($_GET['version']) ? substr($_GET['version'], 0, 32) : '';

$ctx = stream_context_create(['http' => [
    'header'  => "User-Agent: rMesh-Website\r\n",
    'timeout' => 5,
]]);

$json = @file_get_contents('https://api.github.com/repos/DN9KGB/rMesh/releases/latest', false, $ctx);

if ($json) {
    $data   = json_decode($json);
    $latest = $data->tag_name;
    echo json_encode(['version' => $latest]);

    require_once __DIR__ . '/ota_log_helper.php';
    logOtaEvent($call, $device, 'version_check', $version, $latest, '');
    if ($version && $version !== $latest) {
        logOtaEvent($call, $device, 'update_found', $version, $latest, '');
    } elseif ($version && $version === $latest) {
        logOtaEvent($call, $device, 'no_update', $version, $latest, '');
    }
} else {
    http_response_code(503);
    echo json_encode(['version' => 'unknown']);

    require_once __DIR__ . '/ota_log_helper.php';
    logOtaEvent($call, $device, 'version_check_failed', $version, '', 'GitHub API nicht erreichbar');
}
