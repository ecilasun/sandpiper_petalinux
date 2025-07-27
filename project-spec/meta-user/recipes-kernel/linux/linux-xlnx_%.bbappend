FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " file://bsp.cfg"
KERNEL_FEATURES:append = " bsp.cfg"
SRC_URI += "file://user_2025-06-19-21-08-00.cfg \
            file://user_2025-06-19-23-16-00.cfg \
            file://user_2025-06-19-23-42-00.cfg \
            file://user_2025-06-20-01-34-00.cfg \
            file://user_2025-06-20-02-00-00.cfg \
            file://user_2025-06-22-22-35-00.cfg \
            file://user_2025-07-19-23-37-00.cfg \
            file://user_2025-07-20-23-57-00.cfg \
            file://user_2025-07-27-01-39-00.cfg \
            "

