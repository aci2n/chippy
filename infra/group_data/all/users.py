# Local users. Set authorized_keys to your public keys before deploying.
users = [
    {
        "name": "deploy",
        "groups": ["sudo"],
        "shell": "/bin/bash",
        "authorized_keys": [
            # "ssh-ed25519 AAAA... you@laptop",
        ],
        "passwordless_sudo": True,
        "podman": True,
    },
]

# Extra users that need systemd linger (in addition to users with podman=True).
podman_linger_users = []
