FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " file://bsp.cfg"
KERNEL_FEATURES:append = " bsp.cfg"
SRC_URI += "file://user_2025-05-25-08-03-00.cfg \
            file://user_2025-05-26-07-01-00.cfg \
            file://user_2025-05-31-17-47-00.cfg \
            file://user_2025-05-31-21-54-00.cfg \
            "

