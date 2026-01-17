<?php


// Parameter vom ESP32 abgreifen
$type = $_GET['type'];    // 'firmware' oder 'fs'
$version = $_GET['version']; // Die Version, die der ESP aktuell hat


switch ($version) {
	case "V1.0.0-a":
		if ($type == "firmware") { serveFile("firmware_V1.0.1-a.bin"); } 
		if ($type == "littlefs") { serveFile("littlefs_V1.0.1-a.bin"); }
		break;
	default:
		header($_SERVER["SERVER_PROTOCOL"].' 304 Not Modified');
}


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