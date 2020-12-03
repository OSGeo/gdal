#!/bin/bash

mkdir -p packages

CI_SUPPORT=""
if [ "$PLATFORM" == "windows-latest" ]; then
    CI_SUPPORT="win_64_.yaml"
fi

if [ "$PLATFORM" == "ubuntu-latest" ]; then
    CI_SUPPORT="linux_64_.yaml"
fi

if [ "$PLATFORM" == "macos-latest" ]; then
    CI_SUPPORT="osx_64_.yaml"
fi

conda build recipe --clobber-file recipe/recipe_clobber.yaml --output-folder packages -m .ci_support/$CI_SUPPORT
conda install -c ./packages libgdal gdal

gdalinfo --version
