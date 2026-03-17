<?php
/**
 * rMesh Admin – Auth-Hilfsfunktionen
 */

session_start();

function require_auth(): void {
    if (empty($_SESSION['rmesh_admin'])) {
        header('Location: login.php');
        exit;
    }
}

function do_login(string $password): bool {
    require_once dirname(__DIR__) . '/db_config.php';
    $hash = isset($_ENV['ADMIN_PASS_HASH']) ? trim($_ENV['ADMIN_PASS_HASH']) : '';
    if ($hash === '' || !password_verify($password, $hash)) {
        return false;
    }
    session_regenerate_id(true);
    $_SESSION['rmesh_admin'] = true;
    return true;
}

function do_logout(): void {
    $_SESSION = [];
    session_destroy();
}

function get_db(): PDO {
    require_once dirname(__DIR__) . '/db_config.php';
    $dsn = 'mysql:host=' . DB_HOST . ';dbname=' . DB_NAME . ';charset=' . DB_CHARSET;
    return new PDO($dsn, DB_USER, DB_PASS, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
}
