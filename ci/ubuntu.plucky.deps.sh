#!/bin/bash -eux

source ci/common.setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

# Plucky has clang-20.1.2 which has an unfixed crash when building LLFIO.
# It is necessary to download clang from upstream which has
# 20.1.8 with the crash fixed.
$SUDO apt-get update -qq
$SUDO apt-get install -qq --force-yes wget lsb-release software-properties-common gnupg
wget -nv https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
$SUDO ./llvm.sh 20

$SUDO apt-get update -qq
$SUDO apt-get install -y \
    --allow-change-held-packages \
    --allow-downgrades \
    --allow-remove-essential \
    --allow-unauthenticated \
     binutils gcc g++ \
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
     qt6-serialport-dev \
     qt6-shadertools-dev \
     libbluetooth-dev libsdl2-dev libsdl2-2.0-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libclang-dev llvm-dev \
     libvulkan-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     file \
     dpkg-dev \
     lsb-release

source ci/common.deps.sh
