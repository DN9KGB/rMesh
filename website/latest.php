<?php
header('Content-Type: application/json');
header('Cache-Control: no-cache, no-store, must-revalidate');

$ctx = stream_context_create(['http' => [
    'header'  => "User-Agent: rMesh-Website\r\n",
    'timeout' => 5,
]]);

$json = @file_get_contents('https://api.github.com/repos/DN9KGB/rMesh/releases/latest', false, $ctx);

if ($json) {
    $data = json_decode($json);
    echo json_encode(['version' => $data->tag_name]);
} else {
    http_response_code(503);
    echo json_encode(['version' => 'unknown']);
}
