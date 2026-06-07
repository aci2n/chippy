"""Local users, groups, and SSH authorized keys."""

from pyinfra.api import deploy
from pyinfra.operations import server

from operations._helpers import data, in_group, string_put


@deploy("Configure local users")
def configure_users():
    if not in_group("base"):
        return

    users = data("users", [])
    if not users:
        return

    for entry in users:
        username = entry["name"]
        groups = entry.get("groups", [])
        shell = entry.get("shell", "/bin/bash")
        home = entry.get("home")
        keys = [k for k in entry.get("authorized_keys", []) if k and not k.strip().startswith("#")]

        server.user(
            name=f"Ensure user {username}",
            user=username,
            groups=groups,
            shell=shell,
            home=home,
            public_keys=keys or None,
            present=True,
            _sudo=True,
        )

        if entry.get("passwordless_sudo"):
            string_put(
                name=f"Passwordless sudo for {username}",
                dest=f"/etc/sudoers.d/90-{username}",
                contents=f"{username} ALL=(ALL) NOPASSWD:ALL\n",
                mode="0440",
                _sudo=True,
            )


@deploy("Enable linger for container users")
def enable_linger_for_users():
    if not in_group("podman"):
        return

    linger_users = set(data("podman_linger_users", []))
    for entry in data("users", []):
        if entry.get("podman", False):
            linger_users.add(entry["name"])

    for username in sorted(linger_users):
        server.shell(
            name=f"Enable systemd linger for {username}",
            commands=[f"loginctl enable-linger {username}"],
            _sudo=True,
        )
