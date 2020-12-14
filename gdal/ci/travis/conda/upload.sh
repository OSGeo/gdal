#!/bin/bash


if [ -z "${ANACONDA_TOKEN+x}" ]
then
    echo "Anaconda token is not set, not uploading"
    exit 0;
fi

if [ -z "${ANACONDA_TOKEN}" ]
then
    echo "Anaconda token is empty, not uploading"
    exit 0;
fi

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

echo "Anaconda token is available, attempting to upload"
anaconda upload -t "$ANACONDA_TOKEN"  -u gdal-master --force packages/$CI_PLAT-64/gdal*.bz2 packages/$CI_PLAT-64/*gdal*.bz2

