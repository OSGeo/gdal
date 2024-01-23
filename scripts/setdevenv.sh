#!/bin/bash
#
# This script set ups the environment variables needed for executing the
# GDAL build in this tree, without installing it.
# For a CMake build, the script should be run from the build directory
# typically if the build dir is a subdirectory in the source tree,
# ". ../scripts/setdevenv.sh"
#
# The script can be sourced from either bash or zsh.

# Do *NOT* use set set -e|-u flags as this script is intended to be sourced
# and thus an error emitted will kill the shell.
# set -eu

called=$_

if [[ $BASH_VERSION && $(realpath $called) == $(realpath "$0") ]]; then
    echo "Script should be sourced with '. $0', instead of run."
    exit 1
fi

# The following line uses a zsh expansion that is not supported by shellcheck
# shellcheck disable=SC2296
SETDEVENV_SH=${BASH_SOURCE[0]:-${(%):-%x}}

# SC2164 is "Use cd ... || exit in case cd fails"
# shellcheck disable=SC2164
GDAL_ROOT=$(cd $(dirname ${SETDEVENV_SH})/..; pwd)
CUR_DIR=$PWD

echo "Setting environment for a CMake build from ${CUR_DIR}..."

if [[ ! ${PATH} =~ $CUR_DIR/apps ]]; then
    export PATH="$CUR_DIR/apps:$PATH"
    export PATH="$CUR_DIR/perftests:$PATH"
    export PATH="$GDAL_ROOT/swig/python/gdal-utils/scripts:$PATH"
    echo "Setting PATH=$PATH"
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
    if [[ ! "${DYLD_LIBRARY_PATH:-}" =~ $CUR_DIR ]]; then
        export DYLD_LIBRARY_PATH="$CUR_DIR:${DYLD_LIBRARY_PATH:-}"
        echo "Setting DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH"
    fi
else
    if [[ ! "${LD_LIBRARY_PATH:-}" =~ $CUR_DIR ]]; then
        export LD_LIBRARY_PATH="$CUR_DIR:${LD_LIBRARY_PATH:-}"
        echo "Setting LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    fi
fi

if [[ ! ${GDAL_DRIVER_PATH:-} =~ $CUR_DIR/gdalplugins ]]; then
    export GDAL_DRIVER_PATH="$CUR_DIR/gdalplugins"
    echo "Setting GDAL_DRIVER_PATH=$GDAL_DRIVER_PATH"
fi

if [[ ! "${GDAL_DATA:-}" =~ $CUR_DIR/data ]]; then
    export GDAL_DATA="$CUR_DIR/data"
    echo "Setting GDAL_DATA=$GDAL_DATA"
fi

GDAL_PYTHONPATH="$CUR_DIR/swig/python"
if [[ ! "${PYTHONPATH:-}" =~ $GDAL_PYTHONPATH ]]; then
    export PYTHONPATH="$GDAL_PYTHONPATH:${PYTHONPATH:-}"
    echo "Setting PYTHONPATH=$PYTHONPATH"
fi
unset GDAL_PYTHONPATH
