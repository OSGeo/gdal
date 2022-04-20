#!/bin/env bash
printf "Check out source code tree to match the version of installed gdal binaries\
 and then run all of the autotests...\n"

if ! [ -x "$(command -v git)" ]; then
  echo 'Error: git is not installed.' >&2
  exit 1
elif ! [ -x "$(command -v gdalinfo)" ]; then
  echo 'Error: gdal is not installed.' >&2
  exit 1
fi


# Equiv to:
#     $ gdalinfo --version
#     GDAL 3.4.0dev-d2f9067ffb15e593e9b826ca939dbd183636c780, released 2021/10/26
#     $ git checkout d2f9067ffb15e593e9b826ca939dbd183636c780
if ! [git checkout `gdalinfo --version | sed -s "s/GDAL.*-\(.*\), .*/\1/"`]; then
  echo 'Error: failed checkout'
  exit 1
 fi

pip install -r ./requirements.txt
pytest
