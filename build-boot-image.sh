#!/bin/bash
mkdir out/
make O=out/ ARCH=arm sf6580_weg_l_defconfig
make O=out/ ARCH=arm CROSS_COMPILE=$PWD/toolchain/arm-eabi-4.8/bin/arm-eabi- $1
./bootimg/mkbootimg --base 0 --pagesize 2048 --board Blocks_Sombrero --kernel_offset 0x80008000 --ramdisk_offset 0x84000000 --second_offset 0x80f00000 --tags_offset 0x8e000000 --cmdline 'bootopt=64S3,32S1,32S1 androidboot.selinux=permissive' --kernel out/arch/arm/boot/zImage-dtb --ramdisk bootimg/ramdisk.cpio.gz -o bootimg/boot.img
