#!/bin/bash
set -e

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <arch> <platform> [make_args]"
  exit 1
fi

if [[ $1 = "arm32" ]]; then
  export CC=arm-linux-gnueabihf-gcc
  export CXX=arm-linux-gnueabihf-g++
  export LD=arm-linux-gnueabihf-ld
  aname=arm32
elif [[ $1 = "arm64" ]]; then
  export CC=aarch64-linux-gnu-gcc
  export CXX=aarch64-linux-gnu-g++
  export LD=aarch64-linux-gnu-ld
  aname=arm64
else
  echo "unsupported platform $1"
  exit 1
fi
shift
plat=$1
shift


make -f Makefile.libretro platform=$plat "$@"
zip -9 "pcsx_rearmed_libretro_${aname}_${plat}.zip" pcsx_rearmed_libretro.so
make -f Makefile.libretro platform=$plat clean
