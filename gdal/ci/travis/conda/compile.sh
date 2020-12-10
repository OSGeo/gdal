#!/bin/bash

mkdir -p packages

CI_PLAT=""
if [ "$PLATFORM" == "windows-latest" ]; then
    CI_PLAT="win"
fi

if [ "$PLATFORM" == "ubuntu-latest" ]; then
    CI_PLAT="linux"
fi

if [ "$PLATFORM" == "macos-latest" ]; then
    CI_PLAT="osx"
fi

conda build recipe --clobber-file recipe/recipe_clobber.yaml --output-folder packages -m ".ci_support/${CI_PLAT}_64_.yaml"
conda install -c ./packages libgdal gdal

gdalinfo --version
