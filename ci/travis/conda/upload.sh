#!/bin/bash

set -e

if [ -z "${ANACONDA_TOKEN+x}" ]
then
    echo "Anaconda token is not set!"
    exit 1
fi

if [ -z "${CONDA_SUBDIR+x}" ]; then
    echo "CONDA_SUBDIR is not set!"
    exit 1
fi

ls
pwd
find .

files=$(find . -name "*gdal*.conda")

if [ -z "$files" ]; then
  echo "No packages matching *gdal*.conda to upload found"
  exit 1
fi

echo "Anaconda token is available, attempting to upload"
conda install -c conda-forge -c defaults python=3.12 anaconda-client jq curl -y --strict-channel-priority

# remove any existing packages for the same version
for f in $files; do
  filename=$(basename "$f")

  # extract package name
  pkg=$(echo "$filename" | sed -E 's/-[0-9].*//')

  # extract version number (e.g. 3.12.99)
  version=$(echo "$filename" | sed -E 's/^.+-([0-9]+\.[0-9]+\.[0-9]+).*/\1/')

  existing_files=$(curl -s https://api.anaconda.org/package/gdal-master/$pkg | jq -r \
    --arg version "$version" \
    --arg subdir "$CONDA_SUBDIR" '
    .files[]
    | select(.version == $version)
    | select(.attrs.subdir == $subdir)
    | .full_name
    ' | tr -d '\r')

  for ef in $existing_files; do
    echo "Removing $ef"
    anaconda -t "$ANACONDA_TOKEN" remove --force "$ef"
  done
done

# upload new packages
for f in $files; do
  filename=$(basename "$f")
  echo "Uploading $filename"
  anaconda -t "$ANACONDA_TOKEN" upload --force --no-progress --user gdal-master "$f"
done
