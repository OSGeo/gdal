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

# Find the config for this platform
# feedstock renames these files when it is re-rendered
# so look for any file starting with the platform name
# (e.g. win_64_.yaml or win_64_channel_sourcesconda-forge.yaml)
VARIANT_FILE=""
for candidate in .ci_support/${CONDA_PLAT}_${ARCH}_*.yaml; do
    # check the file exists (if no files match, $candidate is the literal pattern)
    if [ -f "$candidate" ]; then
        VARIANT_FILE="$candidate"
        break
    fi
done

if [ -z "$VARIANT_FILE" ]; then
    echo "ERROR: no variant config found for ${CONDA_PLAT}_${ARCH}."
    echo "Files available in .ci_support:"
    ls .ci_support
    exit 1
fi

echo "Using variant config: $VARIANT_FILE"

rattler-build build --recipe recipe/recipe_patched.yaml --output-dir packages -m "$VARIANT_FILE"
conda create -y -n test -c ./packages/${CONDA_PLAT}-${ARCH} python libgdal gdal
conda deactivate

conda activate test
gdalinfo --version
conda deactivate
