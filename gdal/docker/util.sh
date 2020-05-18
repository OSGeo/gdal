#!/bin/bash
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

usage()
{
    echo "Usage: build.sh [--push] [--tag name] [--gdal tag|sha1|master] [--proj tag|sha1|master] [--release]"
    # Non-documented: --test-python
    echo ""
    echo "--push: push image to Docker hub"
    echo "--tag name: suffix to append to image name. Defaults to 'latest' for non release builds or the GDAL tag name for release builds"
    echo "--gdal tag|sha1|master: GDAL version to use. Defaults to master"
    echo "--proj tag|sha1|master: PROJ version to use. Defaults to master"
    echo "--release. Whether this is a release build.In which case --gdal tag must be used."
    exit 1
}

RELEASE=no
while (( "$#" ));
do
    case "$1" in
        -h|--help)
            usage
        ;;

        --push)
            PUSH_GDAL_DOCKER_IMAGE=yes
            shift
        ;;

        --gdal)
            shift
            GDAL_VERSION="$1"
            shift
        ;;

        --proj)
            shift
            PROJ_VERSION="$1"
            shift
        ;;

        --tag)
            shift
            TAG_NAME="$1"
            shift
        ;;

        --release)
            RELEASE=yes
            shift
        ;;

        --test-python)
            TEST_PYTHON=yes
            shift
        ;;

        # Unknown option
        *)
            echo "Unrecognized option: $1"
            usage
        ;;

    esac
done

if test "${RELEASE}" = "yes"; then
    if test "${GDAL_VERSION}" = ""; then
        echo "--gdal tag must be specified when --release is used."
        exit 1
    fi
    if test "${GDAL_VERSION}" = "master"; then
        echo "--gdal master not allowed when --release is used."
        exit 1
    fi
    if test "${PROJ_VERSION}" = ""; then
        echo "--proj tag|sha1|master must be specified when --release is used."
        exit 1
    fi
    if test "${TAG_NAME}" = ""; then
        TAG_NAME="${GDAL_VERSION}"
    fi
else
    if test "${TAG_NAME}" = ""; then
        TAG_NAME=latest
    fi
fi

check_image()
{
    IMAGE_NAME="$1"
    docker run --rm "${IMAGE_NAME}" gdalinfo --version
    docker run --rm "${IMAGE_NAME}" projinfo EPSG:4326
    if test "x${TEST_PYTHON}" != "x"; then
        docker run --rm "${IMAGE_NAME}" python3 -c "from osgeo import gdal, gdalnumeric; print(gdal.VersionInfo(''))"
    fi
}

# No longer used
cleanup_rsync()
{
    rm -f "${RSYNC_DAEMON_TEMPFILE}"
    if test "${RSYNC_PID}" != ""; then
        kill "${RSYNC_PID}" || /bin/true
    fi
}

# No longer used
trap_error_exit()
{
    echo "Exit on error... clean up"
    cleanup_rsync
    exit 1
}

# No longer used
start_rsync_host()
{
    # If rsync is available then start it as a temporary daemon
    if test "${USE_CACHE:-yes}" = "yes" -a -x "$(command -v rsync)"; then

        RSYNC_SERVER_IP=$(ip -4 -o addr show docker0 | awk '{print $4}' | cut -d "/" -f 1)
        if test "${RSYNC_SERVER_IP}" = ""; then
            exit 1
        fi

        RSYNC_DAEMON_TEMPFILE=$(mktemp)

        # Trap exit
        trap "trap_error_exit" EXIT

        cat <<EOF > "${RSYNC_DAEMON_TEMPFILE}"
[gdal-docker-cache]
        path = $HOME/gdal-docker-cache
        comment = GDAL Docker cache
        hosts allow = ${RSYNC_SERVER_IP}/24
        use chroot = false
        read only = false
EOF
        RSYNC_PORT=23985
        while /bin/true; do
            rsync --port=${RSYNC_PORT} --address="${RSYNC_SERVER_IP}" --config="${RSYNC_DAEMON_TEMPFILE}" --daemon --no-detach &
            RSYNC_PID=$!
            sleep 1
            kill -0 ${RSYNC_PID} 2>/dev/null && break
            echo "Port ${RSYNC_PORT} is in use. Trying next one"
            RSYNC_PORT=$((RSYNC_PORT+1))
        done
        echo "rsync daemon forked as process ${RSYNC_PID} listening on port ${RSYNC_PORT}"

        RSYNC_REMOTE="rsync://${RSYNC_SERVER_IP}:${RSYNC_PORT}/gdal-docker-cache/${BASE_IMAGE_NAME}"
        mkdir -p "$HOME/gdal-docker-cache/${BASE_IMAGE_NAME}/proj"
        mkdir -p "$HOME/gdal-docker-cache/${BASE_IMAGE_NAME}/gdal"
        mkdir -p "$HOME/gdal-docker-cache/${BASE_IMAGE_NAME}/spatialite"
    else
        RSYNC_REMOTE=""
    fi
}

# No longer used
stop_rsync_host()
{
    if test "${RSYNC_REMOTE}" != ""; then
        cleanup_rsync
        trap - EXIT
    fi
}

if test "${BASE_IMAGE_NAME}" = "osgeo/gdal:ubuntu-full"; then
    PROJ_DATUMGRID_LATEST_LAST_MODIFIED=$(curl -Is https://cdn.proj.org/index.html | grep Last-Modified)
else
    PROJ_DATUMGRID_LATEST_LAST_MODIFIED=$(curl -Is http://download.osgeo.org/proj/proj-datumgrid-latest.zip | grep Last-Modified)
fi
echo "Using PROJ_DATUMGRID_LATEST_LAST_MODIFIED=${PROJ_DATUMGRID_LATEST_LAST_MODIFIED}"

if test "${PROJ_VERSION}" = "" -o "${PROJ_VERSION}" = "master"; then
    PROJ_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/PROJ/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
fi
echo "Using PROJ_VERSION=${PROJ_VERSION}"

if test "${GDAL_VERSION}" = "" -o "${GDAL_VERSION}" = "master"; then
    GDAL_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/gdal/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
fi
echo "Using GDAL_VERSION=${GDAL_VERSION}"

IMAGE_NAME="${BASE_IMAGE_NAME}-${TAG_NAME}"
BUILDER_IMAGE_NAME="${IMAGE_NAME}_builder"

if test "${RELEASE}" = "yes"; then

    BUILD_ARGS=(
        "--build-arg" "PROJ_DATUMGRID_LATEST_LAST_MODIFIED=${PROJ_DATUMGRID_LATEST_LAST_MODIFIED}" \
        "--build-arg" "PROJ_VERSION=${PROJ_VERSION}" \
        "--build-arg" "GDAL_VERSION=${GDAL_VERSION}" \
        "--build-arg" "GDAL_BUILD_IS_RELEASE=YES" \
    )

    docker build "${BUILD_ARGS[@]}" --target builder \
        -t "${BUILDER_IMAGE_NAME}" "${SCRIPT_DIR}"
    docker build "${BUILD_ARGS[@]}" -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

    check_image "${IMAGE_NAME}"

    if test "x${PUSH_GDAL_DOCKER_IMAGE}" = "xyes"; then
        docker push "${IMAGE_NAME}"
    fi

else

    OLD_BUILDER_ID=$(docker image ls "${BUILDER_IMAGE_NAME}" -q)
    OLD_IMAGE_ID=$(docker image ls "${IMAGE_NAME}" -q)

    if test "${GDAL_RELEASE_DATE}" = ""; then
        GDAL_RELEASE_DATE=$(date "+%Y%m%d")
    fi
    echo "Using GDAL_RELEASE_DATE=${GDAL_RELEASE_DATE}"

    RSYNC_DAEMON_CONTAINER=gdal_rsync_daemon
    BUILD_NETWORK=docker_build_gdal
    HOST_CACHE_DIR="$HOME/gdal-docker-cache"

    mkdir -p "${HOST_CACHE_DIR}/${BASE_IMAGE_NAME}/proj"
    mkdir -p "${HOST_CACHE_DIR}/${BASE_IMAGE_NAME}/gdal"
    mkdir -p "${HOST_CACHE_DIR}/${BASE_IMAGE_NAME}/spatialite"

    # Start a Docker container that has a rsync daemon, mounting HOST_CACHE_DIR
    if ! docker ps | grep "${RSYNC_DAEMON_CONTAINER}"; then
        RSYNC_DAEMON_IMAGE=osgeo/gdal:gdal_rsync_daemon
        docker rmi "${RSYNC_DAEMON_IMAGE}" 2>/dev/null || /bin/true
        docker build -t "${RSYNC_DAEMON_IMAGE}" - <<EOF
FROM alpine

VOLUME /opt/gdal-docker-cache
EXPOSE 23985

RUN apk add --no-cache rsync \
    && mkdir -p /opt/gdal-docker-cache \
    && echo "[gdal-docker-cache]" > /etc/rsyncd.conf \
    && echo "path = /opt/gdal-docker-cache" >> /etc/rsyncd.conf  \
    && echo "hosts allow = *" >> /etc/rsyncd.conf \
    && echo "read only = false" >> /etc/rsyncd.conf \
    && echo "use chroot = false" >> /etc/rsyncd.conf

CMD rsync --daemon --port 23985 && while sleep 1; do /bin/true; done

EOF

        if ! docker network ls | grep "${BUILD_NETWORK}"; then
            docker network create "${BUILD_NETWORK}"
        fi

        THE_UID=$(id -u "${USER}")
        THE_GID=$(id -g "${USER}")

        docker run -d -u "${THE_UID}:${THE_GID}" --rm \
            -v "${HOST_CACHE_DIR}":/opt/gdal-docker-cache \
            --name "${RSYNC_DAEMON_CONTAINER}" \
            --network "${BUILD_NETWORK}" \
            --network-alias "${RSYNC_DAEMON_CONTAINER}" \
            "${RSYNC_DAEMON_IMAGE}"

    fi

    RSYNC_REMOTE="rsync://${RSYNC_DAEMON_CONTAINER}:23985/gdal-docker-cache/${BASE_IMAGE_NAME}"

    BUILD_ARGS=(
        "--build-arg" "PROJ_DATUMGRID_LATEST_LAST_MODIFIED=${PROJ_DATUMGRID_LATEST_LAST_MODIFIED}" \
        "--build-arg" "PROJ_VERSION=${PROJ_VERSION}" \
        "--build-arg" "GDAL_VERSION=${GDAL_VERSION}" \
        "--build-arg" "GDAL_RELEASE_DATE=${GDAL_RELEASE_DATE}" \
        "--build-arg" "RSYNC_REMOTE=${RSYNC_REMOTE}" \
    )

    docker build  --network "${BUILD_NETWORK}" "${BUILD_ARGS[@]}" --target builder \
        -t "${BUILDER_IMAGE_NAME}" "${SCRIPT_DIR}"

    docker build "${BUILD_ARGS[@]}" -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

    check_image "${IMAGE_NAME}"

    if test "x${IMAGE_NAME}" = "xosgeo/gdal:ubuntu-full-latest"; then
        docker image tag "${IMAGE_NAME}" "osgeo/gdal:latest"
    fi

    if test "x${PUSH_GDAL_DOCKER_IMAGE}" = "xyes"; then
        docker push "${IMAGE_NAME}"
        if test "x${IMAGE_NAME}" = "xosgeo/gdal:ubuntu-full-latest"; then
            docker push osgeo/gdal:latest
        fi
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
