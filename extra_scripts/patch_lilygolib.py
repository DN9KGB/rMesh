"""
Patches LilyGoLib to compile without the ST25R3916 RFAL library.
The library unconditionally references RfalNfcClass/RfalRfST25R3916Class even
when USING_ST25R3916 is not defined. This script wraps those references in the
appropriate #ifdef guard.

Idempotent: safe to run multiple times.
"""
Import("env")
import os

if env["PIOENV"] != "LILYGO_T-LoraPager":
    Return()

src_dir = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "LilyGoLib", "src")

def patch_file(path, replacements):
    if not os.path.exists(path):
        print(f"[patch_lilygolib] WARNING: {path} not found, skipping")
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    changed = False
    for old, new in replacements:
        if old in content:
            content = content.replace(old, new)
            changed = True
    if changed:
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"[patch_lilygolib] Patched {os.path.basename(path)}")

# --- LilyGo_LoRa_Pager.h ---
patch_file(
    os.path.join(src_dir, "LilyGo_LoRa_Pager.h"),
    [
        (
            "extern RfalNfcClass NFCReader;",
            "#ifdef USING_ST25R3916\nextern RfalNfcClass NFCReader;\n#endif",
        ),
    ],
)

# --- LilyGo_LoRa_Pager.cpp ---
patch_file(
    os.path.join(src_dir, "LilyGo_LoRa_Pager.cpp"),
    [
        # Global NFC object instantiation
        (
            "RfalRfST25R3916Class nfc_hw(&SPI, NFC_CS, NFC_INT);\nRfalNfcClass NFCReader(&nfc_hw);",
            "#ifdef USING_ST25R3916\nRfalRfST25R3916Class nfc_hw(&SPI, NFC_CS, NFC_INT);\nRfalNfcClass NFCReader(&nfc_hw);\n#endif",
        ),
        # initNFC() body
        (
            '    bool res = false;\n    log_d("Init NFC");\n    res = NFCReader.rfalNfcInitialize() == ST_ERR_NONE;\n    if (!res) {\n        log_e("Failed to find NFC Reader");\n    } else {\n        log_d("Initializing NFC Reader succeeded");\n        devices_probe |= HW_NFC_ONLINE;\n        // Turn off NFC power\n        powerControl(POWER_NFC, false);\n    }\n    return res;',
            '    bool res = false;\n#ifdef USING_ST25R3916\n    log_d("Init NFC");\n    res = NFCReader.rfalNfcInitialize() == ST_ERR_NONE;\n    if (!res) {\n        log_e("Failed to find NFC Reader");\n    } else {\n        log_d("Initializing NFC Reader succeeded");\n        devices_probe |= HW_NFC_ONLINE;\n        // Turn off NFC power\n        powerControl(POWER_NFC, false);\n    }\n#endif\n    return res;',
        ),
    ],
)
