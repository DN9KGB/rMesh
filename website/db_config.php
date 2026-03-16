<?php
/**
 * rMesh Datenbank-Konfiguration
 * Liest Zugangsdaten aus website/.env – diese Datei ist git-sicher.
 */

$envFile = __DIR__ . '/.env';
if (!file_exists($envFile)) {
    die('Fehler: .env Datei nicht gefunden. Siehe .env.example');
}

foreach (file($envFile, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
    if ($line[0] === '#' || strpos($line, '=') === false) continue;
    list($key, $value) = explode('=', $line, 2);
    $_ENV[trim($key)] = trim($value);
}

define('DB_HOST',    isset($_ENV['DB_HOST'])    ? $_ENV['DB_HOST']    : 'localhost');
define('DB_NAME',    isset($_ENV['DB_NAME'])    ? $_ENV['DB_NAME']    : '');
define('DB_USER',    isset($_ENV['DB_USER'])    ? $_ENV['DB_USER']    : '');
define('DB_PASS',    isset($_ENV['DB_PASS'])    ? $_ENV['DB_PASS']    : '');
define('DB_CHARSET', 'utf8mb4');
