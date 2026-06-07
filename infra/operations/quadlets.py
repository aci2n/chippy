"""Deploy rootless Podman quadlet units."""

from pathlib import Path

from pyinfra.api import deploy
from pyinfra.operations import files, systemd

from operations._helpers import any_changed, data, in_group, string_put

_QUADLET_DIR = Path(__file__).resolve().parent.parent / "files" / "quadlets"


def _quadlet_filename(name: str) -> str:
    if name.endswith((".container", ".volume", ".network", ".pod", ".kube")):
        return name
    return f"{name}.container"


def _service_name(filename: str) -> str:
    for suffix in (".container", ".volume", ".network", ".pod", ".kube"):
        if filename.endswith(suffix):
            return filename[: -len(suffix)]
    return filename


def _quadlet_content(entry: dict) -> str:
    if "content" in entry:
        return entry["content"]

    template = entry.get("template")
    if template:
        path = _QUADLET_DIR / template
        return path.read_text()

    raise ValueError(f"quadlet {entry.get('name')!r} needs 'content' or 'template'")


@deploy("Deploy rootless quadlets")
def deploy_quadlets():
    if not in_group("podman"):
        return

    quadlets = data("quadlets", [])
    if not quadlets:
        return

    services_by_user: dict[str, list[str]] = {}
    changes_by_user: dict[str, list] = {}

    for entry in quadlets:
        name = entry["name"]
        username = entry.get("user")
        if not username:
            raise ValueError(f"quadlet {name!r} requires 'user' for rootless deploy")

        dest_dir = f"/home/{username}/.config/containers/systemd"
        filename = _quadlet_filename(name)

        files.directory(
            name=f"Quadlet dir for {username}",
            path=dest_dir,
            mode="0755",
            user=username,
            group=username,
            _sudo=True,
        )

        quadlet_file = string_put(
            name=f"Deploy quadlet {filename}",
            dest=f"{dest_dir}/{filename}",
            contents=_quadlet_content(entry),
            mode="0644",
            user=username,
            group=username,
            _sudo=True,
        )

        services_by_user.setdefault(username, []).append(_service_name(filename))
        changes_by_user.setdefault(username, []).append(quadlet_file)

    for username, services in sorted(services_by_user.items()):
        quadlets_changed = any_changed(*changes_by_user[username])

        systemd.daemon_reload(
            name=f"Reload systemd user units for {username}",
            user_mode=True,
            user_name=username,
            _if=quadlets_changed,
            _sudo=True,
        )

        for service in services:
            systemd.service(
                name=f"Enable {service} for {username}",
                service=service,
                user_mode=True,
                user_name=username,
                enabled=True,
                running=True,
                _if=quadlets_changed,
                _sudo=True,
            )
