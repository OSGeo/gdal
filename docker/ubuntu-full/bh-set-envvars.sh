#!/bin/sh
set -eu

if test "${TARGET_ARCH:-}" != ""; then
  if test "${TARGET_ARCH}" = "arm64"; then
      export GCC_ARCH=aarch64
  else
      echo "Unhandled architecture: ${TARGET_ARCH}"
      exit 0
  fi
  export APT_ARCH_SUFFIX=":${TARGET_ARCH}"
  export CC=${GCC_ARCH}-linux-gnu-gcc-11
  export CXX=${GCC_ARCH}-linux-gnu-g++-11
  export WITH_HOST="--host=${GCC_ARCH}-linux-gnu"
else
  export APT_ARCH_SUFFIX=""
  export WITH_HOST=""
  GCC_ARCH="$(uname -m)"
  export GCC_ARCH
fi
