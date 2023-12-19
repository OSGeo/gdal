#!/bin/sh

set -eu

# Limit virtual memory to 2 GB
ulimit -v 2000000

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

GDAL_RUN_SLOW_TESTS=YES python3 -m pytest $SCRIPT_DIR/slow_tests --capture=no -ra -vv
