#!/bin/bash


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

anaconda -t $ANACONDA_TOKEN upload -u gdal-master --force packages/$CI_PLAT-64/gdal*.bz2 packages/$CI_PLAT-64/*gdal*.bz2

