Sources used to generate the bit file are provided, some manual work is needed
to reproduce the bit stream.

`hls-src` Is the source code and testbench used for HLS synthesis. All pragmas
are included in the source, Vivado HLS can be used to generate the ip block
with all default settings other than setting the language standard to c++11.

`hw-src` Is the block design used to integrate the CRC HW block. To use it
create a new project in vivado. Then add the packaged ip from the HLS project
as an ip repo. The block design script may need to be modified to look for the
CRC ip block with the right name based on settings used to export from Vivado
HLS. To expand the block design create a new block desing in the project.
Then use `Tools` -> `Run TCL script` on `crc.tcl`. Then in the TCL console
in vivado run
```
create_root_design ""
```
This will expand the design into the currently open block design. After this
point the design can be synthesized as normal.
