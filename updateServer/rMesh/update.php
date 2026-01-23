<?php


// Parameter vom ESP32 abgreifen

$h  = mb_strtolower($_GET['h'], 'UTF-8');
$v   = mb_strtolower($_GET['v'], 'UTF-8');
$t = mb_strtolower($_GET['t'], 'UTF-8');

if ($h == "lilygo_t3_lora32_v1_6_1" && $v == "v1.0.0-a" && $t == "firmware") { serveFile("firmware_V1.0.1-a.bin"); } 
if ($h == "lilygo_t3_lora32_v1_6_1" && $v == "v1.0.0-a" && $t == "littlefs") { serveFile("littlefs_V1.0.1-a.bin"); } 



function serveFile($path) {
    if (file_exists($path)) {
        header('Content-Type: application/octet-stream');
        header('Content-Disposition: attachment; filename="'.basename($path).'"');
        header('Content-Length: ' . filesize($path));
        readfile($path);
        exit;
    } else {
        header($_SERVER["SERVER_PROTOCOL"].' 404 Not Found');
    }
}
?>