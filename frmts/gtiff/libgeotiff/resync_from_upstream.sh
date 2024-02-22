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

rm -rf tmp_libgeotiff
git clone --depth 1 https://github.com/OSgeo/libgeotiff tmp_libgeotiff
for i in *.c; do
  if test "$i" != "xtiff.c"; then
    echo "Resync $i"
    cp tmp_libgeotiff/libgeotiff/$i .
  fi
done
for i in *.h; do
  if test "$i" != "gdal_libgeotiff_symbol_rename.h" -a "$i" != "geo_config.h" -a "$i" != "cpl_serv.h" -a "$i" != "xtiffio.h"; then
    echo "Resync $i"
    cp tmp_libgeotiff/libgeotiff/$i .
  fi
done

rm -rf tmp_libgeotiff
