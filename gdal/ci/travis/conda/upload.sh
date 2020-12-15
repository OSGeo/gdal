#!/bin/bash


if [ -z "${ANACONDA_TOKEN+x}" ]
then
    echo "Anaconda token is not set, not uploading"
    exit 0;
fi

ls
pwd
find .

if [ -z "${ANACONDA_TOKEN}" ]
then
    echo "Anaconda token is empty, not uploading"
    exit 0;
fi

export CI_PLAT=""
if [ "$PLATFORM" == "windows-latest" ]; then
    CI_PLAT="win"
fi

if [ "$PLATFORM" == "ubuntu-latest" ]; then
    CI_PLAT="linux"
fi

if [ "$PLATFORM" == "macos-latest" ]; then
    CI_PLAT="osx"
fi

echo "Anaconda token is available, attempting to upload"

find . -name "*gdal*.bz2" -exec anaconda -t "$ANACONDA_TOKEN" upload --force --no-progress --user gdal-master  {} \;

