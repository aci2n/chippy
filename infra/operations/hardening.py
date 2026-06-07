"""Common Debian hardening: sysctl, unattended upgrades, fail2ban."""

from pyinfra.api import deploy
from pyinfra.operations import apt, files, server, systemd

from operations._helpers import data, in_group, string_put


@deploy("Apply sysctl hardening")
def apply_sysctl():
    if not in_group("hardening"):
        return

    settings = data("sysctl", {})
    if not settings:
        return

    lines = ["# Managed by pyinfra\n"]
    for key, value in sorted(settings.items()):
        lines.append(f"{key} = {value}\n")

    string_put(
        name="Deploy sysctl hardening",
        dest="/etc/sysctl.d/99-pyinfra-hardening.conf",
        contents="".join(lines),
        mode="0644",
        _sudo=True,
    )

    server.shell(
        name="Apply sysctl settings",
        commands=["sysctl --system"],
        _sudo=True,
    )


@deploy("Configure unattended upgrades")
def configure_unattended_upgrades():
    if not in_group("hardening"):
        return

    cfg = data("unattended_upgrades", {})
    if not cfg.get("enabled", True):
        return

    apt.packages(
        name="Install unattended-upgrades",
        packages=["unattended-upgrades", "apt-listchanges"],
        present=True,
        _sudo=True,
    )

    string_put(
        name="Enable automatic security updates",
        dest="/etc/apt/apt.conf.d/20auto-upgrades",
        contents=(
            'APT::Periodic::Update-Package-Lists "1";\n'
            'APT::Periodic::Unattended-Upgrade "1";\n'
        ),
        mode="0644",
        _sudo=True,
    )

    origins = cfg.get(
        "origins",
        [
            "${distro_id}:${distro_codename}-security",
            "origin=Debian,codename=${distro_codename}-security,label=Debian-Security",
        ],
    )
    origin_lines = "\n".join(f'        "{origin}";' for origin in origins)
    string_put(
        name="Configure unattended upgrade origins",
        dest="/etc/apt/apt.conf.d/50unattended-upgrades",
        contents=(
            'Unattended-Upgrade::Automatic-Reboot "false";\n'
            "Unattended-Upgrade::Origins-Pattern {\n"
            f"{origin_lines}\n"
            "};\n"
        ),
        mode="0644",
        _sudo=True,
    )


@deploy("Configure fail2ban")
def configure_fail2ban():
    if not in_group("hardening"):
        return

    cfg = data("fail2ban", {})
    if not cfg.get("enabled", False):
        return

    apt.packages(
        name="Install fail2ban",
        packages=["fail2ban"],
        present=True,
        _sudo=True,
    )

    jail_lines = [
        "[DEFAULT]\n",
        f"bantime = {cfg.get('bantime', 3600)}\n",
        f"findtime = {cfg.get('findtime', 600)}\n",
        f"maxretry = {cfg.get('maxretry', 5)}\n",
        "\n[sshd]\n",
        "enabled = true\n",
    ]
    string_put(
        name="Deploy fail2ban jail config",
        dest="/etc/fail2ban/jail.local",
        contents="".join(jail_lines),
        mode="0644",
        _sudo=True,
    )

    systemd.service(
        name="Enable fail2ban",
        service="fail2ban",
        enabled=True,
        running=True,
        _sudo=True,
    )


@deploy("Configure host firewall")
def configure_firewall():
    if not in_group("hardening"):
        return

    cfg = data("firewall", {})
    if not cfg.get("enabled", False):
        return

    apt.packages(
        name="Install ufw",
        packages=["ufw"],
        present=True,
        _sudo=True,
    )

    default_in = cfg.get("default_incoming", "deny")
    default_out = cfg.get("default_outgoing", "allow")
    commands = [
        f"ufw default {default_in} incoming",
        f"ufw default {default_out} outgoing",
    ]
    for rule in cfg.get("allow_ports", []):
        if isinstance(rule, int):
            commands.append(f"ufw allow {rule}/tcp")
        else:
            commands.append(f"ufw allow {rule}")

    commands.append("ufw --force enable")

    server.shell(
        name="Apply ufw rules",
        commands=commands,
        _sudo=True,
    )
