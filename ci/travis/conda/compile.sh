#!/bin/bash

mkdir -p packages

CONDA_PLAT=""
if grep -q "windows" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="win"
    ARCH="64"
fi

if grep -q "ubuntu" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="linux"
    ARCH="64"
fi

if grep -q "macos-14" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="osx"
    ARCH="arm64"
elif grep -q "macos" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="osx"
    ARCH="64"
fi

conda build recipe --clobber-file recipe/recipe_clobber.yaml --output-folder packages -m ".ci_support/${CONDA_PLAT}_${ARCH}_.yaml"
conda create -y -n test -c ./packages/${CONDA_PLAT}-${ARCH} python libgdal gdal
conda deactivate

conda activate test
gdalinfo --version
conda deactivate
