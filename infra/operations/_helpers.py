"""Shared helpers for operations modules."""

from io import StringIO

from pyinfra import host
from pyinfra.operations.util import any_changed

__all__ = ["any_changed", "data", "in_group", "string_put"]


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
