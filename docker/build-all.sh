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

subdirs=$(find "${SCRIPT_DIR}" -mindepth 1 -maxdepth 1 -type d)
for d in ${subdirs}; do
    echo "Building ${d}..."
    "${d}/build.sh" "$@";
done
