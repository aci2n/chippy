"""
Main deploy entrypoint.

Run:
    pyinfra inventory.py deploy.py

Limit to a group or host:
    pyinfra inventory.py deploy.py --limit podman
    pyinfra inventory.py deploy.py --limit web1.example.com
"""

from operations import base, hardening, podman, quadlets, ssh, users

# Importing registers @deploy operations with pyinfra.
