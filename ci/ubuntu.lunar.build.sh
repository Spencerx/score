#!/bin/bash

mkdir build
(
cd build
cmake .. \
  -GNinja \
  -DSCORE_DEPLOYMENT_BUILD=1 \
  -DBUILD_SHARED_LIBS=0 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_C_FLAGS="-g0 $CXXFLAGS" \
  -DCMAKE_CXX_FLAGS="-g0 $CXXFLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" \
  -DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS" \
  -DCMAKE_MODULE_LINKER_FLAGS="$LDFLAGS" \
  $CMAKEFLAGS

cmake --build .
cmake --build . --target install
cmake --build . --target package

mv *.deb ..
)
