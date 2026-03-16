import subprocess
Import("env")

try:
    version = subprocess.check_output(
        ["git", "describe", "--tags", "--always"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
except Exception:
    version = "unknown"

env.Append(CPPDEFINES=[("VERSION", f'\\"{version}\\"')])
