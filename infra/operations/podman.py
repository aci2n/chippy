"""Podman installation and rootless prerequisites."""

from pyinfra.api import deploy
from pyinfra.operations import apt, files, server, systemd

from operations._helpers import data, in_group


def _rootless_users():
    users = set(data("podman_users", []))
    for entry in data("users", []):
        if entry.get("podman", False):
            users.add(entry["name"])
    return sorted(users)


@deploy("Install Podman")
def install_podman():
    if not in_group("podman"):
        return

    cfg = data("podman", {})
    packages = cfg.get(
        "packages",
        ["podman", "slirp4netns", "fuse-overlayfs", "uidmap"],
    )

    apt.packages(
        name="Install podman packages",
        packages=packages,
        present=True,
        update=True,
        _sudo=True,
    )


@deploy("Configure subuid/subgid for rootless Podman")
def configure_subids():
    if not in_group("podman"):
        return

    cfg = data("podman", {})
    base_uid = cfg.get("subid_base", 100000)
    count = cfg.get("subid_count", 65536)

    for username in _rootless_users():
        subid_line = f"{username}:{base_uid}:{count}"
        files.line(
            name=f"subuid for {username}",
            path="/etc/subuid",
            line=subid_line,
            replace=f"^{username}:.*",
            _sudo=True,
        )
        files.line(
            name=f"subgid for {username}",
            path="/etc/subgid",
            line=subid_line,
            replace=f"^{username}:.*",
            _sudo=True,
        )


@deploy("Enable rootless Podman auto-update timer")
def enable_podman_auto_update():
    if not in_group("podman"):
        return

    cfg = data("podman", {})
    if not cfg.get("auto_update", True):
        return

    for username in _rootless_users():
        systemd.service(
            name=f"Enable podman auto-update for {username}",
            service="podman-auto-update.timer",
            user_mode=True,
            user_name=username,
            enabled=True,
            running=True,
            daemon_reload=True,
            _sudo=True,
        )
