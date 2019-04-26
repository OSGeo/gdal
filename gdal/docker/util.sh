#!/bin/sh
# This file is available at the option of the licensee under:
# Public domain
# or licensed under X/MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

set -e

if test "x${SCRIPT_DIR}" = "x"; then
    echo "SCRIPT_DIR not defined"
    exit 1
fi

if test "x${BASE_IMAGE_NAME}" = "x"; then
    echo "BASE_IMAGE_NAME not defined"
    exit 1
fi

if test "$#" -ge 1; then
    if test "$1" = "-h" -o "$1" = "--help"; then
        echo "Usage: build.sh [GDAL_tag_name]"
        exit 1
    fi
    GDAL_VERSION="$1"
    IMAGE_NAME="${BASE_IMAGE_NAME}-${GDAL_VERSION}"
    if test "x${PROJ_VERSION}" = "x"; then
        PROJ_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/proj.4/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    fi
    docker build \
        --build-arg PROJ_VERSION="${PROJ_VERSION}" \
        --build-arg GDAL_VERSION="${GDAL_VERSION}" \
        --build-arg GDAL_BUILD_IS_RELEASE=YES \
        -t "${IMAGE_NAME}" "${SCRIPT_DIR}"
else
    IMAGE_NAME="${BASE_IMAGE_NAME}-latest"
    PROJ_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/proj.4/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    GDAL_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/gdal/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    GDAL_RELEASE_DATE=$(date "+%Y%m%d")
    docker build \
        --build-arg PROJ_VERSION="${PROJ_VERSION}" \
        --build-arg GDAL_VERSION="${GDAL_VERSION}" \
        --build-arg GDAL_RELEASE_DATE="${GDAL_RELEASE_DATE}" \
        -t "${IMAGE_NAME}" "${SCRIPT_DIR}"
fi

docker run --rm "${IMAGE_NAME}" gdalinfo --version
docker run --rm "${IMAGE_NAME}" projinfo EPSG:4326
if test "x${TEST_PYTHON}" != "x"; then
    docker run --rm "${IMAGE_NAME}" python -c "from osgeo import gdal, gdalnumeric; print(gdal.VersionInfo(''))"
fi
