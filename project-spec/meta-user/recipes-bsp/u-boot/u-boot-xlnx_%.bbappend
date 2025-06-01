FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " file://platform-top.h file://bsp.cfg"
SRC_URI += "file://user_2025-05-25-08-13-00.cfg \
            file://user_2025-06-01-00-17-00.cfg \
            file://user_2025-06-01-00-44-00.cfg \
            file://user_2025-06-01-02-11-00.cfg \
            "

