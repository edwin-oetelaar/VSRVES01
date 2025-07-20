ELF2VRI converter 2025-06-18 VLSI Solution
------------------------------------------

1. DESCRIPTION
--------------

When you compile your own Linux image, you will typically end up with
either one ELF file and, depending on how you compile the RAM disk in,
potentionally also some raw binary files. elf2vri converts these
ELF / RAW files to the extremely simple VRI (VLSI Solution's RISC-V)
format that can be quickly loaded by DDRLoad.

ELF2VRI requires readelf to be installed if you need to convert elf files.
readelf is not required for binary files.

The source code in this package may be freely used and modified as long
as it has something to do with VLSI Solution's RISC-V chips.

Compile like this:
gcc -g -Wall -funsigned-char -O -o elf2vri elf2vri.c



2. FULL DOCUMENTATION FOR THE VRI FORMAT
----------------------------------------

The file looks as follows:

  "VRI1"
  Section0
  ...
  SectionN-1

Sections can be either literal run or RLE zero run:

  Section, literal run:
      56 78 12 34	ADDR: Address (mixed-endian, value is 0x12345678)
      56 78 12 34	SIZE: Literal run size in bytes
      00 08 00 00	FLAGS: FL_READ 1, FL_WRITE 2, FL_EXECUTE 4, no FL_BSS 8
      00 00 00 00	EXTENSION: 0
    n*56 78 12 34	Literal data n=SIZE/4.

  Section, RLE zero run:
      56 78 12 34	ADDR: Address (mixed-endian, value is 0x12345678)
      56 78 12 34	SIZE: Literal run size in bytes
      00 08 00 00	FLAGS: FL_READ 1, FL_WRITE 2, FL_EXECUTE 4, set FL_BSS 8
      00 00 00 00	EXTENSION: 0

Note!
Sections are independent of each other so new sections can be
appended to an existing image with no issues.

Note!
All 32-bit values are encoded in a similar mixed-endian way as shown above.
