#!/bin/sh
# Ths file is available at the option of the licensee under:
# Public domain
# or licensed under X/MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

set -eu

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac

IMAGE_NAME=osgeo/gdal:alpine-normal-latest

docker build \
    --build-arg PROJ_VERSION=`curl -Ls https://api.github.com/repos/OSGeo/proj.4/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha"` \
    --build-arg GDAL_VERSION=`curl -Ls https://api.github.com/repos/OSGeo/gdal/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha"` \
    -t ${IMAGE_NAME} ${SCRIPT_DIR}

docker run --rm ${IMAGE_NAME} gdalinfo --version
docker run --rm ${IMAGE_NAME} projinfo EPSG:4326
docker run --rm ${IMAGE_NAME} python -c "from osgeo import gdal, gdalnumeric; print(gdal.VersionInfo(''))"
