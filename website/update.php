<?php
$file = $_GET['file'] ?? '';
if (!preg_match('/^[a-zA-Z0-9_\-\.]+$/', $file)) {
    http_response_code(400);
    exit;
}
header("Location: https://github.com/DN9KGB/rMesh/releases/latest/download/" . $file, true, 302);
