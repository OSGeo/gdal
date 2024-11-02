#!/bin/sh

set -eu

git config --global --add safe.directory ${GDAL_SOURCE_DIR:=..}

wget -q https://scan.coverity.com/download/cxx/linux64 --post-data "token=$COVERITY_SCAN_TOKEN&project=GDAL" -O cov-analysis-linux64.tar.gz
mkdir /tmp/cov-analysis-linux64
tar xzf cov-analysis-linux64.tar.gz --strip 1 -C /tmp/cov-analysis-linux64

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_PYTHON_BINDINGS=OFF \
    -DBUILD_JAVA_BINDINGS=OFF \
    -DBUILD_CSHARP_BINDINGS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DGDAL_USE_TIFF_INTERNAL=ON \
    -DGDAL_USE_GEOTIFF_INTERNAL=ON \
    -DECW_ROOT=/opt/libecwj2-3.3 \
    -DMRSID_ROOT=/usr/local \
    -DFileGDB_ROOT=/usr/local/FileGDB_API

/tmp/cov-analysis-linux64/bin/cov-build --dir cov-int make "-j$(nproc)"
tar czf cov-int.tgz cov-int

apt-get update -y
apt-get install -y jq

## Below from https://scan.coverity.com/projects/gdal/builds/new

# Initialize a build. Fetch a cloud upload url.
curl -X POST \
  -d version=master \
  -d description="$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD)" \
  -d token=$COVERITY_SCAN_TOKEN \
  -d email=$COVERITY_SCAN_EMAIL \
  -d file_name="cov-int.tgz" \
  https://scan.coverity.com/projects/749/builds/init \
  | tee response

# Store response data to use in later stages.
upload_url=$(jq -r '.url' response)
build_id=$(jq -r '.build_id' response)

# Upload the tarball to the Cloud.
curl -X PUT \
  --header 'Content-Type: application/json' \
  --upload-file cov-int.tgz \
  $upload_url

#  Trigger the build on Scan.
curl -X PUT \
  -d token=$COVERITY_SCAN_TOKEN \
  https://scan.coverity.com/projects/749/builds/$build_id/enqueue
