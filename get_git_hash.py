Import("env")
import subprocess

def get_git_hash():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short=8", "HEAD"],
            stderr=subprocess.DEVNULL
        ).strip().decode("utf-8")
    except Exception:
        return "unknown"

def get_git_dirty():
    try:
        status = subprocess.check_output(
            ["git", "status", "--porcelain"],
            stderr=subprocess.DEVNULL
        ).strip()
        return "-dirty" if status else ""
    except Exception:
        return ""

git_hash = get_git_hash() + get_git_dirty()

env.Append(BUILD_FLAGS=[f'-DGIT_HASH=\\"{git_hash}\\"'])
print(f"[get_git_hash] Building with GIT_HASH={git_hash}")
