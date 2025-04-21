#!/bin/bash
export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/build/SDK"

# Copy windows binary
(
  cd build
  ls
  mv ossia\ score-*-win64.exe "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$GITTAGNOV-win64.exe"
)

# Create SDK files
(
  cd build
  cmake --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)

./ci/create-sdk-mingw.sh

# Copy SDK
(
    cd build/SDK
    7z a "$BUILD_ARTIFACTSTAGINGDIRECTORY/sdk-windows-x86_64.zip" usr
)
