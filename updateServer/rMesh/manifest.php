<?php
$device = isset($_GET['device']) ? basename($_GET['device']) : '';

if (empty($device)) {
    http_response_code(400);
    header('Content-Type: application/json');
    echo json_encode(['error' => 'Missing device parameter']);
    exit;
}

$baseDir   = __DIR__;
$deviceDir = "$baseDir/$device";

if (!is_dir($deviceDir)) {
    http_response_code(404);
    header('Content-Type: application/json');
    echo json_encode(['error' => 'Device not found']);
    exit;
}

// Chip family and LittleFS offset per device
$deviceConfig = [
    'ESP32_Without_LoRa'            => ['chip' => 'ESP32',    'lfs_offset' => 0x290000],
    'LILYGO_T3_LoRa32_V1_6_1'      => ['chip' => 'ESP32',    'lfs_offset' => 0x290000],
    'LILYGO_T-Beam'                 => ['chip' => 'ESP32',    'lfs_offset' => 0x290000],
    'LILYGO_T-LoraPager'            => ['chip' => 'ESP32-S3', 'lfs_offset' => 0x610000],
    'HELTEC_Wireless_Stick_Lite_V3' => ['chip' => 'ESP32-S3', 'lfs_offset' => 0x290000],
    'HELTEC_WiFi_LoRa_32_V3'        => ['chip' => 'ESP32-S3', 'lfs_offset' => 0x290000],
    'HELTEC_WiFi_LoRa_32_V4'        => ['chip' => 'ESP32-S3', 'lfs_offset' => 0x290000],
];

$chipFamily      = $deviceConfig[$device]['chip']       ?? 'ESP32';
$lfsOffset       = $deviceConfig[$device]['lfs_offset'] ?? 0x290000;
$isS3            = ($chipFamily === 'ESP32-S3');
$bootloaderOffset = $isS3 ? 0x0000 : 0x1000;

$parts = [];

if (file_exists("$deviceDir/bootloader.bin")) {
    $parts[] = ['path' => "$device/bootloader.bin", 'offset' => $bootloaderOffset];
}
if (file_exists("$deviceDir/partitions.bin")) {
    $parts[] = ['path' => "$device/partitions.bin", 'offset' => 0x8000];
}
if (file_exists("$deviceDir/firmware.bin")) {
    $parts[] = ['path' => "$device/firmware.bin", 'offset' => 0x10000];
}
if (file_exists("$deviceDir/littlefs.bin")) {
    $parts[] = ['path' => "$device/littlefs.bin", 'offset' => $lfsOffset];
}

if (empty($parts)) {
    http_response_code(404);
    header('Content-Type: application/json');
    echo json_encode(['error' => 'No firmware files found for this device']);
    exit;
}

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

echo json_encode([
    'name'    => "rMesh – $device",
    'version' => '1.0',
    'builds'  => [
        [
            'chipFamily' => $chipFamily,
            'parts'      => $parts,
        ]
    ]
], JSON_PRETTY_PRINT);
