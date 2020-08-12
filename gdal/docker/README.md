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
