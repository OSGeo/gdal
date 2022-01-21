#!/bin/bash

mkdir -p packages

CI_PLAT=""
if grep -q "windows" <<< "$PLATFORM"; then
    CI_PLAT="win"
fi

if grep -q "ubuntu" <<< "$PLATFORM"; then
    CI_PLAT="linux"
fi

if grep -q "macos" <<< "$PLATFORM"; then
    CI_PLAT="osx"
fi



export GDAL_ENABLE_DEPRECATED_DRIVER_DODS=YES
conda build recipe --clobber-file recipe/recipe_clobber.yaml --output-folder packages -m ".ci_support/${CI_PLAT}_64_openssl3.yaml"
conda create -y -n test -c ./packages python=3.8 libgdal gdal
conda deactivate
conda activate test
gdalinfo --version
conda deactivate
