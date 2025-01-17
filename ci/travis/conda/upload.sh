#!/bin/bash

set -e

if [ -z "${ANACONDA_TOKEN+x}" ]
then
    echo "Anaconda token is not set!"
    exit 1
fi

ls
pwd
find .

if [[ -n $(find . -name "*gdal*.conda") ]]; then
  echo "Found packages to upload"
else
  echo "No packages matching *gdal*.conda to upload found"
  exit 1
fi

echo "Anaconda token is available, attempting to upload"
conda install -c conda-forge python=3.12 anaconda-client -y

find . -name "*gdal*.conda" -exec anaconda -t "$ANACONDA_TOKEN" upload --force --no-progress --user gdal-master  {} \;

