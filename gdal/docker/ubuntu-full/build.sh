#!/bin/sh
# This file is available at the option of the licensee under:
# Public domain
# or licensed under X/MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

set -eu

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac

export SCRIPT_DIR
TAG_NAME=$(basename "${SCRIPT_DIR}")
export BASE_IMAGE_NAME=osgeo/gdal:${TAG_NAME}

export TEST_PYTHON=YES
"${SCRIPT_DIR}/../util.sh" "$@"
