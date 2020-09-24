GDAL Docker images
==================

This directory contains a number of Dockerfile for different configurations.
Each directory contains a `./build.sh` for convenient building of the image.

Note: the mention of the overall licensing terms of the GDAL build is to the
best of our knowledge and not guaranteed. Users should check by themselves.

# Alpine based (3.12)

## Ultra small: `osgeo/gdal:alpine-ultrasmall-latest`

* Image size: < 40 MB
* Raster drivers: VRT, GTiff, HFA, PNG, JPEG, MEM, JP2OpenJPEG, WEB, GPKG
* Vector drivers: Shapefile, MapInfo, VRT, Memory, GeoJSON, GPKG, SQLite
* External libraries enabled: libsqlite3, libproj, libcurl, libjpeg, libpng, libwebp, libzstd
* No GDAL Python
* Base PROJ grid package
* Overall licensing terms of the GDAL build: permissive (X/MIT, BSD style, Apache, etc..)

See [alpine-ultrasmall/Dockerfile](alpine-ultrasmall/Dockerfile)

## Small: `osgeo/gdal:alpine-small-latest`

* Image size: ~ 52 MB
* Raster drivers: ultrasmall + built-in + SQLite-based ones + network-based ones
* Vector drivers: ultrasmall + built-in + most XML-based ones + network-based ones + PostgreSQL
* External libraries enabled: ultrasmall + libexpat, libpq, libssl
* No GDAL Python
* Base PROJ grid package
* Overall licensing terms of the GDAL build: permissive (X/MIT, BSD style, Apache, etc..)

See [alpine-small/Dockerfile](alpine-small/Dockerfile)

## Normal: `osgeo/gdal:alpine-normal-latest`

* Image size: ~ 226 MB
* Raster drivers: small + netCDF, HDF5, BAG
* Vector drivers: small + Spatialite, XLS
* External libraries enabled: small + libgeos, libhdf5, libhdf5, libkea, libnetcdf, libfreexl,
  libspatialite, libxml2, libpoppler, openexr, libheif
* GDAL Python (Python 3.7)
* Base PROJ grid package
* Overall licensing terms of the GDAL build: copy-left (GPL) + LGPL + permissive

See [alpine-normal/Dockerfile](alpine-normal/Dockerfile)

# Ubuntu based (20:04 / focal)

## Small: `osgeo/gdal:ubuntu-small-latest`

* Image size: ~ 270 MB
* Raster drivers: all built-in + JPEG + PNG + JP2OpenJPEG + WEBP +SQLite-based ones + network-based ones
* Vector drivers: all built-in + XML based ones + SQLite-based ones + network-based ones + PostgreSQL
* External libraries enabled: libsqlite3, libproj, libcurl, libjpeg, libpng, libwebp,
  libzstd, libexpat, libxerces-c, libpq, libssl, libgeos
* GDAL Python (Python 3.8)
* Base PROJ grid package
* Overall licensing terms of the GDAL build: LGPL + permissive (X/MIT, BSD style, Apache, etc..)

See [ubuntu-small/Dockerfile](ubuntu-small/Dockerfile)

## Full: `osgeo/gdal:ubuntu-full-latest` (aliased to `osgeo/gdal`)

* Image size: ~ 1.35 GB
* Raster drivers: all based on almost all possible free and open-source dependencies
* Vector drivers: all based on almost all possible free and open-source dependencies
* External libraries enabled: small + libnetcdf, libhdf4, libhdf5, libtiledb, libkea,
  mongocxx 3.4, libspatialite, unixodbc, libxml2, libcfitsio, libmysqlclient,
  libkml, libpoppler, openexr, libheif
* GDAL Python (Python 3.8)
* *All* PROJ grid packages
* Overall licensing terms of the GDAL build: copy-left (GPL) + LGPL + permissive

See [ubuntu-full/Dockerfile](ubuntu-full/Dockerfile)


# Usage

Pull the required image and then run passing the gdal program you want to execute as a [docker run](https://docs.docker.com/engine/reference/commandline/run/) command. Bind a volume from your local file system to the docker container to run gdal programs that accept a file argument. For example, binding `-v /home:/home` on Linux or `-v /Users:/Users` on Mac will allow you to reference files in your home directory by passing their full path. Use the docker `--rm` option to automatically remove the container when the run completes.

## Example:

```shell
docker pull osgeo/gdal:alpine-small-latest
docker run --rm -v /home:/home osgeo/gdal:alpine-small-latest gdalinfo $PWD/my.tif
```

# Images of releases

Tagged images of recent past releases are available. The last ones (at time of writing) are for GDAL 3.1.0 and PROJ 7.0.1:
* osgeo/alpine-ultrasmall-3.1.0
* osgeo/alpine-small-3.1.0
* osgeo/alpine-normal-3.1.0
* osgeo/ubuntu-small-3.1.0
* osgeo/ubuntu-full-3.1.0

## Multi-arch Images

Each directory contains a `build.sh` shell script that supports building images
for multiple platforms using an experimental feature called [Docker BuildKit](https://docs.docker.com/buildx/working-with-buildx/).

BuildKit CLI looks like `docker buildx build` vs. `docker build`
and allows images to build not only for the architecture and operating system
that the user invoking the build happens to run, but for others as well.

There is a small setup process depending on your operating system. Refer to [Preparation toward running Docker on ARM Mac: Building multi-arch images with Docker BuildX](https://medium.com/nttlabs/buildx-multiarch-2c6c2df00ca2).

#### Example Scenario

If you're running Docker for MacOS with an Intel CPU
and you wanted to build the `alpine-small` image with support for Raspberry Pi 4,
adding a couple flags when running `alpine-small/build.sh` can greatly simplify this process

#### Enabling

Use the two script flags in order to leverage BuildKit:

| Flag  | Description | Arguments |
| ------------- | ------------- | ------------- |
| --with-multi-arch  | Will build using the `buildx` plugin  | N/A |
| --platform  | Which architectures to build  | linux/amd64,linux/arm64 |

**Example**

`alpine-small/build.sh --with-multi-arch --release --gdal v3.1.3 --proj master --platform linux/arm64,linux/amd64`

## Custom Image Names

Override the image and repository by setting the environment variable: `BASE_IMAGE_NAME`

**Example**

`BASE_IMAGE_NAME="YOU_DOCKER_USERNAME/gdal" alpine-small/build.sh --release --gdal v3.1.3 --proj master`
