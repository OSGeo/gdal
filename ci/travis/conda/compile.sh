#!/bin/bash

mkdir -p packages

CONDA_PLAT=""
if grep -q "windows" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="win"
    ARCH="64"
fi

if grep -q "ubuntu" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="linux"
    if grep -q "arm" <<< "$GHA_CI_PLATFORM"; then
      ARCH="aarch64"
    else
      ARCH="64"
    fi
fi

if grep -q "macos-14" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="osx"
    ARCH="arm64"
elif grep -q "macos-15-intel" <<< "$GHA_CI_PLATFORM"; then
    CONDA_PLAT="osx"
    ARCH="64"
fi

echo "CONDA_SUBDIR=${CONDA_PLAT}-${ARCH}" >> $GITHUB_ENV

# Select the stable channel configuration regardless of generated filename suffixes.
CONDA_VARIANTS=()
for VARIANT in .ci_support/"${CONDA_PLAT}_${ARCH}_"*.yaml; do
    if [ -f "$VARIANT" ] && yq -e '.channel_sources | unique == ["conda-forge"]' "$VARIANT" > /dev/null; then
        CONDA_VARIANTS+=("$VARIANT")
    fi
done
if [ "${#CONDA_VARIANTS[@]}" -ne 1 ]; then
    echo "Expected one stable conda-forge variant for ${CONDA_PLAT}-${ARCH}, found ${#CONDA_VARIANTS[@]}" >&2
    exit 1
fi

rattler-build build --recipe recipe/recipe_patched.yaml  --output-dir packages -m "${CONDA_VARIANTS[0]}"
conda create -y -n test -c ./packages/${CONDA_PLAT}-${ARCH} python libgdal gdal
conda deactivate

conda activate test
gdalinfo --version
conda deactivate
