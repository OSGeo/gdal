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

echo "Anaconda token is available, attempting to upload"
conda install -c conda-forge python=3.12 anaconda-client -y

find . -name "*gdal*.bz2" -exec anaconda -t "$ANACONDA_TOKEN" upload --force --no-progress --user gdal-master  {} \;

