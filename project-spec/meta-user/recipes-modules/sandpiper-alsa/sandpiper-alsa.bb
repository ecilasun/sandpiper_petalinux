SUMMARY = "Sandpiper ALSA Audio Driver"
DESCRIPTION = "ALSA sound driver for Sandpiper APU (Audio Processing Unit)"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

inherit module

SRC_URI = " \
    file://sandpiper_alsa.c \
    file://Makefile \
    file://asound.conf \
"

S = "${WORKDIR}"

KERNEL_MODULE_AUTOLOAD += "sandpiper_alsa"

do_compile() {
    unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS
    oe_runmake -C ${STAGING_KERNEL_DIR} M=${S} KERNEL_SRC=${STAGING_KERNEL_DIR}
}

do_install() {
    install -d ${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/sound/drivers
    install -m 0644 sandpiper_alsa.ko ${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/sound/drivers/
    
    # Install ALSA configuration
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/asound.conf ${D}${sysconfdir}/asound.conf
}

FILES:${PN} += "${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/sound/drivers/sandpiper_alsa.ko"
FILES:${PN} += "${sysconfdir}/asound.conf"
