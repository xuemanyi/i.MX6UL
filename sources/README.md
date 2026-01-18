# Source Archives

This directory contains the original source code archives used for learning and developing on the i.MX6UL platform.

Only **unmodified source tarballs** are stored here.  
Extracted source code should be placed in the corresponding directories.


## U-Boot

Extract the U-Boot source archive:

```bash
tar -xjvf u-boot-imx-2016.03-1.0.0.tar.bz2 -C ../bootloader/
```


## Linux Kernel
Extract the Linux kernel source archive:

```bash
tar -xjvf linux-imx-4.1.15-2.1.0-g3dc0a4b-v2.7.tar.bz2 -C ../kernel/
```


## Root Filesystem (BusyBox)

Extract the BusyBox source archive for root filesystem creation:

```bash
tar -xjvf busybox-1.29.0.tar.bz2 -C ../rootfs/ --strip-components=1