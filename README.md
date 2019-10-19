# Quick Start

1. Checkout buildroot at a location of your choice.
	```
	git clone https://github.com/buildroot/buildroot.git
	git checkout 2019.08.1
	```
	It is suggested to do this outside the zybo repo.

2. Seed the configuration with the default config for the board.
	```
	make -C <path-to-buildroot> O=`pwd`/output BR2_EXTERNAL=`pwd` zybo_z720_defconfig
	```
	The above example is for running from the zybo repo.

	- -C points make at the buildroot makefile. Set this to where you
	  checked out buildroot.

	- O sets the build directory, here to the output subfolder.

	- BR2_EXTERNAL tells buildroot where to find the zybo tree.

	After the first make command make can be run as normal from the output
	directory without any of the extra parameters.

3. Customize your buildroot configuration.
	```
	cd output
	make nconfig
	```

	It is suggested to enable ccache (`Build options` ->
	`Enable compiler cache`) in order to speed up rebuilds that may be
	required.

	It may also be useful to adjust the mirrors and download locations in
	order to increase the speed of the initial package downloads. This
	setting is also found under `Build options`.

4. Run the build.
	```
	make all
	```

5. Copy the outputs to a FAT-32 formatted SD card:
	```
	cp images/{boot.bin,u-boot.img,fit.itb} <path-to-sd-card-mount>
	```

6. Set the jumper on your zybo for sd-card boot, insert the sd card, and
	enable the power.

7. Attach a console to the uart port on the zybo and login. By default the only
	user is root, and no password is needed. Suggested consoles are putty
	for windows, or minicom for linux.

# Outputs from the build

All outputs from tbe build are put into the images subdir.

- `boot.bin` is the first stage bootloader. It can be placed on the root
	of an sd card, or written to address 0 of the flash on the zybo
	for a qspi boot. u-boot contains commands for manipulating the
	flash and is the suggested way to program the qspi.

- `u-boot.img` is the second stage bootloader. The first stage
	bootloader expects to load this the same way the first stage was
	loaded. It should be in the root of the sd-card for an sd-card
	boot. For a qspi boot it can be written to address 0x100000 in the
	external flash, as set by `include/configs/zynq-common.h` within
	the u-boot source.

- `zImage` is the linux kernel itself. This will be packaged into `fit.itb`.

- `rootfs.cpio(.gz)` is the root file-system. It will be packaged into
	`fit.itb`. At runtime the filesystem is unpacked into ram, so changes
	won't be persistent.

- `*.dtb` is the compiled device tree. It's name will be dependant on
	configuration. This will be packaged into `fit.itb`.

- `fit.its` is a text description of the image file. It is a auto-generated
	intermediate.

- `fit.itb` is the image file containing the kernel and root-fs. For an
	sd-card boot it can be loaded automatically from the root of the
	sd-card.

# Available defconfigs

Currently the repo contains the following default configurations

- `zybo_z720_defconfig` is a good starting point for new projects. It is a 
  barebones configuration for the zybo-z7 board with a zynq-7020. It uses the
  appropriate in-tree device tree and config files for all components, and does
  not load any bitstream on boot. The root-fs is a ramdisk.

- `crc_example_defconfig` is a modification of the base zybo config. It adds
  a bit stream to the system image to be loaded on startup, which contains a
  hardware implementation of the crc-32 algorithm. A custom device tree is used
  to add the hardware block, and a buildroot package is added which contains the
  kernel driver to interact with the hardware block.

# Network booting

When developing it may be useful to setup a network load of fit.itb, in order
to save time copying each iteration to the sd card.

To start with ensure you have a system that can boot to u-boot, and remove
any fit.itb already programmed to prevent auto-loading.

Next you will need to setup a tftp server to serve fit.itb, and configure it
to have a static ip-address. Hook up the zybo using an ethernet cable to the
same LAN as the tftp server.

Boot into u-boot on the zybo and interrupt the auto boot.

Configure the ip used by your tftp server, and one to be used by the zybo.
```
editenv host_ip
edit: <tftp server addr>
editenv ipaddr
edit: <zybo addr>
```

Create the boot command that can be used to load over tftp
```
editenv bootcmd_tftp
edit: tftpboot $load_addr $host_ip:$fit_image && bootm $load_addr
```

Edit the boot_targets variable to include the new tftp target.
```
editenv boot_targets
edit: tftp mmc0 usb0 pxe dhcp
```

Save the modifications to external flash so they are persisted across resets
```
saveenv
```

Reset the board to test the boot. When the image cannot be found locally u-boot
will automatically attempt to load fit.itb from the tftp server. Just rebuild
and then reset the board to try modifications.
