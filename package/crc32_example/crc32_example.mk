################################################################################
#
# CRC32 Example
#
################################################################################

CRC32_EXAMPLE_VERSION = 1.0.0
CRC32_EXAMPLE_SITE = $(BR2_EXTERNAL_ZYBO_Z7_PATH)/package/crc32_example/src
CRC32_EXAMPLE_SITE_METHOD = local
CRC32_EXAMPLE_LICENSE = GPL-3.0-or-later
CRC32_EXAMPLE_LICENSE_FILES = COPYING

CRC32_EXAMPLE_MODULE_SUBDIRS = mod
$(eval $(kernel-module))
$(eval $(generic-package))

