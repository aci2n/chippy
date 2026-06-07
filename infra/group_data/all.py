"""
Group data for all hosts.

pyinfra loads one module per group name from group_data/*.py. This file
re-exports variables from group_data/all/*.py so config stays split by topic.
"""

__all__ = [
    "users",
    "podman_linger_users",
    "ssh",
    "timezone",
    "sysctl",
    "unattended_upgrades",
    "fail2ban",
    "firewall",
    "podman",
    "quadlets",
]


def _merge_fragments():
    import pathlib

    merged = {}
    for fragment_path in sorted((pathlib.Path(__file__).parent / "all").glob("*.py")):
        namespace = {}
        source = fragment_path.read_text(encoding="utf-8")
        exec(compile(source, str(fragment_path), "exec"), namespace)  # noqa: S102
        for name, value in namespace.items():
            if not name.startswith("_"):
                merged[name] = value
    return merged


globals().update(_merge_fragments())
del _merge_fragments
