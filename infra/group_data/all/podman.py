podman = {
    "auto_update": True,
    "packages": ["podman", "slirp4netns", "fuse-overlayfs", "uidmap"],
    "subid_base": 100000,
    "subid_count": 65536,
}

# Rootless quadlets: one file per service under files/quadlets/ or inline content.
quadlets = [
    {
        "name": "example",
        "user": "deploy",
        "template": "example.container",
    },
]
