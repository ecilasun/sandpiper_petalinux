qemu-system-arm -m 1024 -M xilinx-zynq-a9 -kernel ./zImage -gdb tcp::9000 -dtb ./system.dtb -initrd ./rootfs.cpio.gz.u-boot -display sdl -serial stdio
