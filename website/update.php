<?php
$file = $_GET['file'] ?? '';
$call = isset($_GET['call']) ? strtoupper(substr(preg_replace('/[^A-Z0-9\/\-]/', '', strtoupper($_GET['call'])), 0, 16)) : '';

if (!preg_match('/^[a-zA-Z0-9_\-\.]+$/', $file)) {
    http_response_code(400);
    exit;
}

require_once __DIR__ . '/ota_log_helper.php';
logOtaEvent($call, 'update_start', '', '', $file);

header("Location: https://github.com/DN9KGB/rMesh/releases/latest/download/" . $file, true, 302);
