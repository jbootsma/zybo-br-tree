This directory contains a few scripts usefull for assembling zybo outputs.

- `mkfit.py` Is used to generate the input file for mkimage. It creates a file
	describing the kernel, ramdisk, and device-tree inputs along with an
	optional fpga bitstream. It also creates a single default config that
	loads all images. When used with mkimage it creates a single binary
	suitable for loading by u-boot.

- `post-image.sh` First invokes mkfit.py, and then mkimage to create the final
	image. It should be passed the path of the (compiled) device tree to use
	and can take an optional second argument pointing to a bistream to
	include. All paths are relative to the images folder in the output dir.
