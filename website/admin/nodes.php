<?php
/**
 * rMesh Admin – Nodes
 */
require_once 'auth.php';
require_auth();

$db = get_db();

require_once dirname(__DIR__) . '/ota_log_helper.php';
_ensureLastVersionCheckColumn($db);

$maxAge = max(600, min(2592000, (int)($_GET['max_age'] ?? 86400)));
$cutoff = time() - $maxAge;
$search = trim($_GET['q'] ?? '');

$params = [':cutoff' => $cutoff];
$where  = 'WHERE n.last_seen >= :cutoff';
if ($search !== '') {
    $where .= ' AND n.call LIKE :q';
    $params[':q'] = '%' . $search . '%';
}

$nodes = $db->prepare("
    SELECT n.`call`, n.position, n.last_seen, n.last_version_check,
           COUNT(DISTINCT p.peer_call) AS peer_count,
           COUNT(DISTINCT r.src_call)  AS route_count,
           ota.device, ota.version_to AS firmware
    FROM rmesh_nodes n
    LEFT JOIN rmesh_peers  p   ON p.reporter_call = n.`call` AND p.last_seen >= :cutoff AND p.available = 1
    LEFT JOIN rmesh_routes r   ON r.reporter_call = n.`call` AND r.last_seen >= :cutoff
    LEFT JOIN rmesh_ota_log ota ON ota.id = (
        SELECT id FROM rmesh_ota_log
        WHERE `call` = n.`call` AND version_to IS NOT NULL AND version_to != ''
        ORDER BY timestamp DESC LIMIT 1
    )
    $where
    GROUP BY n.`call`, n.position, n.last_seen, ota.device, ota.version_to
    ORDER BY n.last_seen DESC
");
$nodes->execute($params);
$rows = $nodes->fetchAll(PDO::FETCH_ASSOC);
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>rMesh – Admin Nodes</title>
    <?php include '_head_styles.php'; ?>
</head>
<body>
<?php include '_nav.php'; ?>
<div class="page">
    <h1 class="page-title">Nodes <span style="font-size:1rem;color:#888;font-weight:400;">(<?= count($rows) ?>)</span></h1>

    <form method="get" class="filter-bar">
        <input type="text" name="q" placeholder="Rufzeichen suchen…" value="<?= htmlspecialchars($search) ?>">
        <label>Zeitraum:
            <select name="max_age" onchange="this.form.submit()">
                <option value="3600"   <?= $maxAge===3600   ?'selected':'' ?>>1 Stunde</option>
                <option value="86400"  <?= $maxAge===86400  ?'selected':'' ?>>24 Stunden</option>
                <option value="604800" <?= $maxAge===604800 ?'selected':'' ?>>7 Tage</option>
                <option value="2592000"<?= $maxAge===2592000?'selected':'' ?>>30 Tage</option>
            </select>
        </label>
        <button type="submit" style="padding:7px 14px;background:#0f3460;color:#4ecca3;border:1px solid #4ecca3;border-radius:6px;cursor:pointer;font-size:0.875rem;">Suchen</button>
    </form>

    <div class="table-wrap">
        <table>
            <thead>
                <tr>
                    <th>Rufzeichen</th>
                    <th>Gerät</th>
                    <th>Firmware</th>
                    <th>Position</th>
                    <th>Zuletzt gesehen</th>
                    <th>Alter</th>
                    <th>Version Check</th>
                    <th>Peers</th>
                    <th>Routen</th>
                </tr>
            </thead>
            <tbody>
                <?php foreach ($rows as $r): ?>
                <tr>
                    <td class="strong mono"><?= htmlspecialchars($r['call']) ?></td>
                    <td class="mono"><?= htmlspecialchars($r['device'] ?? '—') ?></td>
                    <td class="mono"><?= $r['firmware'] ? '<span class="badge badge-info">' . htmlspecialchars($r['firmware']) . '</span>' : '<span class="muted">—</span>' ?></td>
                    <td class="mono"><?= htmlspecialchars($r['position'] ?? '—') ?></td>
                    <td class="mono"><?= date('d.m.Y H:i:s', $r['last_seen']) ?></td>
                    <td class="muted"><?= timeAgo($r['last_seen']) ?></td>
                    <td class="muted"><?= $r['last_version_check'] ? timeAgo((int)$r['last_version_check']) : '<span class="muted">—</span>' ?></td>
                    <td><?= $r['peer_count'] ?></td>
                    <td><?= $r['route_count'] ?></td>
                </tr>
                <?php endforeach; ?>
                <?php if (empty($rows)): ?>
                <tr><td colspan="9" style="text-align:center;color:#555;padding:20px;">Keine Nodes gefunden.</td></tr>
                <?php endif; ?>
            </tbody>
        </table>
    </div>
</div>
<?php
function timeAgo(int $ts): string {
    $d = time() - $ts;
    if ($d < 60)   return $d . 's';
    if ($d < 3600) return floor($d/60) . ' min';
    if ($d < 86400) return floor($d/3600) . ' h';
    return floor($d/86400) . ' Tage';
}
?>
</body>
</html>
