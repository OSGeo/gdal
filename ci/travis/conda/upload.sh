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
# anaconda-client is broken with python 3.12 currently. See https://github.com/OSGeo/gdal/actions/runs/6700873032/job/18207482980#step:10:1607 with a No module named 'imp' error
conda install -c conda-forge python=3.11 anaconda-client -y

find . -name "*gdal*.bz2" -exec anaconda -t "$ANACONDA_TOKEN" upload --force --no-progress --user gdal-master  {} \;

