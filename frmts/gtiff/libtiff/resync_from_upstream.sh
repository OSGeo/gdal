#!/bin/sh

rm -rf tmp_libtiff
git clone --depth 1 https://gitlab.com/libtiff/libtiff tmp_libtiff
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
