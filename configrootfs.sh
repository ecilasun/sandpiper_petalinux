sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0
petalinux-config -c rootfs
