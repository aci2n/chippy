"""Shared helpers for operations modules."""

import re
from io import StringIO

from pyinfra import host
from pyinfra.facts.files import FindInFile
from pyinfra.facts.server import Timezone
from pyinfra.facts.systemd import SystemdEnabled, SystemdStatus
from pyinfra.operations.util import any_changed

from facts.systemd import SystemdLinger

__all__ = [
    "any_changed",
    "data",
    "file_missing_line",
    "in_group",
    "linger_disabled",
    "string_put",
    "timezone_differs",
    "unit_inactive",
    "unit_not_enabled",
]


def in_group(name: str) -> bool:
    return name in host.groups


def data(key: str, default=None):
    return getattr(host.data, key, default)


def string_put(name: str, dest: str, contents: str, **kwargs):
    """Upload inline text via files.put (pyinfra 3 requires a src object)."""
    from pyinfra.operations import files

    return files.put(
        name=name,
        src=StringIO(contents),
        dest=dest,
        **kwargs,
    )


def _unit_name(service: str) -> str:
    if service.endswith(
        (
            ".service",
            ".socket",
            ".device",
            ".mount",
            ".automount",
            ".swap",
            ".target",
            ".path",
            ".timer",
            ".slice",
            ".scope",
        )
    ):
        return service
    return f"{service}.service"


def linger_disabled(username: str):
    """Callable for _if: run when linger is not yet enabled for user."""

    def _check() -> bool:
        return not host.get_fact(SystemdLinger, user=username)

    return _check


def timezone_differs(timezone: str):
    """Callable for _if: run when the host timezone does not match."""

    def _check() -> bool:
        return host.get_fact(Timezone) != timezone

    return _check


def file_missing_line(path: str, line: str):
    """Callable for _if: run when line is absent from a remote file."""

    def _check() -> bool:
        matches = host.get_fact(FindInFile, path=path, pattern=f"^{re.escape(line)}$")
        return not matches

    return _check


def unit_inactive(service: str, user_mode: bool = False, user_name: str | None = None):
    """Callable for _if: run when a systemd unit is not active."""

    unit = _unit_name(service)

    def _check() -> bool:
        status = host.get_fact(
            SystemdStatus,
            user_mode=user_mode,
            user_name=user_name,
            services=[unit],
        )
        return not status.get(unit, False)

    return _check


def unit_not_enabled(service: str, user_mode: bool = False, user_name: str | None = None):
    """Callable for _if: run when a systemd unit is not enabled."""

    unit = _unit_name(service)

    def _check() -> bool:
        enabled = host.get_fact(
            SystemdEnabled,
            user_mode=user_mode,
            user_name=user_name,
            services=[unit],
        )
        return not enabled.get(unit, False)

    return _check
