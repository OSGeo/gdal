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
conda build recipe --clobber-file recipe/recipe_clobber.yaml --output-folder packages -m ".ci_support/${CI_PLAT}_64_openssl1.1.1.yaml"
conda create -y -n test -c ./packages/$CI_PLAT-64 libgdal
# used to install python gdal too
conda deactivate
conda activate test
gdalinfo --version
conda deactivate
