import subprocess
import os
Import("env")

project_dir = env.subst("$PROJECT_DIR")

try:
    version = subprocess.check_output(
        ["git", "describe", "--tags", "--always"],
        stderr=subprocess.DEVNULL,
        cwd=project_dir,
        shell=(os.name == "nt")
    ).decode().strip()
except Exception as e:
    print(f"get_version.py: git describe failed ({e}), using 'unknown'")
    version = "unknown"

print(f"get_version.py: VERSION = {version}")

version_header = os.path.join(project_dir, "src", "version.h")
with open(version_header, "w") as f:
    f.write(f'#pragma once\n#define VERSION "{version}"\n')
