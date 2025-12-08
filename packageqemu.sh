cd images/linux
sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0
petalinux-package --wic --outdir .

