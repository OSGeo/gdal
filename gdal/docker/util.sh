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

# Other environment variable: TEST_PYTHON

usage()
{
    echo "Usage: build.sh [--push] [GDAL_tag_name]"
    exit 1
}

for i in "$@"
do
    case $i in
        -h|--help)
            usage
        ;;

        --push)
            PUSH_GDAL_DOCKER_IMAGE=yes
        ;;

        # Unknown option
        -*)
            echo "Unrecognized option: ${i}"
            usage
        ;;

        #Default
        *)
            if test "${GDAL_VERSION}" != ""; then
                echo "too many arguments"
                usage
            fi
            GDAL_VERSION="$1"
        ;;
    esac
done

check_image()
{
    IMAGE_NAME="$1"
    docker run --rm "${IMAGE_NAME}" gdalinfo --version
    docker run --rm "${IMAGE_NAME}" projinfo EPSG:4326
    if test "x${TEST_PYTHON}" != "x"; then
        docker run --rm "${IMAGE_NAME}" python -c "from osgeo import gdal, gdalnumeric; print(gdal.VersionInfo(''))"
    fi
}

if test "${GDAL_VERSION}" != ""; then
    IMAGE_NAME="${BASE_IMAGE_NAME}-${GDAL_VERSION}"
    BUILDER_IMAGE_NAME="${IMAGE_NAME}_builder"
    if test "x${PROJ_VERSION}" = "x"; then
        PROJ_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/proj.4/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    fi
    docker build \
        --build-arg PROJ_VERSION="${PROJ_VERSION}" \
        --build-arg GDAL_VERSION="${GDAL_VERSION}" \
        --build-arg GDAL_BUILD_IS_RELEASE=YES \
        --target builder \
        -t "${BUILDER_IMAGE_NAME}" "${SCRIPT_DIR}"
    docker build \
        --build-arg PROJ_VERSION="${PROJ_VERSION}" \
        --build-arg GDAL_VERSION="${GDAL_VERSION}" \
        --build-arg GDAL_BUILD_IS_RELEASE=YES \
        -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

    check_image "${IMAGE_NAME}"

else

    IMAGE_NAME="${BASE_IMAGE_NAME}-latest"
    BUILDER_IMAGE_NAME="${IMAGE_NAME}_builder"
    OLD_BUILDER_ID=$(docker image ls "${BUILDER_IMAGE_NAME}" -q)
    OLD_IMAGE_ID=$(docker image ls "${IMAGE_NAME}" -q)

    PROJ_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/proj.4/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    GDAL_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/gdal/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    GDAL_RELEASE_DATE=$(date "+%Y%m%d")
    docker build \
        --build-arg PROJ_VERSION="${PROJ_VERSION}" \
        --build-arg GDAL_VERSION="${GDAL_VERSION}" \
        --build-arg GDAL_RELEASE_DATE="${GDAL_RELEASE_DATE}" \
        --target builder \
        -t "${BUILDER_IMAGE_NAME}" "${SCRIPT_DIR}"
    docker build \
        --build-arg PROJ_VERSION="${PROJ_VERSION}" \
        --build-arg GDAL_VERSION="${GDAL_VERSION}" \
        --build-arg GDAL_RELEASE_DATE="${GDAL_RELEASE_DATE}" \
        -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

    check_image "${IMAGE_NAME}"

    if test "x${PUSH_GDAL_DOCKER_IMAGE}" = "xyes"; then
        docker push "${IMAGE_NAME}"
    fi

    # Cleanup previous images
    NEW_BUILDER_ID=$(docker image ls "${BUILDER_IMAGE_NAME}" -q)
    NEW_IMAGE_ID=$(docker image ls "${IMAGE_NAME}" -q)
    if test "${OLD_BUILDER_ID}" != "" -a  "${OLD_BUILDER_ID}" != "${NEW_BUILDER_ID}"; then
        docker rmi "${OLD_BUILDER_ID}"
    fi
    if test "${OLD_IMAGE_ID}" != "" -a  "${OLD_IMAGE_ID}" != "${NEW_IMAGE_ID}"; then
        docker rmi "${OLD_IMAGE_ID}"
    fi
fi
