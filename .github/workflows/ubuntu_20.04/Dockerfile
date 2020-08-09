ARG BASE_IMAGE=gdal-deps-ubuntu-20.04

FROM $BASE_IMAGE as builder

COPY . /build
COPY .github/workflows/ubuntu_20.04/build.sh /build

RUN /build/build.sh
