# Where the msedge.dll under analysis lives: the single home of that path for the
# tools/re scripts. Override with the MSEDGE_DLL environment variable (e.g. to point
# at a copy of an older build); otherwise the highest installed version wins.
import glob
import os

APPLICATION = r"C:\Program Files (x86)\Microsoft\Edge\Application"


def _version_key(path):
    folder = os.path.basename(os.path.dirname(path))
    return tuple(int(part) for part in folder.split(".") if part.isdigit())


def installed_dll():
    override = os.environ.get("MSEDGE_DLL")
    if override:
        return override
    candidates = glob.glob(os.path.join(APPLICATION, "*", "msedge.dll"))
    if not candidates:
        raise FileNotFoundError(f"no msedge.dll under {APPLICATION}")
    return max(candidates, key=_version_key)
