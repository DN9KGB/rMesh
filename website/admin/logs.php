<?php
/**
 * rMesh Admin – OTA Logs
 */
require_once 'auth.php';
require_auth();

$db = get_db();

$perPage = 50;
$page    = max(1, (int)($_GET['page'] ?? 1));
$search  = trim($_GET['q'] ?? '');
$event   = trim($_GET['event'] ?? '');

$params = [];
$where  = 'WHERE 1=1';
if ($search !== '') {
    $where .= ' AND (call LIKE :q OR device LIKE :q2 OR error LIKE :q3)';
    $params[':q']  = '%' . $search . '%';
    $params[':q2'] = '%' . $search . '%';
    $params[':q3'] = '%' . $search . '%';
}
if ($event !== '') {
    $where .= ' AND event = :event';
    $params[':event'] = $event;
}

// Total count
$countStmt = $db->prepare("SELECT COUNT(*) FROM rmesh_ota_log $where");
$countStmt->execute($params);
$total   = (int)$countStmt->fetchColumn();
$pages   = max(1, (int)ceil($total / $perPage));
$page    = min($page, $pages);
$offset  = ($page - 1) * $perPage;

$params[':limit']  = $perPage;
$params[':offset'] = $offset;

$stmt = $db->prepare("
    SELECT id, `call`, device, event, version_from, version_to, error, timestamp
    FROM rmesh_ota_log
    $where
    ORDER BY timestamp DESC
    LIMIT :limit OFFSET :offset
");
// PDO needs bindValue for LIMIT/OFFSET
$stmt->bindValue(':limit',  $perPage, PDO::PARAM_INT);
$stmt->bindValue(':offset', $offset,  PDO::PARAM_INT);
foreach ($params as $k => $v) {
    if ($k !== ':limit' && $k !== ':offset') $stmt->bindValue($k, $v);
}
$stmt->execute();
$rows = $stmt->fetchAll(PDO::FETCH_ASSOC);

// Distinct events for filter
$events = $db->query("SELECT DISTINCT event FROM rmesh_ota_log ORDER BY event")->fetchAll(PDO::FETCH_COLUMN);

function buildUrl(array $extra = []): string {
    $q = array_merge($_GET, $extra);
    return '?' . http_build_query(array_filter($q, function($v) { return $v !== ''; }));
}
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>rMesh – Admin OTA-Logs</title>
    <?php include '_head_styles.php'; ?>
    <style>
        .err-full { max-width: 400px; font-size:0.75rem; color:#e94560; word-break:break-all; }
    </style>
</head>
<body>
<?php include '_nav.php'; ?>
<div class="page">
    <h1 class="page-title">OTA-Logs <span style="font-size:1rem;color:#888;font-weight:400;">(<?= $total ?> Einträge)</span></h1>

    <form method="get" class="filter-bar">
        <input type="text" name="q" placeholder="Rufzeichen / Gerät / Fehler…" value="<?= htmlspecialchars($search) ?>">
        <label>Ereignis:
            <select name="event" onchange="this.form.submit()">
                <option value="">Alle</option>
                <?php foreach ($events as $ev): ?>
                    <option value="<?= htmlspecialchars($ev) ?>" <?= $event===$ev?'selected':'' ?>><?= htmlspecialchars($ev) ?></option>
                <?php endforeach; ?>
            </select>
        </label>
        <input type="hidden" name="page" value="1">
        <button type="submit" style="padding:7px 14px;background:#0f3460;color:#4ecca3;border:1px solid #4ecca3;border-radius:6px;cursor:pointer;font-size:0.875rem;">Suchen</button>
    </form>

    <div class="table-wrap">
        <table>
            <thead>
                <tr>
                    <th>#</th>
                    <th>Zeit</th>
                    <th>Rufzeichen</th>
                    <th>Gerät</th>
                    <th>Ereignis</th>
                    <th>Version Von → Zu</th>
                    <th>Fehler</th>
                </tr>
            </thead>
            <tbody>
                <?php foreach ($rows as $r): ?>
                <tr>
                    <td class="muted"><?= $r['id'] ?></td>
                    <td class="mono"><?= date('d.m.Y H:i:s', $r['timestamp']) ?></td>
                    <td class="strong mono"><?= htmlspecialchars($r['call']) ?></td>
                    <td class="mono"><?= htmlspecialchars($r['device'] ?? '') ?></td>
                    <td><?php
                        $ev = isset($r['event']) ? $r['event'] : '';
                        if ($ev === 'update_success')     $cls = 'badge-ok';
                        elseif ($ev === 'update_failed')  $cls = 'badge-err';
                        elseif ($ev === 'version_check')  $cls = 'badge-info';
                        else                              $cls = 'badge-neu';
                        echo "<span class=\"badge $cls\">" . htmlspecialchars($ev) . '</span>';
                    ?></td>
                    <td class="mono"><?php
                        $vf = $r['version_from'] ?? '';
                        $vt = $r['version_to'] ?? '';
                        if ($vf && $vt) echo htmlspecialchars($vf) . ' → ' . htmlspecialchars($vt);
                        elseif ($vt) echo htmlspecialchars($vt);
                        elseif ($vf) echo htmlspecialchars($vf);
                        else echo '—';
                    ?></td>
                    <td><div class="err-full"><?= htmlspecialchars($r['error'] ?? '') ?></div></td>
                </tr>
                <?php endforeach; ?>
                <?php if (empty($rows)): ?>
                <tr><td colspan="7" style="text-align:center;color:#555;padding:20px;">Keine Einträge gefunden.</td></tr>
                <?php endif; ?>
            </tbody>
        </table>
    </div>

    <?php if ($pages > 1): ?>
    <div class="pagination">
        <?php if ($page > 1): ?>
            <a href="<?= buildUrl(['page' => $page - 1]) ?>" class="pag-btn">← Zurück</a>
        <?php endif; ?>
        <?php
        $start = max(1, $page - 3);
        $end   = min($pages, $page + 3);
        if ($start > 1) { echo '<a href="' . buildUrl(['page' => 1]) . '" class="pag-btn">1</a>'; if ($start > 2) echo '<span class="pag-info">…</span>'; }
        for ($i = $start; $i <= $end; $i++) {
            $cls = $i === $page ? ' active' : '';
            echo '<a href="' . buildUrl(['page' => $i]) . '" class="pag-btn' . $cls . '">' . $i . '</a>';
        }
        if ($end < $pages) { if ($end < $pages - 1) echo '<span class="pag-info">…</span>'; echo '<a href="' . buildUrl(['page' => $pages]) . '" class="pag-btn">' . $pages . '</a>'; }
        ?>
        <?php if ($page < $pages): ?>
            <a href="<?= buildUrl(['page' => $page + 1]) ?>" class="pag-btn">Weiter →</a>
        <?php endif; ?>
        <span class="pag-info">Seite <?= $page ?> / <?= $pages ?> &nbsp;(<?= $total ?> Einträge)</span>
    </div>
    <?php endif; ?>
</div>
</body>
</html>
