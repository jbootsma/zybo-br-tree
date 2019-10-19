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

define CRC32_EXAMPLE_BUILD_CMDS
	$(MAKE) CXX="$(TARGET_CXX)" -C $(@D)/test
endef

define CRC32_EXAMPLE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/test/crc32_test $(TARGET_DIR)/usr/bin
endef

CRC32_EXAMPLE_MODULE_SUBDIRS = mod
$(eval $(kernel-module))
$(eval $(generic-package))

