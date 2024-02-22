#!/bin/sh

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
cd "${SCRIPT_DIR}"

rm -rf tmp_libtiff
git clone --depth 1 https://gitlab.com/libtiff/libtiff tmp_libtiff
(cd tmp_libtiff; ./autogen.sh; ./configure)
for i in *.c; do
  if test "$i" != "tif_vsi.c"; then
    echo "Resync $i"
    cp tmp_libtiff/libtiff/$i .
  fi
done
for i in *.h; do
  if test "$i" != "gdal_libtiff_symbol_rename.h" -a "$i" != "tif_config.h" -a "$i" != "tiffconf.h"; then
    echo "Resync $i"
    cp tmp_libtiff/libtiff/$i .
  fi
done

rm -rf tmp_libtiff
