#!/bin/bash -eux

source ci/common.setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

# For newer CMake
$SUDO apt-get install -y ca-certificates gpg wget lsb-release software-properties-common
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | $SUDO tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' | $SUDO tee /etc/apt/sources.list.d/kitware.list >/dev/null
$SUDO apt-get update -qq
$SUDO rm /usr/share/keyrings/kitware-archive-keyring.gpg
$SUDO apt-get install -y kitware-archive-keyring

$SUDO apt-get update -qq
$SUDO apt-get install -y \
    --allow-change-held-packages \
    --allow-downgrades \
    --allow-remove-essential \
    --allow-unauthenticated \
     binutils gcc-14 g++-14 clang-19 lld-19 \
     software-properties-common wget \
     libasound-dev \
     ninja-build cmake \
     libfftw3-dev \
     libdbus-1-dev \
     libsuil-dev liblilv-dev lv2-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev \
     libqt6opengl6-dev \
     libqt6websockets6-dev \
     libqt6serialport6-dev \
     libqt6shadertools6-dev \
     libbluetooth-dev libsdl2-dev libsdl2-2.0-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libclang-19-dev llvm-19-dev \
     libvulkan-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     file \
     dpkg-dev \
     lsb-release

# needed because GCC does not support -fuse-ld=lld-19
$SUDO rm -rf /usr/bin/lld /usr/bin/ld.lld

$SUDO ln -s /usr/bin/lld-19 /usr/bin/lld
$SUDO ln -s /usr/bin/ld.lld-19 /usr/bin/ld.lld

source ci/common.deps.sh
