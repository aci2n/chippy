"""SSH daemon hardening."""

from pyinfra.api import deploy
from pyinfra.operations import files, systemd

from operations._helpers import any_changed, data, in_group


def _sshd_line(key: str, value: str):
    return files.line(
        name=f"sshd_config {key}",
        path="/etc/ssh/sshd_config",
        line=f"{key} {value}",
        replace=f"^{key}\\s+.*",
        _sudo=True,
    )


@deploy("Harden SSH")
def harden_ssh():
    if not in_group("hardening"):
        return

    cfg = data("ssh", {})
    if not cfg.get("enabled", True):
        return

    changes = [
        _sshd_line("PermitRootLogin", cfg.get("permit_root_login", "no")),
        _sshd_line("PasswordAuthentication", cfg.get("password_authentication", "no")),
        _sshd_line("KbdInteractiveAuthentication", cfg.get("kbd_interactive_authentication", "no")),
        _sshd_line("ChallengeResponseAuthentication", cfg.get("challenge_response_authentication", "no")),
        _sshd_line("X11Forwarding", cfg.get("x11_forwarding", "no")),
        _sshd_line("MaxAuthTries", str(cfg.get("max_auth_tries", 3))),
        _sshd_line("ClientAliveInterval", str(cfg.get("client_alive_interval", 300))),
        _sshd_line("ClientAliveCountMax", str(cfg.get("client_alive_count_max", 2))),
    ]

    allowed = cfg.get("allow_users")
    if allowed:
        changes.append(_sshd_line("AllowUsers", " ".join(allowed)))

    systemd.service(
        name="Restart ssh",
        service="ssh",
        restarted=True,
        _if=any_changed(*changes),
        _sudo=True,
    )
