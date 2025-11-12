#!/bin/sh
# This file is available at the option of the licensee under:
# Public domain
# or licensed under MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

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

export SCRIPT_DIR
TAG_NAME=$(basename "${SCRIPT_DIR}")
export TARGET_IMAGE=${TARGET_IMAGE:-osgeo/gdal:${TAG_NAME}}

HAS_PLATFORM=0
if echo "$*" | grep "\-\-platform" > /dev/null; then
  HAS_PLATFORM=1
fi

HAS_RELEASE=0
if echo "$*" | grep "\-\-release" > /dev/null; then
  HAS_RELEASE=1
fi

HAS_PUSH=0
if echo "$*" | grep "\-\-push" > /dev/null; then
  HAS_PUSH=1
fi

"${SCRIPT_DIR}/../util.sh" "$@" --test-python

if test "${HAS_PLATFORM}" = "0" && test "${HAS_RELEASE}" = "0" && test "${TARGET_IMAGE}" = "osgeo/gdal:ubuntu-small"; then
 "${SCRIPT_DIR}/../util.sh" --platform linux/arm64 "$@" --test-python

  if test "${HAS_PUSH}" = "1" && test -z "${CI}"; then
   DOCKER_REPO=$(cat /tmp/gdal_docker_repo.txt)

   docker manifest rm ${DOCKER_REPO}/${TARGET_IMAGE}-latest || /bin/true
   docker buildx imagetools create -t ${DOCKER_REPO}/${TARGET_IMAGE}-latest \
   ${DOCKER_REPO}/${TARGET_IMAGE}-latest-amd64 \
   ${DOCKER_REPO}/${TARGET_IMAGE}-latest-arm64
 fi
fi
