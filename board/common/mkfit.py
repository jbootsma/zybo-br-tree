#!/usr/bin/env python
import collections
import itertools
import json
import os.path
import subprocess
import sys


class Writer(object):
    def __init__(self, dst):
        self.dst = dst
        self.indent = ''
        self.indents = []

    def push_indent(self, indent='\t'):
        self.indents.append(indent)
        self.indent = ''.join(self.indents)

    def pop_indent(self):
        self.indents.pop()
        self.indent = ''.join(self.indents)

    def writeln(self, line):
        self.dst.write(self.indent)
        self.dst.write(line)
        self.dst.write('\n')


class ImageTree(object):
    def __init__(self, desc, configs):
        self.desc = desc
        self.configs = configs
        self.images = {i for i in itertools.chain(*(
            [conf.kernel, conf.ramdisk, conf.fdt, conf.fpga]
            for conf in configs
        )) if i is not None}

    def write(self, dst):
        dst = Writer(dst)
        dst.writeln('/* Auto-generated image config */')
        dst.writeln('/dts-v1/;')
        dst.writeln('/ {')
        dst.push_indent()

        dst.writeln('description = "{}";'.format(self.desc))
        dst.writeln('#address-cells = <1>;')
        dst.writeln('images {')
        dst.push_indent()

        for image in sorted(self.images, key=lambda i: i.name):
            image.write(dst)

        dst.pop_indent()
        dst.writeln('};')
        dst.writeln('configurations {')
        dst.push_indent()

        dst.writeln('default = "{}";'.format(self.configs[0].name))
        for config in self.configs:
            config.write(dst)

        dst.pop_indent()
        dst.writeln('};')

        dst.pop_indent()
        dst.writeln('};')


class Image(object):
    counters = collections.defaultdict(lambda: 0)

    def __init__(self):
        self.name = ''
        self.desc = ''
        self.file_name = ''
        self.image_type = ''
        self.arch = None
        self.os = None
        self.compression = 'none'
        self.loadaddr = None
        self.entryaddr = None

    def make_name(self, prefix=None):
        if self.name is not None:
            return

        if prefix is None:
            prefix = self.image_type

        Image.counters[prefix] += 1
        self.name = '{}-{}'.format(
            prefix,
            Image.counters[prefix]
        )

    def write(self, dst):
        dst.writeln('{} {{'.format(self.name))
        dst.push_indent()

        dst.writeln('description = "{}";'.format(self.desc))
        dst.writeln('data = /incbin/("{}");'.format(self.file_name))
        dst.writeln('type = "{}";'.format(self.image_type))
        if self.arch is not None:
            dst.writeln('arch = "{}";'.format(self.arch))
        if self.os is not None:
            dst.writeln('os = "{}";'.format(self.os))
        dst.writeln('compression = "{}";'.format(self.compression))
        if self.loadaddr is not None:
            dst.writeln('load = <0x{:X}>;'.format(self.loadaddr))
        if self.entryaddr is not None:
            dst.writeln('entry = <0x{:X}>;'.format(self.entryaddr))

        # Fixed hashes for everything!
        def write_hash(name, algo):
            dst.writeln('{} {{'.format(name))
            dst.push_indent()
            dst.writeln('algo = "{}";'.format(algo))
            dst.pop_indent()
            dst.writeln('};')

        write_hash('hash-1', 'crc32')
        write_hash('hash-2', 'sha1')

        dst.pop_indent()
        dst.writeln('};')


class Kernel(Image):
    def __init__(self):
        super(Kernel, self).__init__()
        self.name = 'kernel'
        self.desc = 'Linux Kernel'
        self.file_name = 'zImage'
        self.image_type = 'kernel'
        self.arch = 'arm'
        self.os = 'linux'
        self.loadaddr = 0x8000
        self.entryaddr = self.loadaddr


class Ramdisk(Image):
    def __init__(self):
        super(Ramdisk, self).__init__()
        self.name = 'ramdisk'
        self.desc = 'Initial ram disk'
        self.file_name = 'rootfs.cpio.gz'
        self.image_type = 'ramdisk'
        self.arch = 'arm'
        self.os = 'linux'
        self.compression = 'gzip'


class Fdt(Image):
    def __init__(self, in_file, name=None):
        super(Fdt, self).__init__()
        self.name = name
        self.desc = in_file
        self.file_name = in_file
        self.image_type = 'flat_dt'
        self.arch = 'arm'
        self.make_name('fdt')


class Fpga(Image):
    def __init__(self, in_file, name=None):
        super(Fpga, self).__init__()
        self.name = name
        self.desc = in_file
        self.file_name = in_file
        self.image_type = 'fpga'
        self.arch = 'arm'
        self.os = 'linux'
        self.loadaddr = 100 * 1024 * 1024
        self.make_name()


class Configuration(object):
    counter = 0

    def __init__(
            self, desc, kernel,
            ramdisk=None,
            fdt=None,
            fpga=None,
            name=None):

        if name is None:
            Configuration.counter += 1
            name = 'conf-{}'.format(Configuration.counter)

        self.desc = desc
        self.kernel = kernel
        self.ramdisk = ramdisk
        self.fdt = fdt
        self.fpga = fpga
        self.name = name

    def write(self, dst):
        dst.writeln('{} {{'.format(self.name))
        dst.push_indent()

        dst.writeln('description = "{}";'.format(self.desc))
        dst.writeln('kernel = "{}";'.format(self.kernel.name))
        if self.ramdisk is not None:
            dst.writeln('ramdisk = "{}";'.format(self.ramdisk.name))
        if self.fdt is not None:
            dst.writeln('fdt = "{}";'.format(self.fdt.name))
        if self.fpga is not None:
            dst.writeln('fpga = "{}";'.format(self.fpga.name))

        dst.pop_indent()
        dst.writeln('};')


def make_img():
    kernel = Kernel()
    ramdisk = Ramdisk()

    # For now only support 1 config.
    fdt = Fdt(sys.argv[2])
    fpga = None
    if len(sys.argv) > 3:
        fpga = Fpga(sys.argv[3])
    config = Configuration('Default Configuration', kernel, ramdisk, fdt, fpga)
    tree = ImageTree('Zybo Z7 custom configuration', [config])

    with open(sys.argv[1], 'w') as dst:
        tree.write(dst)


if __name__ == '__main__':
    make_img()
