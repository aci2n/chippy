"""Systemd/logind facts not provided by pyinfra."""

from typing_extensions import override

from pyinfra.api import FactBase, QuoteString, StringCommand


class SystemdLinger(FactBase[bool]):
    """
    Return whether systemd user linger is enabled for a local user.

    Uses ``loginctl show-user <user> -p Linger``.
    """

    @override
    def requires_command(self, *args, **kwargs) -> str:
        return "loginctl"

    @staticmethod
    def default() -> bool:
        return False

    @override
    def command(self, user: str) -> StringCommand:
        return StringCommand(
            "loginctl",
            "show-user",
            QuoteString(user),
            "-p",
            "Linger",
            "--value",
        )

    @override
    def process(self, output: list[str]) -> bool:
        if not output:
            return False
        return output[0].strip().lower() == "yes"
