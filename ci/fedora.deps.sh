#!/bin/bash -eux

# Needed for ffmpeg-devel
dnf -y install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
dnf -y install https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

# TODO fix SDL2-devel SDL2-static -fPIC issue
dnf -y update # Needed for dnf-5 does not know allowerasing error
dnf -y install dnf5-plugins
dnf -y install --allowerasing \
     clang \
     cmake ninja-build \
     ffmpeg-devel \
     clang-devel llvm-devel \
     alsa-lib-devel \
     portaudio-devel \
     jack-audio-connection-kit-devel \
     lilv-devel suil-devel libquadmath-devel \
     fftw-devel \
     avahi-devel \
     dbus-devel \
     libxkbcommon-x11-devel libxkbcommon-devel \
     bluez-libs-devel \
     qt6-qtbase-devel \
     qt6-qtbase-private-devel \
     qt6-qtscxml-devel \
     qt6-qtshadertools-devel \
     qt6-qtserialport-devel \
     qt6-qttools-devel \
     qt6-qtwebsockets-devel \
     qt6-qtdeclarative-devel \
     qt6-qtdeclarative-private-devel \
     pipewire-devel \
     zlib-ng-compat-static zlib-ng-compat-devel

source ci/common.deps.sh
