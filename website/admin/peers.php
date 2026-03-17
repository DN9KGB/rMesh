<?php
/**
 * rMesh Admin – Peers
 */
require_once 'auth.php';
require_auth();

$db = get_db();

$maxAge = max(600, min(2592000, (int)($_GET['max_age'] ?? 86400)));
$cutoff = time() - $maxAge;
$search = trim($_GET['q'] ?? '');
$port   = $_GET['port'] ?? '';

$params = [':cutoff' => $cutoff];
$where  = 'WHERE last_seen >= :cutoff AND available = 1';
if ($search !== '') {
    $where .= ' AND (reporter_call LIKE :q OR peer_call LIKE :q2)';
    $params[':q']  = '%' . $search . '%';
    $params[':q2'] = '%' . $search . '%';
}
if ($port === '0' || $port === '1') {
    $where .= ' AND port = :port';
    $params[':port'] = (int)$port;
}

$stmt = $db->prepare("
    SELECT reporter_call, peer_call, rssi, snr, port, available, last_seen
    FROM rmesh_peers
    $where
    ORDER BY last_seen DESC
    LIMIT 2000
");
$stmt->execute($params);
$rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>rMesh – Admin Peers</title>
    <?php include '_head_styles.php'; ?>
</head>
<body>
<?php include '_nav.php'; ?>
<div class="page">
    <h1 class="page-title">Peers <span style="font-size:1rem;color:#888;font-weight:400;">(<?= count($rows) ?>)</span></h1>

    <form method="get" class="filter-bar">
        <input type="text" name="q" placeholder="Rufzeichen suchen…" value="<?= htmlspecialchars($search) ?>">
        <label>Typ:
            <select name="port" onchange="this.form.submit()">
                <option value=""  <?= $port===''  ?'selected':'' ?>>Alle</option>
                <option value="0" <?= $port==='0' ?'selected':'' ?>>LoRa</option>
                <option value="1" <?= $port==='1' ?'selected':'' ?>>WiFi/UDP</option>
            </select>
        </label>
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
                    <th>Berichtende Node</th>
                    <th>Peer</th>
                    <th>RSSI (dBm)</th>
                    <th>SNR (dB)</th>
                    <th>Typ</th>
                    <th>Zuletzt gesehen</th>
                    <th>Alter</th>
                </tr>
            </thead>
            <tbody>
                <?php foreach ($rows as $r): ?>
                <tr>
                    <td class="strong mono"><?= htmlspecialchars($r['reporter_call']) ?></td>
                    <td class="mono"><?= htmlspecialchars($r['peer_call']) ?></td>
                    <td class="mono <?= (float)$r['rssi'] >= -70 ? 'badge-ok' : ((float)$r['rssi'] >= -90 ? 'badge-neu' : 'badge-err') ?>"
                        style="padding:8px 12px;"><?= number_format((float)$r['rssi'], 1) ?></td>
                    <td class="mono"><?= number_format((float)$r['snr'], 1) ?></td>
                    <td><?= (int)$r['port'] === 0 ? '<span class="pill-lora">LoRa</span>' : '<span class="pill-wifi">WiFi/UDP</span>' ?></td>
                    <td class="mono"><?= date('d.m.Y H:i:s', $r['last_seen']) ?></td>
                    <td class="muted"><?= timeAgo($r['last_seen']) ?></td>
                </tr>
                <?php endforeach; ?>
                <?php if (empty($rows)): ?>
                <tr><td colspan="7" style="text-align:center;color:#555;padding:20px;">Keine Peers gefunden.</td></tr>
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
