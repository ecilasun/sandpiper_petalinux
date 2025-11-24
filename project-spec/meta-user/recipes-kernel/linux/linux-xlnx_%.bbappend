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
            file://user_2025-07-28-00-53-00.cfg \
            file://user_2025-07-29-01-27-00.cfg \
            file://user_2025-08-09-00-07-00.cfg \
            file://user_2025-08-14-17-58-00.cfg \
            file://user_2025-09-01-11-44-00.cfg \
            file://user_2025-11-24-00-48-00.cfg \
            file://user_2025-11-24-01-21-00.cfg \
            "

