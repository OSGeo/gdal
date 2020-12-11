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

ls
pwd
ls packages
echo "anaconda upload -t \"$SECRETS_TEST\"  -u gdal-master --force packages/$CI_PLAT-64/gdal*.bz2 packages/$CI_PLAT-64/*gdal*.bz2"
anaconda upload -t "$ANACONDA_TOKEN"  -u gdal-master --force packages/$CI_PLAT-64/gdal*.bz2 packages/$CI_PLAT-64/*gdal*.bz2

