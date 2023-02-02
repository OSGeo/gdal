#!/bin/bash

mkdir -p packages

CONDA_PLAT=""
if grep -q "windows" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="win"
fi

if grep -q "ubuntu" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="linux"
fi

if grep -q "macos" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="osx"
fi

conda build recipe --clobber-file recipe/recipe_clobber.yaml --output-folder packages -m ".ci_support/${CONDA_PLAT}_64_.yaml"
conda create -y -n test -c ./packages/${CONDA_PLAT}-64 python libgdal gdal
conda deactivate

conda activate test
gdalinfo --version
conda deactivate
