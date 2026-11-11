"""Minimum framework readiness; this does not establish app rendering."""

import re


def framework_ready(boot_completed, hwsm_ready, services):
    """Require successful ADB probes, boot completion, and exact Binder names."""
    if any(result.returncode != 0 for result in
           (boot_completed, hwsm_ready, services)):
        return False
    if boot_completed.stdout.strip() != "1":
        return False
    if hwsm_ready.stdout.strip() != "true":
        return False
    names = set(re.findall(r"^\s*\d+\s+(\S+):\s*\[", services.stdout, re.M))
    return {"activity", "window", "package", "SurfaceFlinger", "installd"} <= names
