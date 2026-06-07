# Debian server configuration with pyinfra

Minimal, scalable layout for reproducible Debian hardening, user setup, and **rootless Podman quadlets** with registry auto-updates.

## Layout

```text
infra/
  inventory.py          # hosts + groups (debian, base, hardening, podman)
  deploy.py             # entrypoint; imports all operations
  group_data/all/       # shared config (users, ssh, sysctl, quadlets, …)
  operations/           # idempotent @deploy functions, gated by group
  files/quadlets/       # quadlet unit templates
  config/               # optional host documentation / future generators
```

**Scaling pattern**

| Layer | Responsibility |
|-------|----------------|
| `inventory.py` | Which machines exist; group membership (`base`, `hardening`, `podman`) |
| `group_data/<group>/` | Per-environment overrides (`group_data/production/`, `staging/`, …) |
| `operations/` | Reusable, composable deploy steps |
| `files/` | Static templates (quadlets, future configs) |

Add environments by creating `group_data/production/users.py` etc. pyinfra merges group data; more specific groups override `all`.

## Quick start

```bash
cd infra
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# Inspect inventory + merged data
pyinfra inventory.py debug-inventory

# Dry run (no changes)
pyinfra inventory.py deploy.py --dry

# Apply (confirm when prompted)
pyinfra inventory.py deploy.py
```

Limit scope:

```bash
pyinfra inventory.py deploy.py --limit podman
pyinfra inventory.py deploy.py --limit 10.0.0.10
```

## Before first deploy

1. Edit `inventory.py` — replace the example host with your servers.
2. Edit `group_data/all/users.py` — add SSH public keys for the `deploy` user.
3. Review `group_data/all/ssh.py` — `AllowUsers` must include your login user.
4. Adjust `group_data/all/hardening.py` — firewall ports, fail2ban on/off.
5. Define services in `group_data/all/podman.py` (`quadlets` list).

## What gets configured

### Base (`base` group)

- Baseline packages, apt upgrade, timezone
- Local users, sudo, authorized keys
- `loginctl enable-linger` for users with `podman: true`

### Hardening (`hardening` group)

- SSH: no root/password login, `AllowUsers`, sane timeouts
- `sysctl` drop-in under `/etc/sysctl.d/`
- `unattended-upgrades` for security patches
- Optional: `fail2ban`, `ufw` (enabled in defaults — disable in group_data if unwanted)

### Podman (`podman` group)

- Podman + rootless dependencies (`slirp4netns`, `fuse-overlayfs`, `uidmap`)
- `/etc/subuid` + `/etc/subgid` for container users
- Rootless quadlets in `~/.config/containers/systemd/`
- `podman-auto-update.timer` per user (registry image updates)

## Rootless quadlets + auto-update

Quadlet template example (`files/quadlets/example.container`):

```ini
[Container]
Image=docker.io/library/nginx:alpine
PublishPort=8080:80
AutoUpdate=registry
```

Register in `group_data/all/podman.py`:

```python
quadlets = [
    {"name": "myapp", "user": "deploy", "template": "myapp.container"},
    # or inline: {"name": "x", "user": "deploy", "content": "..."},
]
```

Requirements on the server:

- Target user exists and has `podman: true` (or is listed in `podman_users`)
- Systemd user linger enabled (handled automatically)
- Images use `AutoUpdate=registry` and/or label `io.containers.autoupdate=registry`

## Change-aware restarts

Operations capture return values from file edits and gate restarts/reloads with `_if`:

```python
config = files.line(...)
systemd.service(
    service="ssh",
    restarted=True,
    _if=config.did_change,
)
```

Multiple edits use `any_changed` from `operations._helpers`. SSH, sysctl, fail2ban, and quadlet deploys follow this pattern so unchanged runs skip service restarts and `daemon-reload`.

## Suggested rollout order

On fresh Debian hosts, run once with all groups:

```bash
pyinfra inventory.py deploy.py -y
```

For ongoing changes, limit to what you changed:

```bash
pyinfra inventory.py deploy.py --limit podman -y
```

## CI / automation

```bash
pyinfra inventory.py deploy.py --dry -y   # plan in CI
pyinfra inventory.py deploy.py -y         # apply from a trusted runner with SSH key
```

Store secrets (SSH keys) outside the repo; inject via CI variables or `group_data` generated at runtime.
