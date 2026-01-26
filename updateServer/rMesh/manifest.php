<?php
header('Content-Type: application/json');

$device = $_GET['device'] ?? null;

if (!$device) {
    echo json_encode(["error" => "Missing device parameter"]);
    exit;
}

$basePath = __DIR__ . "/$device";

$firmware = "$basePath/firmware.bin";
$littlefs = "$basePath/littlefs.bin";

if (!file_exists($firmware) || !file_exists($littlefs)) {
    echo json_encode(["error" => "Firmware files missing"]);
    exit;
}

$manifest = '{
  "name": "'.$device.'",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "'.$device.'/bootloader.bin", "offset": 4096 },
        { "path": "'.$device.'/partitions.bin", "offset": 32768 },
        { "path": "'.$device.'/firmware.bin", "offset": 65536 },
        { "path": "'.$device.'/littlefs.bin", "offset": 2686976 }
      ]
    }
  ],
  "baudrate": 921600
}';

echo $manifest;

//echo json_encode($manifest, JSON_PRETTY_PRINT);
