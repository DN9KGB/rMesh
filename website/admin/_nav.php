<?php
$current = basename($_SERVER['PHP_SELF']);
function navActive(string $page, string $current): string {
    return $page === $current ? ' active' : '';
}
?>
<nav>
    <div class="nav-inner">
        <a href="index.php" class="nav-logo">
            <img src="../images/rmesh-logo.svg" alt="rMesh" style="height:38px;vertical-align:middle;">
            <span>Admin</span>
        </a>
        <div class="nav-links">
            <a href="index.php"  class="nav-link<?= navActive('index.php',  $current) ?>">Dashboard</a>
            <a href="nodes.php"  class="nav-link<?= navActive('nodes.php',  $current) ?>">Nodes</a>
            <a href="peers.php"  class="nav-link<?= navActive('peers.php',  $current) ?>">Peers</a>
            <a href="routes.php" class="nav-link<?= navActive('routes.php', $current) ?>">Routen</a>
            <a href="logs.php"   class="nav-link<?= navActive('logs.php',   $current) ?>">OTA-Logs</a>
            <a href="../map.html" target="_blank" class="nav-link">Karte ↗</a>
        </div>
        <a href="logout.php" class="nav-logout">Abmelden</a>
    </div>
</nav>
