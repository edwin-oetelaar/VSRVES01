# VSRVES01
code and notes related to VLSI Solution's Risc-V Engineering Samples version 01 VLSI.fi

we need a special toolchain :
```
git clone --depth 1 https://github.com/riscv/riscv-gnu-toolchain

./configure --prefix=/tmp/new-toolchain --with-arch=rv32ima_zicsr --enable-linux --with-cmodel=medlow --with-abi=ilp32

make -j 8
```
