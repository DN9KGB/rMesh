<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <title>rMesh Installer</title>

    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>

    <style>

.global-changelog {
    max-width: 1000px;
    margin: 40px auto;
    background: white;
    padding: 25px;
    border-radius: 12px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.1);
}

.global-changelog h3 {
    margin-top: 0;
    color: #333;
    border-bottom: 2px solid #0078ff;
    padding-bottom: 10px;
    font-size: 20px;
}

.changelog-content {
    background: #fafafa;
    border: 1px solid #eee;
    padding: 15px;
    border-radius: 8px;
    margin-top: 15px;
}

.changelog-content pre {
    white-space: pre-wrap;
    word-wrap: break-word;
    margin: 0;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 14px;
    color: #444;
    line-height: 1.5;
}

        body {
            font-family: Arial, sans-serif;
            background: #f4f6f8;
            margin: 0;
            padding: 20px;
        }

        h2 {
            text-align: center;
            margin-bottom: 30px;
        }

		.device-grid {
			display: grid;
			grid-template-columns: repeat(3, 1fr);
			gap: 20px;
			padding: 10px;
		}

        .device-card {
            background: white;
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.1);
            text-align: center;
        }

        .device-card img {
            width: 250px;
            height: 180px;
            object-fit: contain;
            border-radius: 8px;
            background: #fafafa;
            border: 1px solid #ddd;
            margin-bottom: 15px;
        }

        .device-name {
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 12px;
        }

        /* Split-Button */
        .btn-group {
            display: inline-flex;
            position: relative;
        }

        .btn-group button[slot="activate"] {
            border-radius: 6px 0 0 6px;
            background: #0078ff;
            color: white;
            border: none;
            padding: 10px 18px;
            cursor: pointer;
            font-size: 15px;
        }

        .btn-group button[slot="activate"]:hover {
            background: #005fcc;
        }

        .btn-arrow {
            background: #005fcc;
            color: white;
            border: none;
            border-left: 1px solid rgba(255,255,255,0.3);
            padding: 10px 12px;
            border-radius: 0 6px 6px 0;
            cursor: pointer;
            font-size: 13px;
            line-height: 1;
        }

        .btn-arrow:hover {
            background: #004baa;
        }

        .dropdown-menu {
            display: none;
            position: absolute;
            top: calc(100% + 4px);
            right: 0;
            background: white;
            border: 1px solid #ddd;
            border-radius: 6px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
            z-index: 100;
            min-width: 190px;
            text-align: left;
        }

        .dropdown-menu a {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 10px 15px;
            text-decoration: none;
            color: #333;
            font-size: 14px;
        }

        .dropdown-menu a:hover {
            background: #f4f6f8;
            border-radius: 6px;
        }

        .btn-group.open .dropdown-menu {
            display: block;
        }

        /* Kein Split-Button wenn kein ZIP */
        button[slot="activate"].standalone {
            border-radius: 6px;
            background: #0078ff;
            color: white;
            border: none;
            padding: 10px 18px;
            cursor: pointer;
            font-size: 15px;
        }

        button[slot="activate"].standalone:hover {
            background: #005fcc;
        }
    </style>
</head>
<body>

<h2>rMesh Installer</h2>

<div class="device-grid">

<?php
$baseDir = __DIR__;
$devices = array_filter(glob($baseDir . '/*'), 'is_dir');

foreach ($devices as $devicePath) {
    $device = basename($devicePath);

    if (!file_exists("$devicePath/firmware.bin") && !file_exists("$devicePath/littlefs.bin")) {
        continue;
    }

    $imagePath = "$devicePath/image.webp";
    $imageUrl = file_exists($imagePath) ? "$device/image.webp" : "https://via.placeholder.com/250x180?text=No+Image";

    $hasZip = file_exists("$devicePath/$device.zip");

    echo "<div class='device-card'>";
    echo "<img src='$imageUrl' alt='Board Image'>";
    echo "<div class='device-name'>$device</div>";

    if ($hasZip) {
        echo "
        <div class='btn-group'>
            <esp-web-install-button manifest=\"$device/manifest.php\">
                <button slot='activate'>Firmware installieren</button>
                <span slot='unsupported'>Browser nicht unterstützt.</span>
            </esp-web-install-button>
            <button class='btn-arrow' onclick='toggleDropdown(this)'>&#9660;</button>
            <div class='dropdown-menu'>
                <a href='$device/$device.zip' download>Download Firmware</a>
            </div>
        </div>
        ";
    } else {
        echo "
        <esp-web-install-button manifest=\"$device/manifest.php\">
            <button slot='activate' class='standalone'>Firmware installieren</button>
            <span slot='unsupported'>Browser nicht unterstützt.</span>
        </esp-web-install-button>
        ";
    }

    echo "</div>";
}
?>

</div>


</div> <?php
$globalChangelog = $baseDir . '/changelog.txt';
if (file_exists($globalChangelog)):
    $content = htmlspecialchars(file_get_contents($globalChangelog));
?>
    <div class="global-changelog">
        <h3>Release Notes / Changelog</h3>
        <div class="changelog-content">
            <pre><?php echo $content; ?></pre>
        </div>
    </div>
<?php endif; ?>

<script>
function toggleDropdown(btn) {
    const group = btn.closest('.btn-group');
    group.classList.toggle('open');
}

document.addEventListener('click', function(e) {
    if (!e.target.closest('.btn-group')) {
        document.querySelectorAll('.btn-group.open').forEach(g => g.classList.remove('open'));
    }
});
</script>

</body>
</html>
