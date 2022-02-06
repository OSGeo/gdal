#!/bin/bash
#
# This script set ups the environment variables needed for executing the
# GDAL build in this tree, without installing it.
# For a CMake build, the script should be run from the build directory
# typically if the build dir is a subdirectory in the source tree,
# ". ../scripts/setdevenv.sh"

# Do *NOT* use set set -e|-u flags as this script is intended to be sourced
# and thus an error emitted will kill the shell.
# set -eu

called=$_

if [[ $called == "$0" ]]; then
    echo "Script should be sourced with '. $0', instead of run."
    exit 1
fi

# SC2164 is "Use cd ... || exit in case cd fails"
# shellcheck disable=SC2164
GDAL_ROOT=$(cd $(dirname ${BASH_SOURCE[0]})/..; pwd)
CUR_DIR=$PWD

if test -f "${CUR_DIR}/CMakeCache.txt"; then

  echo "Setting environment for a CMake build from ${CUR_DIR}..."

  if [[ ! ${PATH} =~ $CUR_DIR/apps ]]; then
      export PATH="$CUR_DIR/apps:$PATH"
      export PATH="$GDAL_ROOT/swig/python/gdal-utils/scripts:$PATH"
      echo "Setting PATH=$PATH"
  fi

  if [[ "$(uname -s)" == "Darwin" ]]; then
      if [[ ! "${DYLD_LIBRARY_PATH}" =~ $CUR_DIR ]]; then
          export DYLD_LIBRARY_PATH="$CUR_DIR:$DYLD_LIBRARY_PATH"
          echo "Setting DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH"
      fi
  else
      if [[ ! "${LD_LIBRARY_PATH}" =~ $CUR_DIR ]]; then
          export LD_LIBRARY_PATH="$CUR_DIR:$LD_LIBRARY_PATH"
          echo "Setting LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
      fi
  fi

  if [[ ! ${GDAL_DRIVER_PATH} =~ $CUR_DIR/gdalplugins ]]; then
      export GDAL_DRIVER_PATH="$CUR_DIR/gdalplugins"
      echo "Setting GDAL_DRIVER_PATH=$GDAL_DRIVER_PATH"
  fi

  if [[ ! "${GDAL_DATA}" =~ $GDAL_ROOT/data ]]; then
      export GDAL_DATA="$GDAL_ROOT/data"
      echo "Setting GDAL_DATA=$GDAL_DATA"
  fi

  GDAL_PYTHONPATH="$CUR_DIR/swig/python"
  if [[ ! "${PYTHONPATH}" =~ $GDAL_PYTHONPATH ]]; then
      export PYTHONPATH="$GDAL_PYTHONPATH:$PYTHONPATH"
      echo "Setting PYTHONPATH=$PYTHONPATH"
  fi
  unset GDAL_PYTHONPATH

else

  echo "Setting environment for a autoconf in-source-tree build..."

  if [[ ! ${PATH} =~ $GDAL_ROOT/apps ]]; then
      export PATH="$GDAL_ROOT/apps:$GDAL_ROOT/apps/.libs:$PATH"
      export PATH="$GDAL_ROOT/swig/python/gdal-utils/scripts:$PATH"
      echo "Setting PATH=$PATH"
  fi

  if [[ "$(uname -s)" == "Darwin" ]]; then
      if [[ ! "${DYLD_LIBRARY_PATH}" =~ $GDAL_ROOT ]]; then
          export DYLD_LIBRARY_PATH="$GDAL_ROOT:$GDAL_ROOT/.libs:$DYLD_LIBRARY_PATH"
          echo "Setting DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH"
      fi
  else
      if [[ ! "${LD_LIBRARY_PATH}" =~ $GDAL_ROOT ]]; then
          export LD_LIBRARY_PATH="$GDAL_ROOT:$GDAL_ROOT/.libs:$LD_LIBRARY_PATH"
          echo "Setting LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
      fi
  fi

  if [[ ! "${GDAL_DATA}" =~ $GDAL_ROOT/data ]]; then
      export GDAL_DATA="$GDAL_ROOT/data"
      echo "Setting GDAL_DATA=$GDAL_DATA"
  fi

  if command -v python >/dev/null; then
      GDAL_PYTHONPATH=$(python -c "from distutils.command.build import build;from distutils.dist import Distribution;b = build(Distribution());b.finalize_options();print(b.build_platlib)")
  elif command -v python3 >/dev/null; then
      GDAL_PYTHONPATH=$(python3 -c "from distutils.command.build import build;from distutils.dist import Distribution;b = build(Distribution());b.finalize_options();print(b.build_platlib)")
  fi
  if test "$GDAL_PYTHONPATH" != ""; then
      GDAL_PYTHONPATH="$GDAL_ROOT/swig/python/$GDAL_PYTHONPATH:$GDAL_ROOT/swig/python/gdal-utils"
      if [[ ! "${PYTHONPATH}" =~ $GDAL_PYTHONPATH ]]; then
          export PYTHONPATH="$GDAL_PYTHONPATH:$PYTHONPATH"
          echo "Setting PYTHONPATH=$PYTHONPATH"
      fi
      unset GDAL_PYTHONPATH
  fi

fi
