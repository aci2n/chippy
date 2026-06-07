"""Custom pyinfra facts for this inventory."""

from facts.systemd import SystemdLinger

__all__ = ["SystemdLinger"]
