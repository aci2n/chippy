timezone = "UTC"

sysctl = {
    "net.ipv4.conf.all.rp_filter": 1,
    "net.ipv4.conf.default.rp_filter": 1,
    "net.ipv4.icmp_echo_ignore_broadcasts": 1,
    "net.ipv4.conf.all.accept_redirects": 0,
    "net.ipv4.conf.default.accept_redirects": 0,
    "net.ipv6.conf.all.accept_redirects": 0,
    "net.ipv6.conf.default.accept_redirects": 0,
    "kernel.randomize_va_space": 2,
    "fs.protected_hardlinks": 1,
    "fs.protected_symlinks": 1,
}

unattended_upgrades = {
    "enabled": True,
}

fail2ban = {
    "enabled": True,
    "bantime": 3600,
    "findtime": 600,
    "maxretry": 5,
}

firewall = {
    "enabled": True,
    "allow_ports": [22, 80, 443],
}
