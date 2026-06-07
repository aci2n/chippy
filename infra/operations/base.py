"""Baseline Debian packages and apt hygiene."""

from pyinfra.api import deploy
from pyinfra.operations import apt, server

from operations._helpers import data, in_group


@deploy("Install baseline packages")
def install_baseline_packages():
    if not in_group("base"):
        return

    apt.packages(
        name="Install baseline packages",
        packages=[
            "ca-certificates",
            "curl",
            "gnupg",
            "lsb-release",
            "sudo",
            "vim-tiny",
        ],
        present=True,
        update=True,
        _sudo=True,
    )


@deploy("Upgrade apt packages")
def upgrade_packages():
    if not in_group("base"):
        return

    apt.upgrade(
        name="Upgrade installed packages",
        _sudo=True,
    )


@deploy("Set system timezone")
def configure_timezone():
    if not in_group("base"):
        return

    timezone = data("timezone", "UTC")
    server.shell(
        name=f"Set timezone to {timezone}",
        commands=[f"timedatectl set-timezone {timezone}"],
        _sudo=True,
    )
