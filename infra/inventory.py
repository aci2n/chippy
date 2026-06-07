"""
Host inventory for pyinfra.

Define hosts once, then assign them to groups. Operations gate on group
membership (see operations/*.py). Override per-host data in the tuple dict.

Example:
    debian = [
        ("10.0.0.10", {"ssh_user": "root"}),
        ("10.0.0.11", {"ssh_user": "root"}),
    ]
    base = debian
    hardening = debian
    podman = [("10.0.0.10", {"ssh_user": "root"})]
"""

# Edit before running against real servers.
debian = [
    # ("10.0.0.10", {"ssh_user": "root"}),
]

# Default demo host so `pyinfra inventory.py debug-inventory` works out of the box.
if not debian:
    debian = [
        ("example.debian.local", {"ssh_user": "root"}),
    ]

base = debian
hardening = debian
podman = debian
