# Sandpiper SoC

This repo contains the OS kernel and driver build files for project sandpiper.

Please check the individual license files in the source directory for any third party opensource library used.

Linux drivers, hardware design ideas and video/audio/vcp units (C)2025 Engin Cilasun

# Building Petalinux with existing configuration

1) Run the following to import the hardware module:
```
importhardware.sh
```
Exit any software that pops up and accept to save the configuration

2) Run the following to build petalinux:
```
build.sh
```

3) Now it's time to run the package script:
```
package.sh
```
This will take a bit of time and finally generate the bootable images in your images/linux folder.
The following files from this folder can then be copied to an sdcard's BOOT partition:
```
boot.scr
BOOT.BIN
image.ub
```
In addition, sandpiper requires the splash.bin file from image/BOOT/ folder which is the splash screen image.

3) To get a qemu image built, use the following:
```
makequemuimage.sh
```
This will take a short while and produce a bootable configuration for us to use with QUEMU

4) To run QUEMU with our virtual device, use the following:
```
emulate.sh
```
This will boot the device with Petalinux (which is not what is shipped on the sdcard image with sandpiper, but all the same)
Once you get to the login prompt, use the following to log in:
```
username: peta
password: peta
```
The default QUEMU emulator won't show video or have any functioning devices, but the sandpiper devide will be up and running since it's essentially just a memory mapper. Future versions will improve the experience and allow for device functionality as much as possible, by extending QEMU.

# About Sandpiper

Sandpiper is an interesting machine. It is a linux based small computer based around a Zynq 7020 SoC, with custom video and audio circuitry programmed into the FPGA fabric. A specialzed device driver allows access to a shared memory region and some control registers to control these video and audio devices.

An SDK is provided alongside the PCB for the keyboard module and enclosure files for 3D printing, as well as the build files for Linux kernel and drivers in the following repositories:

https://github.com/ecilasun/sandpiper_hw/

https://github.com/ecilasun/sandpiper_petalinux/

https://github.com/ecilasun/sandpiper_pcb/

https://github.com/ecilasun/sandpiper_sdk/
