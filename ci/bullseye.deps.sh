#!/bin/bash -eux

source ci/common.setup.sh

wget -nv https://github.com/xpack-dev-tools/gcc-xpack/releases/download/v14.2.0-2/xpack-gcc-14.2.0-2-linux-x64.tar.gz
tar xaf xpack-gcc-14.2.0-2-linux-x64.tar.gz
rm xpack-gcc-14.2.0-2-linux-x64.tar.gz
$SUDO mv xpack-gcc-14.2.0-2 /opt/gcc-14

echo 'deb http://deb.debian.org/debian bullseye-backports main' | $SUDO tee -a /etc/apt/sources.list
# libsdl2-dev libsdl2-2.0-0
$SUDO apt-get update -qq
$SUDO apt-get install -qq --force-yes -t bullseye-backports \
     binutils gcc g++ clang-13 cmake \
     libasound-dev \
     ninja-build \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     libclang-13-dev llvm-13-dev \
     libdrm-dev libgbm-dev \
     libdbus-1-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev \
     libqt6opengl6-dev \
     qt6-websockets-dev \
     qt6-serialport-dev \
     qt6-shadertools-dev \
     libbluetooth-dev \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev

source ci/common.deps.sh
