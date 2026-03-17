<?php
/**
 * rMesh Admin – Dashboard
 */
require_once 'auth.php';
require_auth();

$db = get_db();
$since24h = time() - 86400;

$counts = [];

$counts['nodes']  = (int)$db->query("SELECT COUNT(*) FROM rmesh_nodes WHERE last_seen >= $since24h")->fetchColumn();
$counts['peers']  = (int)$db->query("SELECT COUNT(*) FROM rmesh_peers WHERE last_seen >= $since24h AND available=1")->fetchColumn();
$counts['routes'] = (int)$db->query("SELECT COUNT(*) FROM rmesh_routes WHERE last_seen >= $since24h")->fetchColumn();
$counts['logs']   = (int)$db->query("SELECT COUNT(*) FROM rmesh_ota_log")->fetchColumn();

$recent = $db->query("
    SELECT `call`, device, event, version_from, version_to, error, timestamp
    FROM rmesh_ota_log
    ORDER BY timestamp DESC
    LIMIT 15
")->fetchAll(PDO::FETCH_ASSOC);
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>rMesh – Admin Dashboard</title>
    <?php include '_head_styles.php'; ?>
</head>
<body>
<?php include '_nav.php'; ?>
<div class="page">
    <h1 class="page-title">Dashboard</h1>

    <div class="stat-grid">
        <a href="nodes.php" class="stat-card">
            <div class="stat-value"><?= $counts['nodes'] ?></div>
            <div class="stat-label">Nodes (24h)</div>
        </a>
        <a href="peers.php" class="stat-card">
            <div class="stat-value"><?= $counts['peers'] ?></div>
            <div class="stat-label">Peers (24h)</div>
        </a>
        <a href="routes.php" class="stat-card">
            <div class="stat-value"><?= $counts['routes'] ?></div>
            <div class="stat-label">Routen (24h)</div>
        </a>
        <a href="logs.php" class="stat-card">
            <div class="stat-value"><?= $counts['logs'] ?></div>
            <div class="stat-label">OTA-Log-Einträge</div>
        </a>
    </div>

    <h2 class="section-title">Letzte OTA-Ereignisse</h2>
    <div class="table-wrap">
        <table>
            <thead>
                <tr>
                    <th>Zeit</th>
                    <th>Rufzeichen</th>
                    <th>Gerät</th>
                    <th>Ereignis</th>
                    <th>Version</th>
                    <th>Fehler</th>
                </tr>
            </thead>
            <tbody>
                <?php foreach ($recent as $r): ?>
                <tr>
                    <td class="mono"><?= date('d.m.y H:i', $r['timestamp']) ?></td>
                    <td class="strong"><?= htmlspecialchars($r['call']) ?></td>
                    <td><?= htmlspecialchars($r['device'] ?? '') ?></td>
                    <td><?php
                        $ev = isset($r['event']) ? $r['event'] : '';
                        if ($ev === 'update_success')     $cls = 'badge-ok';
                        elseif ($ev === 'update_failed')  $cls = 'badge-err';
                        elseif ($ev === 'version_check')  $cls = 'badge-info';
                        else                              $cls = 'badge-neu';
                        echo "<span class=\"badge $cls\">" . htmlspecialchars($ev) . '</span>';
                    ?></td>
                    <td class="mono">
                        <?php
                        $vf = $r['version_from'] ?? '';
                        $vt = $r['version_to']   ?? '';
                        if ($vf && $vt) echo htmlspecialchars($vf) . ' → ' . htmlspecialchars($vt);
                        elseif ($vt) echo htmlspecialchars($vt);
                        elseif ($vf) echo htmlspecialchars($vf);
                        ?>
                    </td>
                    <td class="err-text"><?= htmlspecialchars(mb_substr($r['error'] ?? '', 0, 60)) ?></td>
                </tr>
                <?php endforeach; ?>
            </tbody>
        </table>
    </div>
    <p style="margin-top:10px;"><a href="logs.php" class="link-more">Alle Logs anzeigen →</a></p>
</div>
</body>
</html>
