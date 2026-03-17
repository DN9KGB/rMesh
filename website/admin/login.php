<?php
/**
 * rMesh Admin – Login
 */
require_once 'auth.php';

// Bereits eingeloggt?
if (!empty($_SESSION['rmesh_admin'])) {
    header('Location: index.php');
    exit;
}

// Hash konfiguriert?
require_once dirname(__DIR__) . '/db_config.php';
$hashSet = !empty(trim($_ENV['ADMIN_PASS_HASH'] ?? ''));

$error = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST' && $hashSet) {
    $pw = (string)($_POST['password'] ?? '');
    if (do_login($pw)) {
        header('Location: index.php');
        exit;
    }
    $error = 'Falsches Passwort.';
}
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>rMesh – Admin Login</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body { font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif; background:#1a1a2e; color:#e0e0e0; display:flex; flex-direction:column; min-height:100vh; }
        header { background:#0f172a; padding:0 24px; box-shadow:0 2px 8px rgba(0,0,0,.4); }
        .header-inner { max-width:100%; display:flex; align-items:center; height:60px; }
        .header-logo { font-size:22px; font-weight:700; letter-spacing:3px; color:#38bdf8; text-decoration:none; }
        .header-logo span { color:#4ecca3; }
        main { flex:1; display:flex; align-items:center; justify-content:center; padding:40px 16px; }
        .login-box { background:#16213e; border:1px solid #0f3460; border-radius:10px; padding:36px 40px; width:100%; max-width:380px; }
        .login-box h2 { color:#4ecca3; font-size:1.1rem; letter-spacing:1px; text-transform:uppercase; margin-bottom:24px; text-align:center; }
        label { display:block; font-size:0.85rem; color:#aaa; margin-bottom:6px; }
        input[type=password] { width:100%; padding:10px 14px; background:#0f172a; border:1px solid #0f3460; border-radius:6px; color:#e0e0e0; font-size:1rem; outline:none; transition:border-color .2s; }
        input[type=password]:focus { border-color:#4ecca3; }
        .btn { width:100%; margin-top:18px; padding:11px; background:#4ecca3; color:#0f172a; font-size:1rem; font-weight:700; border:none; border-radius:6px; cursor:pointer; transition:background .2s; }
        .btn:hover { background:#38bdf8; }
        .error { background:#e94560; color:#fff; border-radius:6px; padding:8px 14px; font-size:0.875rem; margin-bottom:16px; text-align:center; }
        .setup-hint { background:#0f3460; border:1px solid #1e4d8c; border-radius:6px; padding:12px 14px; font-size:0.8rem; color:#aaa; line-height:1.5; }
        .setup-hint code { color:#4ecca3; background:#1a1a2e; padding:2px 6px; border-radius:3px; font-size:0.8rem; }
    </style>
</head>
<body>
<header>
    <div class="header-inner">
        <a href="../index.html" class="header-logo">rMesh <span>Admin</span></a>
    </div>
</header>
<main>
    <div class="login-box">
        <h2>Admin-Zugang</h2>
        <?php if ($error): ?>
            <div class="error"><?= htmlspecialchars($error) ?></div>
        <?php endif; ?>
        <?php if (!$hashSet): ?>
            <div class="setup-hint">
                <strong style="color:#f0a500;">Setup erforderlich:</strong><br><br>
                Füge folgenden Eintrag in <code>.env</code> ein:<br>
                <code>ADMIN_PASS_HASH=&lt;hash&gt;</code><br><br>
                Hash erzeugen:<br>
                <code>php -r "echo password_hash('deinPasswort', PASSWORD_DEFAULT);"</code>
            </div>
        <?php else: ?>
            <form method="post">
                <label for="password">Passwort</label>
                <input type="password" id="password" name="password" autofocus autocomplete="current-password">
                <button type="submit" class="btn">Anmelden</button>
            </form>
        <?php endif; ?>
    </div>
</main>
</body>
</html>
