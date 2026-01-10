#!/bin/bash
# This file is available at the option of the licensee under:
# Public domain
# or licensed under MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

set -e

if test "${SCRIPT_DIR}" = ""; then
    echo "SCRIPT_DIR not defined"
    exit 1
fi

if test "${TARGET_IMAGE}" = ""; then
    echo "TARGET_IMAGE not defined"
    exit 1
fi

usage()
{
    echo "Usage: build.sh [--push] [--docker-repository repo] [--tag name] [--gdal tag|sha1|master] [--gdal-repository repo] [--proj tag|sha1|master] [--release] [--docker-cache|--no-docker-cache] [--no-rsync-daemon]"
    # Non-documented: --test-python
    echo ""
    echo "--push: push image to docker repository (defaults to ghcr.io)"
    echo "--tag name: suffix to append to image name. Defaults to 'latest' for non release builds or the GDAL tag name for release builds"
    echo "--gdal tag|sha1|master: GDAL version to use. Defaults to master"
    echo "--proj tag|sha1|master: PROJ version to use. Defaults to master"
    echo "--gdal-repository repo: github repository. Defaults to OSGeo/gdal"
    echo "--release: Whether this is a release build. In which case --gdal tag must be used."
    echo "--docker-cache/--no-docker-cache: instruct Docker to build with/without using its cache. Defaults to no cache for release builds."
    echo "--no-rsync-daemon: do not use the rsync daemon to save build cache in home directory."
    echo "--with-debug-symbols/--without-debug-symbols. Whether to include debug symbols. Only applies to ubuntu-full, default is to include for non-release builds."
    echo "--with-oracle: Whether to include Oracle Instant Client proprietary SDK"
    echo "--with-ecw: Whether to include ECW proprietary SDK"
    echo "--with-mrsid: Whether to include MrSID proprietary SDK"
    exit 1
}

RELEASE=no
ARCH_PLATFORMS="linux/amd64"
DOCKER_REPO=ghcr.io

GDAL_REPOSITORY=OSGeo/gdal

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

        --docker-repository)
            shift
            DOCKER_REPO="$1"
            shift
        ;;

        --gdal)
            shift
            GDAL_VERSION="$1"
            shift
        ;;

        --gdal-repository)
            shift
            GDAL_REPOSITORY="$1"
            shift
        ;;

        --platform)
            shift
            ARCH_PLATFORMS="$1"
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

        --with-debug-symbols)
            WITH_DEBUG_SYMBOLS=yes
            shift
        ;;

        --with-multi-arch)
            DOCKER_BUILDKIT=1
            DOCKER_CLI_EXPERIMENTAL=enabled
            shift
        ;;

        --docker-cache)
            DOCKER_CACHE_PARAM=""
            shift
        ;;
        --no-docker-cache)
            DOCKER_CACHE_PARAM="--no-cache"
            shift
        ;;

        --no-rsync-daemon)
            NO_RSYNC_DAEMON=1
            shift
        ;;

        --without-debug-symbols)
            WITH_DEBUG_SYMBOLS=no
            shift
        ;;

        --test-python)
            TEST_PYTHON=yes
            shift
        ;;

        --with-oracle)
            WITH_ORACLE=yes
            shift
        ;;

        --with-ecw)
            WITH_ECW=yes
            shift
        ;;

        --with-mrsid)
            WITH_MRSID=yes
            shift
        ;;

        # Unknown option
        *)
            echo "Unrecognized option: $1"
            usage
        ;;

    esac
done

echo "${DOCKER_REPO}" > /tmp/gdal_docker_repo.txt

if test "${DOCKER_BUILDKIT}" = "1" && test "${DOCKER_CLI_EXPERIMENTAL}" = "enabled"; then
  DOCKER_BUILDX="buildx"
  DOCKER_BUILDX_ARGS=("--platform" "${ARCH_PLATFORMS}")
  if [ -n "${CI}" ]; then
     DOCKER_BUILDX_ARGS+=("--sbom" "true" "--provenance" "true")
  fi
fi

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
    if test "${WITH_DEBUG_SYMBOLS}" = ""; then
        WITH_DEBUG_SYMBOLS=no
    fi
    [ -v DOCKER_CACHE_PARAM ] || DOCKER_CACHE_PARAM="--no-cache"
else
    if test "${TAG_NAME}" = ""; then
        TAG_NAME=latest
    fi
    if test "${WITH_DEBUG_SYMBOLS}" = ""; then
        WITH_DEBUG_SYMBOLS=yes
    fi
    [ -v DOCKER_CACHE_PARAM ] || DOCKER_CACHE_PARAM=""
fi

check_image()
{
    TMP_IMAGE_NAME="$1"
    docker run --rm "${TMP_IMAGE_NAME}" gdalinfo --version
    docker run --rm "${TMP_IMAGE_NAME}" projinfo EPSG:4326
    if test "${TEST_PYTHON}" != ""; then
        docker run --rm "${TMP_IMAGE_NAME}" python3 -c "from osgeo import gdal, gdalnumeric; print(gdal.VersionInfo(''))"
    fi
}

build_cmd()
{
    if test "${DOCKER_BUILDX}" = "buildx"; then
        echo "${DOCKER_BUILDX}" build "${DOCKER_BUILDX_ARGS[@]}"
    else
        echo build
    fi
}

if test "${TARGET_IMAGE}" = "osgeo/gdal:ubuntu-full"; then
    PROJ_DATUMGRID_LATEST_LAST_MODIFIED=$(curl -Is https://cdn.proj.org/index.html | grep -i Last-Modified)
else
    PROJ_DATUMGRID_LATEST_LAST_MODIFIED=$(curl -Is http://download.osgeo.org/proj/proj-datumgrid-latest.zip | grep -i Last-Modified)
fi
echo "Using PROJ_DATUMGRID_LATEST_LAST_MODIFIED=${PROJ_DATUMGRID_LATEST_LAST_MODIFIED}"

if test "${PROJ_VERSION}" = "" || test "${PROJ_VERSION}" = "master"; then
    PROJ_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/PROJ/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
fi
echo "Using PROJ_VERSION=${PROJ_VERSION}"

if test "${GDAL_VERSION}" = "" || test "${GDAL_VERSION}" = "master"; then
    GDAL_VERSION=$(curl -Ls "https://api.github.com/repos/${GDAL_REPOSITORY}/commits/HEAD" -H "Accept: application/vnd.github.VERSION.sha")
fi
echo "Using GDAL_VERSION=${GDAL_VERSION}"
echo "Using GDAL_REPOSITORY=${GDAL_REPOSITORY}"

IMAGE_NAME="${TARGET_IMAGE}-${TAG_NAME}"
REPO_IMAGE_NAME="${DOCKER_REPO}/${IMAGE_NAME}"
ARCH_PLATFORM_ARCH=$(echo "${ARCH_PLATFORMS}" | sed "s/linux\///")


BUILD_ARGS=(
    "--build-arg" "PROJ_DATUMGRID_LATEST_LAST_MODIFIED=${PROJ_DATUMGRID_LATEST_LAST_MODIFIED}" \
    "--build-arg" "PROJ_VERSION=${PROJ_VERSION}" \
    "--build-arg" "GDAL_VERSION=${GDAL_VERSION}" \
    "--build-arg" "GDAL_REPOSITORY=${GDAL_REPOSITORY}" \
    "--build-arg" "WITH_DEBUG_SYMBOLS=${WITH_DEBUG_SYMBOLS}" \
)
[ -z "${DOCKER_CACHE_PARAM}" ] || BUILD_ARGS+=("${DOCKER_CACHE_PARAM}")

if test "${WITH_ORACLE}" != ""; then
      BUILD_ARGS+=("--build-arg" "WITH_ORACLE=${WITH_ORACLE}")
fi

if test "${WITH_ECW}" != ""; then
      BUILD_ARGS+=("--build-arg" "WITH_ECW=${WITH_ECW}")
fi

if test "${WITH_MRSID}" != ""; then
      BUILD_ARGS+=("--build-arg" "WITH_MRSID=${WITH_MRSID}")
fi

if test "${WITH_PROJ_GRIDS}" != ""; then
  BUILD_ARGS+=("--build-arg" "$WITH_PROJ_GRIDS=${WITH_PROJ_GRIDS}")
fi

if test "${RELEASE}" = "yes"; then
    BUILD_ARGS+=("--build-arg" "GDAL_BUILD_IS_RELEASE=YES")

    if [[ -z "${SOURCE_DATE_EPOCH}" ]]; then
        # Try to set SOURCE_DATE_EPOCH to the timestamp of the tar.gz for
        # the release, so repeated builds give similar output.
        # https://github.com/moby/buildkit/blob/master/docs/build-repro.md#source_date_epoch
        # Will proceed without setting SOURCE_DATE_EPOCH when failing to get
        # the timestamp, so "build.sh --gdal HEAD" works.
        LAST_MODIFIED=$(curl -L -sI "https://github.com/OSGeo/gdal/releases/download/${GDAL_VERSION}/gdal-${GDAL_VERSION#v}.tar.gz" \
                            | grep -i last-modified | awk -F: '{print $2}')
        if [ -n "${LAST_MODIFIED}" ]; then
            MODIFIED_SINCE_EPOCH=$(date -d "${LAST_MODIFIED}" +%s)
            if [ -n "${MODIFIED_SINCE_EPOCH}" ]; then
                export SOURCE_DATE_EPOCH="${MODIFIED_SINCE_EPOCH}"
            fi
        fi
    fi

    if test "${BASE_IMAGE}" != ""; then
        BUILD_ARGS+=("--build-arg" "BASE_IMAGE=${BASE_IMAGE}")
        if test "${TARGET_IMAGE}" = "osgeo/gdal:ubuntu-full" || test "${TARGET_IMAGE}" = "osgeo/gdal:ubuntu-small"; then
          BUILD_ARGS+=("--build-arg" "TARGET_BASE_IMAGE=${BASE_IMAGE}")
        fi
    fi

    LABEL_ARGS=(
       "--label" "org.opencontainers.image.description=GDAL is an open source MIT licensed translator library for raster and vector geospatial data formats." \
       "--label" "org.opencontainers.image.title=GDAL ${TARGET_IMAGE}" \
       "--label" "org.opencontainers.image.licenses=MIT" \
       "--label" "org.opencontainers.image.source=https://github.com/${GDAL_REPOSITORY}" \
       "--label" "org.opencontainers.image.url=https://github.com/${GDAL_REPOSITORY}" \
       "--label" "org.opencontainers.image.revision=${GDAL_VERSION}" \
       "--label" "org.opencontainers.image.version=${TAG_NAME}" \
    )
    IMAGE_NAME_WITH_ARCH="${REPO_IMAGE_NAME}-${ARCH_PLATFORM_ARCH}"
    if test "${DOCKER_BUILDX}" = "buildx"; then
      if test "${PUSH_GDAL_DOCKER_IMAGE}" = "yes"; then
        docker $(build_cmd) "${BUILD_ARGS[@]}" "${LABEL_ARGS[@]}" -t "${IMAGE_NAME_WITH_ARCH}" --push "${SCRIPT_DIR}"
      else
        docker $(build_cmd) "${BUILD_ARGS[@]}" "${LABEL_ARGS[@]}" -t "${IMAGE_NAME_WITH_ARCH}" --load "${SCRIPT_DIR}"
      fi
    else

        docker $(build_cmd) "${BUILD_ARGS[@]}" "${LABEL_ARGS[@]}" -t "${IMAGE_NAME_WITH_ARCH}" "${SCRIPT_DIR}"
        check_image "${IMAGE_NAME_WITH_ARCH}"

        if test "${PUSH_GDAL_DOCKER_IMAGE}" = "yes"; then
            docker push "${IMAGE_NAME_WITH_ARCH}"
        fi
    fi

else
    BUILD_ARGS+=("--build-arg" "BUILDKIT_INLINE_CACHE=1")
    BUILD_ARGS+=("--build-arg" "WITH_CCACHE=1")

    IMAGE_NAME_WITH_ARCH="${REPO_IMAGE_NAME}"
    # If building for a single architecture, include the architecture name
    # as a suffix in the image name.
    if test "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-full-latest" || \
        test "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-small-latest" || \
        test "${IMAGE_NAME}" = "osgeo/gdal:alpine-small-latest" || \
        test "${IMAGE_NAME}" = "osgeo/gdal:alpine-normal-latest"; then
        if test "${DOCKER_BUILDX}" != "buildx" || (echo ${ARCH_PLATFORMS} | grep -v -q ','); then
          ARCH_PLATFORM_ARCH=$(echo "${ARCH_PLATFORMS}" | sed "s/linux\///")
          IMAGE_NAME_WITH_ARCH="${REPO_IMAGE_NAME}-${ARCH_PLATFORM_ARCH}"
        fi
    fi

    OLD_IMAGE_ID=$(docker image ls "${IMAGE_NAME_WITH_ARCH}" -q)

    if test "${GDAL_RELEASE_DATE}" = ""; then
        GDAL_RELEASE_DATE=$(date "+%Y%m%d")
    fi
    echo "Using GDAL_RELEASE_DATE=${GDAL_RELEASE_DATE}"
    BUILD_ARGS+=("--build-arg" "GDAL_RELEASE_DATE=${GDAL_RELEASE_DATE}")

    if [[ -z ${NO_RSYNC_DAEMON} ]]; then
        RSYNC_DAEMON_CONTAINER=gdal_rsync_daemon
        HOST_CACHE_DIR="$HOME/gdal-docker-cache"

        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/proj/"{x86_64,aarch64}
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/gdal/"{x86_64,aarch64}
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/kealib/"{x86_64,aarch64}
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/libjxl/"{x86_64,aarch64}
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/libopendrive/"{x86_64,aarch64}
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/libqb3/"{x86_64,aarch64}
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/"mongo-{c,cxx}-driver/{x86_64,aarch64}
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/spatialite"
        mkdir -p "${HOST_CACHE_DIR}/${TARGET_IMAGE}/tiledb/"{x86_64,aarch64}

        # Start a Docker container that has a rsync daemon, mounting HOST_CACHE_DIR
        if ! docker ps | grep "${RSYNC_DAEMON_CONTAINER}"; then
            RSYNC_DAEMON_IMAGE=osgeo/gdal:gdal_rsync_daemon
            docker rmi "${RSYNC_DAEMON_IMAGE}" 2>/dev/null || true
            docker $(build_cmd) -t "${RSYNC_DAEMON_IMAGE}" - <<EOF
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

CMD rsync --daemon --port 23985 && while sleep 1; do true; done

EOF

            THE_UID=$(id -u "${USER}")
            THE_GID=$(id -g "${USER}")

            docker run -d -u "${THE_UID}:${THE_GID}" --rm \
                   -v "${HOST_CACHE_DIR}":/opt/gdal-docker-cache \
                   --name "${RSYNC_DAEMON_CONTAINER}" \
                   --network host \
                   "${RSYNC_DAEMON_IMAGE}"
        fi
        RSYNC_REMOTE="rsync://127.0.0.1:23985/gdal-docker-cache/${TARGET_IMAGE}"
        BUILD_ARGS+=(
            "--build-arg" "RSYNC_REMOTE=${RSYNC_REMOTE}" \
            "--network" "host" \
        )
    fi

    if test "${BASE_IMAGE}" != ""; then
        BUILD_ARGS+=("--build-arg" "BASE_IMAGE=${BASE_IMAGE}")
        if test "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-full-latest" || test "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-small-latest"; then
          BUILD_ARGS+=("--build-arg" "TARGET_BASE_IMAGE=${BASE_IMAGE}")
        fi
    else
      if test "${DOCKER_BUILDX}" != "buildx" && [[ "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-full-latest" || "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-small-latest" ]]; then
        if test "${ARCH_PLATFORMS}" = "linux/arm64"; then
          BASE_IMAGE=$(grep "ARG BASE_IMAGE=" "${SCRIPT_DIR}/Dockerfile" | sed "s/ARG BASE_IMAGE=//")
          echo "Fetching digest for ${BASE_IMAGE} ${ARCH_PLATFORMS}..."
          ARCH_PLATFORM_ARCH=$(echo "${ARCH_PLATFORMS}" | sed "s/linux\///")
          TARGET_BASE_IMAGE_DIGEST=$(docker manifest inspect "${BASE_IMAGE}" | jq --raw-output '.manifests[] | (if .platform.architecture == "'"${ARCH_PLATFORM_ARCH}"'" then .digest else empty end)')
          docker pull "${BASE_IMAGE}@${TARGET_BASE_IMAGE_DIGEST}"
          BUILD_ARGS+=("--build-arg" "TARGET_ARCH=${ARCH_PLATFORM_ARCH}")
          BUILD_ARGS+=("--build-arg" "TARGET_BASE_IMAGE=${BASE_IMAGE}@${TARGET_BASE_IMAGE_DIGEST}")
          echo "${BUILD_ARGS[@]}"
        fi
      elif test "${DOCKER_BUILDX}" != "buildx" && [[ "${IMAGE_NAME}" = "osgeo/gdal:alpine-small-latest" || "${IMAGE_NAME}" = "osgeo/gdal:alpine-normal-latest" ]]; then
        if test "${ARCH_PLATFORMS}" = "linux/arm64"; then
          ALPINE_VERSION=$(grep "ARG ALPINE_VERSION=" "${SCRIPT_DIR}/Dockerfile" | sed "s/ARG ALPINE_VERSION=//")
          BASE_IMAGE="alpine:${ALPINE_VERSION}"
          echo "Fetching digest for ${BASE_IMAGE} ${ARCH_PLATFORMS}..."
          ARCH_PLATFORM_ARCH=$(echo "${ARCH_PLATFORMS}" | sed "s/linux\///")
          TARGET_BASE_IMAGE_DIGEST=$(docker manifest inspect "${BASE_IMAGE}" | jq --raw-output '.manifests[] | (if .platform.architecture == "'"${ARCH_PLATFORM_ARCH}"'" then .digest else empty end)')
          echo "${TARGET_BASE_IMAGE_DIGEST}"
          BUILD_ARGS+=("--build-arg" "ALPINE_VERSION=${ALPINE_VERSION}@${TARGET_BASE_IMAGE_DIGEST}")
          echo "${BUILD_ARGS[@]}"
        fi
      fi
    fi


    if test "${DOCKER_BUILDX}" != "buildx"; then
        docker $(build_cmd) "${BUILD_ARGS[@]}" -t "${IMAGE_NAME_WITH_ARCH}" "${SCRIPT_DIR}"
        check_image "${IMAGE_NAME_WITH_ARCH}"
    else
        docker $(build_cmd) "${BUILD_ARGS[@]}" -t "${IMAGE_NAME_WITH_ARCH}" "${SCRIPT_DIR}" --load
    fi

    if test "${PUSH_GDAL_DOCKER_IMAGE}" = "yes"; then
        if test "${DOCKER_BUILDX}" = "buildx"; then
            docker $(build_cmd) "${BUILD_ARGS[@]}" -t "${IMAGE_NAME_WITH_ARCH}" --push "${SCRIPT_DIR}"
        else
            docker push "${IMAGE_NAME_WITH_ARCH}"
        fi

        if test "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-full-latest"; then
            if test "${DOCKER_BUILDX}" = "buildx"; then
                docker $(build_cmd) "${BUILD_ARGS[@]}" -t "${DOCKER_REPO}/osgeo/gdal:latest" --push "${SCRIPT_DIR}"
            else
                if test "${ARCH_PLATFORMS}" = "linux/amd64"; then
                    docker image tag "${IMAGE_NAME}" "${DOCKER_REPO}/osgeo/gdal:latest"
                    docker push "${DOCKER_REPO}/osgeo/gdal:latest"
                fi
            fi
        fi
    fi

    # Cleanup previous images
    NEW_IMAGE_ID=$(docker image ls "${IMAGE_NAME_WITH_ARCH}" -q)
    if test "${OLD_IMAGE_ID}" != "" && test "${OLD_IMAGE_ID}" != "${NEW_IMAGE_ID}"; then
        docker rmi "${OLD_IMAGE_ID}" --force 2>/dev/null
    fi

    if test "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-full-latest" || \
        test "${IMAGE_NAME}" = "osgeo/gdal:ubuntu-small-latest" || \
        test "${IMAGE_NAME}" = "osgeo/gdal:alpine-small-latest" || \
        test "${IMAGE_NAME}" = "osgeo/gdal:alpine-normal-latest"; then
        if test "${DOCKER_BUILDX}" != "buildx" && test "${ARCH_PLATFORMS}" = "linux/amd64"; then
          docker image tag "${IMAGE_NAME_WITH_ARCH}" "${REPO_IMAGE_NAME}"
        fi
    fi
fi
